#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#ifdef USERPROG
#include "lib/user/syscall.h" // ADDED HEADER
#endif

/***** ADDED STUFF *****/
/* ADT for file_descripto which contains file pointer and fd number */
struct mmap_descriptor
{
  int mmap_id;
  void* start_addr;
  int size;
  void* last_page;
  struct file* file;
  struct list_elem elem;
};

void syscall_init (void);
void halt (void);
void exit (int status);
pid_t exec (const char *cmd_line);
int wait(pid_t);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open(const char *file);
int filesize(int fd);
int read (int fd, void *buffer, unsigned length);
int write(int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

/* Project 3 and optionally project 4. */
mapid_t mmap (int fd, void *addr);
void munmap (mapid_t);


/*proj 2*/
struct file* get_struct_file(int fd);
struct file_descriptor* get_struct_fd_struct(int fd);
bool invalid_addr(void *);
void * get_kernel_addr(void*);
/*proj 3*/
struct mmap_descriptor* get_mmap_descriptor(int mmap_id);


#endif /* userprog/syscall.h */
