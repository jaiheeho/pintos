#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h" // ADDED HEADER
#include "userprog/process.h" // ADDED HEADER
#include "filesys/file.h" // ADDED HEADER
#include "filesys/filesys.h" // ADDED HEADER
#include "lib/user/syscall.h" // ADDED HEADER
#include "threads/vaddr.h" // ADDED HEADER
#include <stdlib.h> // ADDED HEADER
static void syscall_handler (struct intr_frame *);
void get_args(void* esp, int *args, int argsnum);

/************************************************************************
* FUNCTION : syscall_init                                               *
* Input : NONE                                                          *
* Output : NONE                                                         *
* Purporse : update priority values of all threads                      *
* when function is Called                                               *
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
* Purporse : update priority values of all threads                      *
* when function is Called                                               *
************************************************************************/
 /*added function */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  bool returnZ = false;
  int retval;

  uint32_t syscall_num;
  int args[12];
  //check whether address is vaild
  //printf("f->esp : %d\n",  f->esp);
  if (invalid_addr(f->esp))
    exit(-1);
  else
    syscall_num = *((int*)f->esp);

  //check validity of  syscall_num
  if (syscall_num > SYS_INUMBER)
    exit(-1);

  //printf("THREAD <%s> CALLED SYSCALL NUMBER : %d\n", thread_name(), *((int*)f->esp));
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
      retval=exec(args[0]);
      returnZ=true;
      break;
    case SYS_WAIT:
      get_args(f->esp, args, 1);
      retval=wait(args[0]);
      returnZ=true;
      break;
    case SYS_CREATE:
      get_args(f->esp, args, 2);
      retval=create(args[0],args[1]);
      returnZ=true;
      break;
    case SYS_REMOVE:
      break;
      get_args(f->esp, args, 1);
      retval=remove(args[0]);
      returnZ=true;
    case SYS_OPEN:
      get_args(f->esp, args, 1);
      retval = open(args[0]);
      returnZ=true;
      break;
    case SYS_FILESIZE:
      get_args(f->esp, args, 1);
      retval = filesize(args[0]);
      returnZ=true;
      break;
    case SYS_READ:
      get_args(f->esp, args, 3);
      returnZ=true;
      break;
    case SYS_WRITE:
      //printf("SYS_WRITE\n");
      get_args(f->esp, args, 3);
      retval = write(args[0], args[1], args[2]);
      returnZ=true;
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
  pid_t process_id =  process_execute(cmd_line);
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
  if (file == NULL)
    exit(-1);
  success = filesys_create(file, initial_size);
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
  success = filesys_remove(file);
  return success;
}

/************************************************************************
* FUNCTION : open                                                       *
* Input : NONE                                                          *
* Output : NONE                                                         *
* Purporse : update priority values of all threads                      *
* when function is Called                                               *
************************************************************************/
 /*added function */
int 
open(const char *file)
{
  return;
  struct file *filestruct = filesys_open(file);
}

/************************************************************************
* FUNCTION : filesize                                                   *
* Input : NONE                                                          *
* Output : NONE                                                         *
* Purporse : update priority values of all threads                      *
* when function is Called                                               *
************************************************************************/
 /*added function */
int filesize(int fd)
{
  struct file *file = get_struct_file(fd);
  return file_length(file);
}
/************************************************************************
* FUNCTION : write                                                      *
* Input : NONE                                                          *
* Output : NONE                                                         *
* Purporse : update priority values of all threads                      *
* when function is Called                                               *
************************************************************************/
 /*added function */
int write(int fd, const void *buffer, unsigned size)
{
  //printf("fd: %d, buf: %s, size: %d\n", fd, buffer, size);
  if(fd == 0)
    {
      //error
      return -1;
    }
  else if(fd == 1)
    {
      putbuf(buffer, size);
      return size;
    }
  else
    {
      struct file *file = get_struct_file(fd);
      return file_write(file, buffer, size);
    }
}

/************************************************************************
* FUNCTION : get_struct_file                                            *
* Input : NONE                                                          *
* Output : NONE                                                         *
* Purporse : update priority values of all threads                      *
* when function is Called                                               *
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
* FUNCTION : get_args                                                   *
* Input : NONE                                                          *
* Output : NONE                                                         *
* Purporse : update priority values of all threads                      *
* when function is Called                                               *
************************************************************************/
 /*added function */
void get_args(void* esp, int *args, int argsnum)
{
  int i;
  int* esp_copy = (int*)esp;

  //in order to check whether address of arguments exceed PHYS_BASE [hint from : sc_bad_arg test]
  if (esp_copy + argsnum  >= PHYS_BASE)
    exit(-1);

  for(i=0; i<argsnum; i++)
    {
      esp_copy += 1;
      if (!invalid_addr(esp_copy))
        exit(-1);
      args[i] = *esp_copy;
    }
}
/************************************************************************
* FUNCTION : invalid_addr                                               *
* Input : addr                                                          *
* Output : true of false                                                *
* Purporse : check wheter given address is valid or not                 *
************************************************************************/
 /*added function */
bool invalid_addr(void* addr){
  //check whether it is user addr
  if (!is_user_vaddr(addr))
    return true;
  //under CODE segment
  if (addr <(void*)0x08048000)
    return true;

  // if (!pagedir_get_page (thread_current()->pagedir, addr))
  //    return true;

  return false;
}
