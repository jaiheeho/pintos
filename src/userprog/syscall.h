#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <syscall.h> // ADDED HEADER

void syscall_init (void);

void exit(int);
int wait(pid_t);
int write(int, const void *, unsigned);
int open(const char *file);
int filesize(int fd);

struct file* get_struct_file(int fd);


#endif /* userprog/syscall.h */
