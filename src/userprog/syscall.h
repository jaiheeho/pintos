#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#ifdef USERPROG
#include "lib/user/syscall.h" // ADDED HEADER
#endif

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

struct file* get_struct_file(int fd);
struct file_descriptor* get_struct_fd_struct(int fd);
bool invalid_addr(void *);
void * get_kernel_addr(void*);

#endif /* userprog/syscall.h */
