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
#include <stdlib.h>
static void syscall_handler (struct intr_frame *);
void get_args(void* esp, int *args, int argsnum);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  bool returnZ = false;
  int retval;

  int syscall_num;
  int args[12];
  //check whether address is vaild
  //printf("f->esp : %d\n",  f->esp);
  if (invalid_addr(f->esp))
    exit(-1);
  else
    syscall_num = *((int*)f->esp);

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

void
halt (void) 
{
  power_off();
  NOT_REACHED ();
}

void
exit(int status)
{
  // exit the thread(thread_exit will call process_exit)
  // print exit message
  printf("%s: exit(%d)\n", thread_name(), status);
  struct thread *curr = thread_current();
  curr->exit_status=status;
  thread_exit();
  // return exit status to kernel
}

pid_t
exec (const char *cmd_line)
{
  pid_t process_id =  process_execute(cmd_line);
  printf("exec current :%s, %d\n",thread_name(), thread_current()->tid);
  return process_id;
}

int 
wait(int pid){
  //printf("syscall wait : THREAD <%s> pid : %d\n", thread_name(), pid);
  int retval;
  retval = process_wait(pid);
  return retval;
}

bool 
create (const char *file, unsigned initial_size){
  bool success;
  success = filesys_create(file, initial_size);
  return success;
}

bool
remove (const char *file)
{
  bool success;
  success = filesys_remove(file);
  return success;
}

int open(const char *file)
{
  return;
}

int filesize(int fd)
{
  struct file *file = get_struct_file(fd);
  return file_length(file);
}

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
      args[i] = *esp_copy;
    }
}

bool invalid_addr(void* addr){
  if (addr > PHYS_BASE)
    return true;

  if (addr <=(void*)4000000)
    return true;
  
  return false;
}
