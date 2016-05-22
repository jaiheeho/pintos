#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <list.h> // ADDED HEADER
#include "threads/malloc.h" // ADDED HEADER
#include "lib/user/syscall.h" // ADDED HEADER
#include "userprog/syscall.h"

//for proj3 
#include "vm/frame.h"
#include "vm/swap.h"


static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/************************************************************************
* FUNCTION : process_execute                                            *
* Input : file_name                                                     *
* Output : new process id if successful                                 *
************************************************************************/
 /*fixed function */
/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;
  //printf("process_execute :  %s to execute : %s\n", thread_current()->name, file_name);

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  /***** ADDED CODE *****/
  char file_name_temp[128];
  int i;
  for(i=0; !((fn_copy[i] == '\0')|| (fn_copy[i] == ' ')); i++);
  if(i != 0)
    {
      strlcpy(file_name_temp, fn_copy, i+1);
    }
  /***** END OF ADDED CODE *****/

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name_temp, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 

  /***** ADDED CODE *****/
  //Loading elf was not successful return tid -1
  //To make chile it should be check by parent process
  /*not yet called load from chile*/
  struct thread *curr = thread_current ();
  struct list_elem *iter_child;
  struct list *child_list = &curr->child_procs;
  struct thread *child=NULL;
  struct thread *c=NULL;

  //Find a newly made child process from me(parent of child)
  for(iter_child = list_begin(child_list); iter_child != list_tail(child_list); 
    iter_child = list_next(iter_child))
  {
    child = list_entry(iter_child, struct thread, child_elem);
    if (child->tid == tid)
    {
      c = child;
      break;
    }  
  }
  /* should not happend in pintos but for safety*/
  if (!c)
    return TID_ERROR;

  /* if loading of child with tid did no end yet, wait for it*/
  if (c->is_loaded == 2)
  {
    sema_try_down(&c->loading_safer);
    sema_down(&c->loading_safer);
  }
  /* if loading of child failed, return TIE_ERROR*/
  if (c->is_loaded == 0)
  {
    return TID_ERROR;
  }
  /***** END OF ADDED CODE *****/

  return tid;
}


/************************************************************************
* FUNCTION : start_process                                              *
* Input : f_name                                                        *
* Output : None                                                         *
************************************************************************/
/*fixed function */
/* A thread function that loads a user process and makes it start
   running. */
static void
start_process (void *f_name)
{
  char *file_name = f_name;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  /***** ADDED CODE *****/
  //supplemental page table  for proj3 
  struct thread * curr = thread_current();
  sup_page_table_init(&curr->spt);

  //deny write to executable 
  //executable of thread is saved in struct thread
  curr->executable = filesys_open(file_name);
  file_deny_write(curr->executable);

  //addeed filesys_lock
  sema_down(&filesys_global_lock);
  success = load (file_name, &if_.eip, &if_.esp);
  sema_up(&filesys_global_lock);
  /***** END OF ADDED CODE *****/

  /* If load failed, quit. */
  /***** ADDED CODE *****/
  //Palloc_free_page has to be done to free memory for proc name.
  if (!success) {
    palloc_free_page (file_name);
    curr->is_loaded = 0;
    //if loading was unsuccessful remove thread from parent's child list and exit();
    list_remove(&curr->child_elem);
    /*for the loading safer (incase of parent waiting for child loading end*/
    sema_try_down(&curr->loading_safer);
    sema_up(&curr->loading_safer);
    thread_exit ();
  }

  // initialize file descriptor table and is_process flag (because this is process)
  list_init(&curr->file_descriptor_table);
  curr->is_process = true;
  // fd starts from 2 since 0 and 1 is allocated for stdin & stdout.
  curr->fd_given = 2;
  curr->is_loaded = 1;

  /* mmap_id recording*/
  curr->mmap_id_given = 1;
  list_init(&curr->mmap_table);

  palloc_free_page (file_name);
  /*for the loading safer (incase of parent waiting for child loading end*/
  sema_try_down(&curr->loading_safer);
  sema_up(&curr->loading_safer);
  //printf("READY TO LAUNCH PROG\n");
  /***** END OF ADDED CODE *****/

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}


