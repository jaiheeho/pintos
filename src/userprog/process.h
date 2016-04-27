#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/***** ADDED STUFF *****/
/* ADT for file_descripto which contains file pointer and fd number */
struct file_descriptor
{
  struct file *file;
  int fd;
  struct list_elem elem;
};

// //for global file locking, initialized to 1 sema_init(&filesys_global_lock , 1) at thread.c
// extern struct semaphore filesys_global_lock;

/***** END of ADDED STUFF *****/
#endif /* userprog/process.h */
