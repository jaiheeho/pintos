#include <stdio.h>
#include <stdlib.h> // ADDED HEADER
#include <syscall-nr.h>
#include <hash.h> // ADDED HEADER
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h" // ADDED HEADER
#include "threads/vaddr.h" // ADDED HEADER
#include "threads/malloc.h" // ADDED HEADER
#include "userprog/syscall.h"
#include "userprog/process.h" // ADDED HEADER
#include "userprog/pagedir.h" // ADDED HEADER
#include "filesys/file.h" // ADDED HEADER
#include "filesys/filesys.h" // ADDED HEADER
#include "lib/user/syscall.h" // ADDED HEADER
#include "devices/input.h" // ADDED HEADER
#include "vm/page.h"// ADDED HEADER
#include "vm/frame.h"
#include <inttypes.h> // ADDED HEADER


static void syscall_handler (struct intr_frame *);
void get_args(void* esp, int *args, int argsnum);
/************************************************************************
* FUNCTION : syscall_init                                               *
* Input : NONE                                                          *
* Output : NONE                                                         *
* Purporse : register syscall handler interrupt                         *
************************************************************************/
 /*added function */
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/************************************************************************
* FUNCTION : syscall_handler                                            *
* Input : NONE                                                          *
* Output : NONE                                                         *
* Purporse : syscall_handler which is called when interrupt occurs      *
* calls approriate systemm call                                         *
************************************************************************/
 /*added function */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  bool returnZ = false;
  int retval=0;

  uint32_t syscall_num;
  int args[12];
  //check whether esp is vaild
  if (invalid_addr(f->esp))
    exit(-1);
  else
    syscall_num = *((int*)f->esp);

  //check validity of  syscall_num
  if (syscall_num > SYS_INUMBER)
    exit(-1);

  switch(syscall_num)
    {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      get_args(f->esp, args, 1);
      exit(args[0]);
      break;
    case SYS_EXEC:
      get_args(f->esp, args, 1);
      retval=exec((char *)args[0]);
      returnZ=true;
      break;
    case SYS_WAIT:
      get_args(f->esp, args, 1);
      retval=wait(args[0]);
      returnZ=true;
      break;
    case SYS_CREATE:
      get_args(f->esp, args, 2);
      retval=create((char *)args[0],args[1]);
      returnZ=true;
      break;
    case SYS_REMOVE:
      break;
      get_args(f->esp, args, 1);
      retval=remove((char *)args[0]);
      returnZ=true;
    case SYS_OPEN:
      get_args(f->esp, args, 1);
      retval = open((char *)args[0]);
      returnZ=true;
      break;
    case SYS_FILESIZE:
      get_args(f->esp, args, 1);
      retval = filesize(args[0]);
      returnZ=true;
      break;
    case SYS_READ:
      get_args(f->esp, args, 3);
      retval = read(args[0], (void *)args[1], args[2]);
      returnZ=true;
      break;
    case SYS_WRITE:
      get_args(f->esp, args, 3);
      retval = write(args[0], (void *)args[1], args[2]);
      returnZ=true;
      break;
    case SYS_SEEK:
      get_args(f->esp, args, 2);
      seek(args[0], args[1]);
      break;
    case SYS_TELL:
      get_args(f->esp, args, 1);
      retval = tell(args[0]);
      returnZ=true;
      break;
    case SYS_CLOSE:
      get_args(f->esp, args, 1);
      close(args[0]);
      break;    
    case SYS_MMAP:
      get_args(f->esp, args, 2);
      retval=mmap(args[0],(void *)args[1]);
      returnZ=true;
      break;
    case SYS_MUNMAP:
      get_args(f->esp, args, 1);
      munmap(args[0]);
      break;
    }
  // if return value is needed, plug in the return value
  if(returnZ)
      f->eax = retval;
  //thread_exit ();
}

/************************************************************************
* FUNCTION : halt                                                       *
* Input : NONE                                                          *
* Output : NONE                                                         *
* Purporse : system halt                                                *
************************************************************************/
 /*added function */
void
halt (void) 
{
  power_off();
  NOT_REACHED ();
}

/************************************************************************
* FUNCTION : exit                                                       *
* Input : status                                                        *
* Output : NONE                                                         *
* Purporse : exit process with exit exit_status                         *
************************************************************************/
 /*added function */
void
exit(int status)
{
  // exit the thread(thread_exit will call process_exit)
  // print exit message
  printf("%s: exit(%d)\n", thread_name(), status);
  struct thread *curr = thread_current();
  curr->exit_status=status;
  thread_exit();
  NOT_REACHED ();
  // return exit status to kernel
}