/************************************************************************
* FUNCTION : process_wait                                               *
* Input : child_tid                                                     *
* Output : exit_status of child                                         *
************************************************************************/
/*fixed function */

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
  /***** ADDED CODE *****/
  struct thread *curr = thread_current ();
  struct list_elem *iter_child;
  struct list *child_list = &curr->child_procs;
  struct thread *child=NULL;
  struct thread *c=NULL;
  int retval;

  //Check whether child of the calling process, -> not child process return -1
  for(iter_child = list_begin(child_list);
    iter_child != list_tail(child_list); iter_child = list_next(iter_child))
  {
    child = list_entry(iter_child, struct thread, child_elem);
    if (child->tid == child_tid)
    {
      c = child;
      break;
    }
  }

  //Check whether TID is invalid, invalid -> return -1
  if (c == NULL || c->status == THREAD_DYING)
    return -1;
  //process_wait() has already been successfully called for the given TID, return -1
  if (c->is_wait_called)
    return -1;

  //Wait for tid to be exited
  //At first, try_sema_down to acquire semaphor for child's wait
  //if semaphore is acquired, wait for child
  //else, child has already exited but it is waiting in case of late wait call. 
  // (=> release semaphore for child and make it exited completely)
  if(sema_try_down(&c->sema_wait))
  {
    c->is_wait_called = true;
    //this is waiting call sema-down again
    sema_down(&c->sema_wait);
    //when c is exited and return value
    retval = c->exit_status;
  }
  else
  {
    //late wait calls, so get exit_status of child and releas child.
    retval = c->exit_status;
    sema_up(&c->sema_wait);
  }
  return retval;
  /***** END OF ADDED CODE *****/
}


