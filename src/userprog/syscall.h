#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

void exit(int status);
int write(int fd, const void *buffer, unsigned size);
int open(const char *file);
int filesize(int fd);
int wait(pid_t);

struct file* get_struct_file(int fd);


#endif /* userprog/syscall.h */