/************************************************************************
* FUNCTION : exec                                                       *
* Input : cmd_line                                                      *
* Output : NONE                                                         *
* Purporse : execute new process                                        *
************************************************************************/
 /*added function */
pid_t
exec (const char *cmd_line)
{
  pid_t process_id;
  if(invalid_addr((void*)cmd_line))
    exit(-1);
  process_id =  process_execute(cmd_line);
  return process_id;
}

/************************************************************************
* FUNCTION : wait                                                       *
* Input : pid_t                                                         *
* Output : child(=pid) 's  exit status                                  *
* Purporse : wait for the child process with pid to exit and return     *
* child's exit status                                                   *
************************************************************************/
 /*added function */
int 
wait(int pid){
  return process_wait(pid);;
}

/************************************************************************
* FUNCTION : create                                                     *
* Input : file, initial_size                                            *
* Output : true of false                                                *
* Purporse : create new file with file name and initial size            *
************************************************************************/
 /*added function */
bool 
create (const char *file, unsigned initial_size){
  bool success;
  if(invalid_addr((void*)file))
    exit(-1);
  sema_down(&filesys_global_lock);
  success = filesys_create(file, initial_size);
  sema_up(&filesys_global_lock);
  return success;
}

/************************************************************************
* FUNCTION : remove                                                     *
* Input : file                                                          *
* Output : true of false                                                *
* Purporse : remove file with file name                                 *
************************************************************************/
 /*added function */
bool
remove (const char *file)
{
  bool success;
  sema_down(&filesys_global_lock);
  success = filesys_remove(file);
  sema_up(&filesys_global_lock);
  return success;
}

/************************************************************************
* FUNCTION : open                                                       *
* Input : file                                                          *
* Output : fd number                                                    *
* Purporse : Open file with file name by using filesys_open             *
* and return file descriptor                                            *
************************************************************************/
 /*added function */
int 
open(const char *file)
{

  struct file *filestruct;
  struct thread *curr = thread_current(); 
  if(invalid_addr((void*)file))
    exit(-1);
  sema_down(&filesys_global_lock);
  //open file with name (file)
  filestruct = filesys_open(file);
  //check whether open was successful
  if (filestruct == NULL)
  {
    sema_up(&filesys_global_lock);
    return -1;
  }
  //allocate memory
  struct file_descriptor *new_fd;
  new_fd = (struct file_descriptor *)malloc (sizeof (struct file_descriptor));
  if (!new_fd)
    return -1;

  //initialize new_fd
  new_fd->file = filestruct;
  new_fd->fd =  curr->fd_given ++;
  list_push_back(&curr->file_descriptor_table, &new_fd->elem);
  sema_up(&filesys_global_lock);
  return new_fd->fd;
}

/************************************************************************
* FUNCTION : filesize                                                   *
* Input : file desciptor number                                         *
* Output : size of file                                                 *
* Purporse : return filesize                                            *
************************************************************************/
 /*added function */
int filesize(int fd)
{
  struct file *file = get_struct_file(fd);
  int size;
  if (!file)
    return -1;
  sema_down(&filesys_global_lock);
  size = file_length(file);
  sema_up(&filesys_global_lock);
  return size;
}

/************************************************************************
* FUNCTION : read                                                       *
* Input : fild desciptor number, buffer, length to read                 *
* Output : read bytes actuall read                                      *
* Purporse : read bytes from fd and write it to buffer                  *
************************************************************************/
 /*added function */
int read (int fd, void *buffer, unsigned length)
{
  uint32_t i;
  uint8_t* buf_char = (uint8_t *) buffer;
  int retval;
  if(fd == 0)
  {
    //std out 
    for(i = 0; i<length; i++)
    {
      *(buf_char + i) = input_getc();
    } 
    retval = length;
  }
  else if(fd == 1)
  {
    //error
    retval = -1;
  }
  else
  {
    sema_down(&filesys_global_lock);
    struct file *file = get_struct_file(fd);
    if (!file)
    {
      sema_up(&filesys_global_lock);
      return -1;  
    }

    retval = file_read(file, buffer, length);
    sema_up(&filesys_global_lock);
  }
  return retval;
}

/************************************************************************
* FUNCTION : write                                                      *
* Input : fild desciptor number, buffer, length to read                 *
* Output : actually written bytes                                       *
* Purporse : write buffer to file with fd                               *
************************************************************************/
 /*added function */
int write(int fd, const void *buffer, unsigned length)
{
  int retval;
  uint8_t* buf_char = (uint8_t *) buffer; 
  if(fd <= 0)
    {
      //error
      retval = -1;
    }
  else if(fd == 1)
    {
      //std in
      putbuf(buf_char, length);
      retval = length;
    }
  else
    {
      sema_down(&filesys_global_lock);
      struct file *file = get_struct_file(fd);
      //if fd is bad 
      if (!file)
      {
        sema_up(&filesys_global_lock);
        return -1;
      }
      retval = file_write(file, buffer, length);
      sema_up(&filesys_global_lock);
    }

  return retval;
}