/************************************************************************
* FUNCTION : process_exit                                               *
* Input : None                                                          *
* Output : None                                                         *
************************************************************************/
/*fixed function */

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *curr = thread_current ();
  uint32_t *pd;
  struct list *child_list = &curr->child_procs;
  struct thread *c;
  struct list_elem *iter_child;

  /***** ADDED CODE *****/
  for(iter_child = list_begin(child_list);
    iter_child != list_tail(child_list); iter_child = list_next(iter_child))
  {
    //first for every child in this thread's children list, make their parent pointer to NULL
    c = list_entry(iter_child, struct thread, child_elem);
    c->parent_proc = NULL;
    //if exit was called in child process but child is blocked 
    // because of parent's wait call, then release lock of child and make it dead completely.
    // to make it simple first call sema_try_down and call sema_up
    sema_try_down(&c->sema_wait);
    sema_up(&c->sema_wait);
  }

  //allow write to executable by closing it in write deny part of proj2
  sema_down(&filesys_global_lock);
  if (curr->executable){
    file_close(curr->executable);
  }
  struct list *fdt = &curr->file_descriptor_table;
  struct list_elem *iter_fd;
  struct file_descriptor *f;
  //empty file_descriptor table for the process which was malloced when files were opened.
  while (!list_empty (fdt) && curr->is_loaded == 1)
  {
    iter_fd = list_pop_front (fdt);
    f = list_entry(iter_fd, struct file_descriptor, elem);
    file_close(f->file);
    free(f);  
  }
  //empty mmap_table  for the process which was malloced when mmaped.
  struct list *mmap_table = &curr->mmap_table;
  struct list_elem *iter_md;
  struct mmap_descriptor *m;  
  while (!list_empty (mmap_table) && curr->is_loaded == 1)
  {
    iter_md = list_pop_front (mmap_table);
    m = list_entry(iter_md, struct mmap_descriptor, elem);
    free(m);  
  }

  sema_up(&filesys_global_lock);


  /***** ADDED CODE *****/
  //Finally, wake up parent who waiting for this thread*/
  if (curr->is_wait_called){
    sema_up(&curr->sema_wait);
  }
  else {
    //If, parent didn't call wait() for this process yet, wait for parent until parent calls exit() itself or calls wait()
    if (curr->parent_proc != NULL){
      sema_down(&curr->sema_wait);
      sema_down(&curr->sema_wait);
    }
  }
  //Disconncect with its parent (i.e remove itself from children list of parent)
  if (curr->parent_proc != NULL)
    list_remove (&curr->child_elem);

  //finally free supplement page table for this process.
  sup_page_table_free(&curr->spt);


  /***** END OF ADDED CODE *****/

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = curr->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      curr->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }

}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, char *file_name, char **strtok_r_ptr);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /*ADDED CODE*/
  char *strtok_r_ptr;
  char *token_ptr;

  token_ptr = strtok_r((char*)file_name, " ", &strtok_r_ptr);
  //printf("load: init\n");
  /*END OF ADDED CODE*/

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;
      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;

      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }
  //printf("load done.\n");
  /* Set up stack. */
  if (!setup_stack (esp, (char*)file_name, &strtok_r_ptr))
    {printf("Stack setup aint right\n");
      goto done;}
  //printf("stack setup done\n");
  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;
  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);
  //printf("ofs=%d upage=%p, read_bytes=%d, zero_bytes=%d\n", ofs, upage, read_bytes, zero_bytes);
  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Do calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /*********** MODIFIED CODE - PROJ3-2****************/
      //printf("read_bytes=%d zero_bytes=%d\n", read_bytes, zero_bytes);
      if(!load_page_file(upage, file, ofs, page_read_bytes,
			 page_zero_bytes, writable))
	{
	  printf("load_segment: load_page_file failed\n");
	  return false;
	}
      
      /* Get a page of memory. */
      /* uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
      	return false;
      */
      // printf("LUL\n");
      /* Load this page. */
      /*    if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
	  
          //palloc_free_page (kpage);
          return false; 
        }
      printf("LUL2\n");
      memset (kpage + page_read_bytes, 0, page_zero_bytes);
*/
      /* Add the page to the process's address space. */
      /* if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }
      */
      /*************** MODIFIED CODE END *******************/



      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/************************************************************************
* FUNCTION : setup_stack                                                *
* Input : void **esp, char *file_name, char **strtok_r_ptr              *
* Output : true of false                                                *
************************************************************************/
/*fixed function */
/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, char *file_name, char **strtok_r_ptr) 
{
  uint8_t *kpage;
  bool success = false;

  /*
  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  printf("kpage = %0x\n", kpage);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }


  */

  /*ADDED CODE PROJ3-1*/
  success = load_page_new(((uint8_t *)PHYS_BASE) - PGSIZE, true);
  if(success)
    {
      *esp = PHYS_BASE;
      
    }
  else PANIC("STACK SETUP PANIC\n");
  /*ADDED END*/



  /*ADDED CODE*/
  char *token_ptr;
  int argc = 0;
  char* argv[128];
  uint32_t esp_limit = (uint32_t)*esp;

  //insert file name argv[0]
  *esp -= strlen(file_name) + 1;
  strlcpy(*esp, file_name, strlen(file_name)+1);
  argv[argc] = *esp;
  argc++;  

  //insert argv strings

  for(token_ptr = strtok_r(NULL, " ", strtok_r_ptr);
      token_ptr != NULL; token_ptr = strtok_r(NULL, " ", strtok_r_ptr))
    {
      //printf("Tokens: %s\n", token_ptr);
      *esp -= strlen(token_ptr) + 1;
      strlcpy(*esp, token_ptr, strlen(token_ptr)+1);
      argv[argc] = *esp;
      argc++;
    }


  // Insert padding
  if((PHYS_BASE - *esp)%4)
    { 
      *esp -= 4 - (PHYS_BASE - *esp)%4;
    }
  // Insert NULL
  argv[argc] = NULL;
  argc++;
  
  //insert argv pointer array
  int i = argc - 1;
  for(; i >= 0; i--)
    {
      *esp -= sizeof(char *);
      *((char**)(*esp)) = argv[i];
    }

  *esp -= sizeof(char**);
  *((char***)(*esp)) = *esp + sizeof(char**);

  //insert argc
  argc--;
  *esp -= sizeof(int);
  memcpy(*esp, &argc, sizeof(int));

  //insert dummy return address
  *esp -= sizeof(void*);
  *(int*)(*esp) = 0;

  //check if stack is safe//
  if (((uint32_t)*esp-esp_limit) < PGSIZE)
    success = false;

  //printf("ESP : %x\n", *esp);
  int a = 0;
  for(a=0; 4*a < (PHYS_BASE - *esp); a++){
    //printf("addr: %x  |  content: 0x%08x\n", (int*)(*esp) + a ,*((int*)(*esp) + a));
  }
  /*END OF ADDED CODE*/

  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
