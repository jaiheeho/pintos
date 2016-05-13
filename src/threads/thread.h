#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <hash.h>
#include <stdint.h>
#include "threads/synch.h"
/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    /* IMPELMENTATION */
    ///WHERE WE ADDED/////////
    int64_t wakeup_time;                /* wakeup time in ticks*/
    int recent_cpu;                     /* recent_cpu */
    int nice;                           /* nice */
    int priority_rollback;              /* prioty value to be rolled back */
    struct list lock_holdings;

    //For Project 2
    struct list file_descriptor_table;  /* save the list_of_file_descriptor */
    struct list child_procs;            /* list of child processes */
    struct thread *parent_proc;         /* parent processes */
    struct list_elem child_elem;        /* child_elem used in parent's child list */
    bool is_wait_called;                /* check whether wait function is called for thread */
    struct semaphore sema_wait;         /* semaphore that blocks parent proceess who wait for this thread*/
    int exit_status;                    /* store exit_status when it is called */
    bool is_process;                    /* check whether thread is proces of kernel thread*/
    bool is_loaded;                     /* check whether thread's child process is loaded successfully */
    int fd_given;                       /* check last fd number give for the opened file */
    struct file * executable;           /* to deny and allow executable */

    //For Project 3
    struct hash spt;                    /* hash table for supplemental page table */
    bool filesys_holder;
    ///WHERE WE ADDED END/////

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);
///WHERE WE ADDED/////////
/*IMPLEMENTAION FOR priority donation*/
struct thread *thread_get_priority_donation(struct thread *, int);
int thread_get_priority_for_thread (struct thread *t);
///WHERE WE ADDED END/////

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

///WHERE WE ADDED/////////
/*IMPLEMENTAION FOR sleeplist(bloack & unblock) & comparision list elom*/
bool priority_less_func(const struct list_elem *a,
			       const struct list_elem *b,
			       void *aux);
extern struct list sleep_list; // added

void increment_recent_cpu(struct thread *);
void update_load_avg(void);
void update_recent_cpus(void);
void update_priorities(void);
int calc_priority(int, int);

//for global file locking, initialized to 1 sema_init(&filesys_global_lock , 1) for proj2
extern struct semaphore filesys_global_lock;
///WHERE WE ADDED END///

#endif /* threads/thread.h */