/************************************************************************
* FUNCTION : seek                                                       *
* Input : file desciptor number                                         *
* Output : NONE                                                         *
* Purporse : change next read byte of file                              *
************************************************************************/
 /*added function */
void seek (int fd, unsigned position)
{
  struct file *file = get_struct_file(fd);
  if (!file)
    return;
  sema_down(&filesys_global_lock);
  file_seek(file, position);
  sema_up(&filesys_global_lock);
}

/************************************************************************
* FUNCTION : tell                                                       *
* Input : file desciptor number                                         *
* Output : off-set                                                      *
* Purporse : return off-set of file                                     *
************************************************************************/
 /*added function */
unsigned tell (int fd)
{
  struct file *file = get_struct_file(fd);
  int off;
  if (!file)
    return -1;
  sema_down(&filesys_global_lock);
  off = file_tell(file);
  sema_up(&filesys_global_lock);
  return off;
}

/************************************************************************
* FUNCTION : close                                                      *
* Input : file desciptor number                                         *
* Output : NONE                                                         *
* Purporse : close file with fd number                                  *
************************************************************************/
 /*added function */
void close (int fd)
{
  struct file_descriptor *fdt;
  fdt = get_struct_fd_struct(fd);
  if (!fdt)
    return;
  sema_down(&filesys_global_lock);
  list_remove(&fdt->elem);
  file_close(fdt->file);
  free(fdt);
  sema_up(&filesys_global_lock);
}

mapid_t 
mmap (int fd, void *addr)
{

  if((!is_user_vaddr(addr)) || (pg_ofs(addr) != 0) || (fd < 2) || addr < 0x08048000)
    {
      return MAP_FAILED;
    }

  sema_down(&filesys_global_lock);
  struct file_descriptor *fdt;
  fdt = get_struct_fd_struct(fd);
  if(fdt == NULL)
    {
      sema_up(&filesys_global_lock);
      return MAP_FAILED;
    }

  struct file *file_to_mmap = file_reopen(fdt->file);
  if (!file_to_mmap)
  {
    sema_up(&filesys_global_lock);
    return MAP_FAILED;  
  }
  int size = file_length(file_to_mmap);

  if (size == 0)
  {
    file_close(file_to_mmap);
    sema_up(&filesys_global_lock);
    return MAP_FAILED;  
  }



  // no need to hold the lock
  sema_up(&filesys_global_lock);



  //allocate memory
  struct mmap_descriptor *new_mmap;
  new_mmap = (struct mmap_descriptor *)malloc (sizeof (struct mmap_descriptor));
  if (!new_mmap)
  {
    file_close(file_to_mmap);
    return MAP_FAILED;
  }


  //initialize new_mmap
  struct thread * curr = thread_current();  
  new_mmap->start_addr = addr;
  new_mmap->mmap_id =  curr->mmap_id_given ++;
  new_mmap->size = size;
  new_mmap->last_page = addr;
  new_mmap->file = file_to_mmap;

  list_push_back(&curr->mmap_table, &new_mmap->elem);


  // check if mmap pages can fit in the addrspace

  void* temp;

  for(temp = addr; temp <= pg_round_down(addr + size); temp += PGSIZE)
    {
      struct hash_elem *e = found_hash_elem_from_spt(temp);
      
      if(e != NULL)
	{
	  struct spte* spte_target = hash_entry(e, struct spte, elem);
	  
	  if(0)//spte_target->type != BLANK)
	    {
	      // this addr already in use by code/mmap/stack etc.
	      file_close(file_to_mmap);
	      return MAP_FAILED;
	    }
	}
    }
  
  

  off_t read_bytes = size;
  off_t ofs = 0;
  bool writable = true;
  void* upage = addr;

  while (read_bytes > 0) 
    {
      /* Do calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
 


      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      

      if(!load_page_file_lazy(upage, file_to_mmap, ofs, page_read_bytes,
			      page_zero_bytes, writable))
    	{
    	  printf("load_segment: load_page_file failed\n");
	  munmap(new_mmap->mmap_id);
	  file_close(file_to_mmap);

	  return MAP_FAILED;
    	}
      /* Advance. */
      new_mmap->last_page = pg_round_down(upage);
      read_bytes -= page_read_bytes;
      upage += PGSIZE;
      ofs += page_read_bytes;


    }

  return new_mmap->mmap_id;
}



