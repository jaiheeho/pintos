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

int write(int, const void *, unsigned);
int open(const char *file);
int filesize(int fd);

struct file* get_struct_file(int fd);
bool invalid_addr(void *);


// void halt (void) NO_RETURN;
// void exit (int status) NO_RETURN;
// pid_t exec (const char *file);
// int wait (pid_t);
// bool create (const char *file, unsigned initial_size);
// bool remove (const char *file);
// int open (const char *file);
// int filesize (int fd);
// int read (int fd, void *buffer, unsigned length);
// int write (int fd, const void *buffer, unsigned length);
// void seek (int fd, unsigned position);
// unsigned tell (int fd);
// void close (int fd);

#endif /* userprog/syscall.h */