void 
munmap (mapid_t mmap_id)
{
  struct mmap_descriptor *m;
  m = get_mmap_descriptor(mmap_id);
  if (!m)
    return;



  struct hash *spt = &thread_current()->spt;
  void* temp;

  for(temp = m->start_addr; temp <= m->last_page ; temp += PGSIZE)
    {
      
      struct hash_elem *e = found_hash_elem_from_spt(temp);
      
      if(e == NULL)
	{
	  // wtf? it cannot be!!
	}
      
      struct spte* target = hash_entry(e, struct spte, elem);
      
      /*if(target->type != MMAP)
	{
	  // it cant be!!!
	}
      */
      //lock the frame
      target->frame_locked = true;


      if(target->present == true)
	{
	  if(pagedir_is_dirty(thread_current()->pagedir, target->user_addr))
	    {
	      sema_down(&filesys_global_lock);
	      file_write_at(m->file, target->user_addr,
			    target->loading_info.page_read_bytes,
			    target->loading_info.ofs);
	      sema_up(&filesys_global_lock);
	    }

	      // must free the frame
	      frame_free(target->fte);
	      pagedir_clear_page(thread_current()->pagedir, target->user_addr);
	}
      
      
      
      
      
      hash_delete(spt, e);

    }

  sema_down(&filesys_global_lock);
  file_close(m->file);
  sema_up(&filesys_global_lock);


  list_remove(&m->elem);
  //need to implement clear_page 
  free(m);
  /*clear_page*/

}

struct mmap_descriptor* get_mmap_descriptor(int mmap_id)
{
  struct thread* curr = thread_current();
  struct list *mmap_table = &curr->mmap_table;
  struct list_elem *iter_md;
  struct mmap_descriptor *m;

  for(iter_md = list_begin(mmap_table); iter_md != list_tail(mmap_table);
      iter_md = list_next(iter_md))
  {
    m = list_entry(iter_md, struct mmap_descriptor, elem);
    if(mmap_id == m->mmap_id)
    {
      return m;
    }
  }
  return NULL;
}

/************************************************************************
* FUNCTION : get_struct_file                                            *
* Input : file desciptor number                                         *
* Output : file pointer                                                 *
* Purporse : from file descriptor number extract file pointer           *
in file_desriptor adt                                                   *
************************************************************************/
 /*added function */
struct file* get_struct_file(int fd)
{
  struct thread* curr = thread_current();
  struct list *fdt = &curr->file_descriptor_table;
  struct list_elem *iter_fd;
  struct file_descriptor *f;

  for(iter_fd = list_begin(fdt); iter_fd != list_tail(fdt);
      iter_fd = list_next(iter_fd))
    {
      f = list_entry(iter_fd, struct file_descriptor, elem);
      if(fd == f->fd)
	{
	  return f->file;
	}
    }
  return NULL;
}

/************************************************************************
* FUNCTION : get_struct_fd_struct                                       *
* Input : file desciptor number                                         *
* Output : file_descriptor pointer                                      *
* Purporse : from file descriptor number extract file_desriptor adt     *
************************************************************************/
 /*added function */
struct file_descriptor* 
get_struct_fd_struct(int fd)
{
  struct thread* curr = thread_current();
  struct list *fdt = &curr->file_descriptor_table;
  struct list_elem *iter_fd;
  struct file_descriptor *f;

  for(iter_fd = list_begin(fdt); iter_fd != list_tail(fdt);
      iter_fd = list_next(iter_fd))
    {
      f = list_entry(iter_fd, struct file_descriptor, elem);
      if(fd == f->fd)
  {
    return f;
  }
    }
  return NULL;
}

/************************************************************************
* FUNCTION : get_args                                                   *
* Input : oid* esp, int *args, int argsnum                              *
* Output : NONE                                                         *
* Purporse : from esp, extract argument to syscall                      *
with agrsnum arguments                                                  *
************************************************************************/
 /*added function */
void get_args(void* esp, int *args, int argsnum)
{
  int i;
  int* esp_copy = (int*)esp;

  //in order to check whether address of arguments exceed PHYS_BASE [hint from : sc_bad_arg test]
  if ((void*)(esp_copy + argsnum) >= PHYS_BASE)
    exit(-1);

  for(i=0; i<argsnum; i++)
    {
      esp_copy += 1;
      //check if each argument is vaild
      if (invalid_addr(esp_copy))
        exit(-1);
      args[i] = *esp_copy;
    }
}

bool invalid_addr(void* addr){
  if (!is_user_vaddr(addr))
    return true;
  //under CODE segment
  if (addr <(void*)0x08048000)
    return true;
  if (addr == NULL)
    return true;
  //Not within pagedir
  return false;
}
