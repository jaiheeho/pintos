#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
//for proj3 
#include "vm/frame.h"
#include "vm/swap.h"
//for proj4
#include "filesys/cache.h"


#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* Sleep List */
struct list sleep_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* IMPLEMENTATAION */
static int load_avg = 0;
#define FP 0x8000

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

/***** ADDED CODE *****/
/* ADDED GLOBAL VARIABLES */
static int thread_start_complete = 0;
//for global file locking, initialized to 1 sema_init(&filesys_global_lock , 1) for proj2
struct semaphore filesys_global_lock;
/***** END OF ADDED CODE *****/

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* ADDED FUNCTIONS */

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  // ADDED CODE //
  list_init (&ready_list);
  list_init (&sleep_list);
  // END OF ADDED CODE//

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();

  ///WHERE WE ADDED/////////
  // initialize initial thread//
  initial_thread->recent_cpu = 0;
  initial_thread->nice = 0;
  initial_thread->pwd = NULL;
  ///WHERE WE ADDED END/////
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
/*fixed function */
void
thread_start (void) 
{
  //printf("THREAD_START\n");
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
  ///WHERE WE ADDED END/////
  //TO CHECK if main thread start//
  thread_start_complete = 1;
  ///WHERE WE ADDED END/////

}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
/*fixed function */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{  
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);
  /* Allocate thread. */
  
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;


  //proj4-3: inherit pwd before we move on
  if(thread_start_complete == 1)
    {
      if(thread_current()->pwd != NULL)
	t->pwd = dir_reopen(thread_current()->pwd);
      else
	t->pwd = dir_open_root();
    }
  /* Add to run queue. */
  thread_unblock (t);
  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);
  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the -to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
/*fixed function */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  ASSERT (t->status == THREAD_BLOCKED);

  ///WHERE WE ADDED/////////
  //printf("unblock : %s, %d, current prio:%d\n", t->name,t->priority,thread_current()->priority);
  old_level = intr_disable ();
  list_insert_ordered(&ready_list, &t->elem,
		      (list_less_func *) &priority_less_func, NULL); 

  if(thread_start_complete == 1)
  {
    if (t->priority >= thread_current()->priority)
    {
      if(intr_context() == false)
	    {
	      thread_yield();
	    }
      else if (intr_context() == true)
	    {
	      intr_yield_on_return();
	    }
    }
  }
  intr_set_level (old_level);
  ///WHERE WE ADDED END/////

}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Just set our status to dying and schedule another process.
     We will be destroyed during the call to schedule_tail(). */
  intr_disable ();
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/************************************************************************
* FUNCTION : thread_set_nice                                            *
* Purporse :  Yields the CPU. The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim.          *
************************************************************************/
/*fixed function */
void
thread_yield (void) 
{
  struct thread *curr = thread_current ();
  enum intr_level old_level;
  //ASSERT (!intr_context ());
  ///WHERE WE ADDED/////////
  old_level = intr_disable ();
  if (curr != idle_thread) {
    list_sort(&ready_list, (list_less_func *) &priority_less_func, NULL);
    list_insert_ordered(&ready_list, &curr->elem,
			(list_less_func *) &priority_less_func, NULL); 
  }
  ///WHERE WE ADDED END/////
  curr->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/************************************************************************
* FUNCTION : thread_set_priority                                        *
* INPUT    : new_priority                                               *
* Purporse : Sets the current thread's priority to NEW_PRIORITY.        *
            check priority of readylist  and preemp if priority is low  *
************************************************************************/
/*fixed function */
void
thread_set_priority (int new_priority) 
{
  enum intr_level old_level;
  int old_priority;
  struct thread *t;

  if (thread_mlfqs)
    return;
  old_level = intr_disable();
  t = thread_current();
  old_priority = t->priority;
  t->priority = new_priority;
  t->priority_rollback = new_priority;
  if (!list_empty(&ready_list) && (new_priority < old_priority))
  {
    struct thread *front_of_q;
    front_of_q = list_entry(list_front(&ready_list), struct thread, elem);
    if (front_of_q->priority > new_priority)
  	{
  	  if(intr_context() == false)
	    {
	      thread_yield();
	    }
  	  else if (intr_context() == true)
	    {
	      intr_yield_on_return();
	    } 
  	}  
  }
  intr_set_level(old_level);
}
/************************************************************************
* FUNCTION : thread_get_priority                                        *
* INPUT    : void                                                       *
* Purporse : Returns the current thread's priority. or donated priority *
* Use thread_get_priority_donation recursively                          *
************************************************************************/
/*fixed function */
int
thread_get_priority (void) 
{
  ///WHERE WE ADDED/////////
  struct list *lock_holding;
  struct list *waiting;
  int max_priority = 0;
  struct lock *l;
  struct thread *t;
  struct thread *max_priority_thread = thread_current();
  struct list_elem *iter_lock;
  struct list_elem *iter_waiting;
  int depth = 8;

  if (thread_mlfqs)
    return thread_current()->priority;

  //we should compare with priority_rollback instead of priority itself because priority can be donated priority
  max_priority = max_priority_thread->priority_rollback; 
  lock_holding = &max_priority_thread->lock_holdings;

  //for every lock current thread own
  for(iter_lock = list_begin(lock_holding);
    iter_lock != list_tail(lock_holding); iter_lock = iter_lock->next)
  {
    l = list_entry(iter_lock, struct lock, elem);
    waiting = &(&l->semaphore)->waiters;
    //for every thread waiting for the lock that current thread own
    for(iter_waiting = list_begin(waiting);
    iter_waiting != list_tail(waiting); iter_waiting = iter_waiting->next)
    {
      t = list_entry(iter_waiting, struct thread, elem);
      //use recursion for deeper chain
      if (t->priority > max_priority){
        max_priority_thread = thread_get_priority_donation(t, depth-1);
        max_priority = max_priority_thread->priority;
      }
    }
  }
  return max_priority;
  ///WHERE WE ADDED END/////
}
/************************************************************************
* FUNCTION : thread_get_priority_for_thread                             *
* INPUT    : target thread                                              *
* Purporse : Returns the target thread's priority. or donated priority  *
* Use thread_get_priority_donation recursively                          *
************************************************************************/
/*added function */
int
thread_get_priority_for_thread (struct thread *target) 
{
  struct list *lock_holding;
  struct list *waiting;
  int max_priority = 0;
  struct lock *l;
  struct thread *t;
  struct thread *max_priority_thread = target;
  struct list_elem *iter_lock;
  struct list_elem *iter_waiting;
  int depth = 8;

  if (thread_mlfqs)
    return thread_current()->priority;

  //we should compare with priority_rollback instead of priority itself because priority can be donated priority
  max_priority = max_priority_thread->priority_rollback; 
  lock_holding = &max_priority_thread->lock_holdings;

  //for every lock current thread own
  for(iter_lock = list_begin(lock_holding);
    iter_lock != list_tail(lock_holding); iter_lock = iter_lock->next)
  {
    l = list_entry(iter_lock, struct lock, elem);
    waiting = &(&l->semaphore)->waiters;
    //for every thread waiting for the lock that current thread own
    for(iter_waiting = list_begin(waiting);
    iter_waiting != list_tail(waiting); iter_waiting = iter_waiting->next)
    {
      //use recursion for deeper chain
      t = list_entry(iter_waiting, struct thread, elem);
      if (t->priority > max_priority){
        max_priority_thread = thread_get_priority_donation(t, depth-1);
        max_priority = max_priority_thread->priority;
      }
    }
  }
  return max_priority;
}
/************************************************************************
* FUNCTION : thread_get_priority_donation                               *
* INPUT    : thread & depth                                             *
* Purporse : Returns the thread that has maximum priority               *
* started from thread_get_priority() & called recursively               *
************************************************************************/
/*added function */
struct thread *
thread_get_priority_donation(struct thread *t, int depth)
{
  struct thread *max_priority_thread = t;
  struct list *lock_holding = &t->lock_holdings;
  int max_priority = t->priority;
  struct list *waiting;
  struct lock *l;
  struct list_elem *iter_lock;
  struct list_elem *iter_waiting;

  if (depth == 0)
    return max_priority_thread;

  //for every lock current thread own
  for(iter_lock = list_begin(lock_holding);
    iter_lock != list_tail(lock_holding); iter_lock = iter_lock->next)
  {
    l = list_entry(iter_lock, struct lock, elem);
    waiting = &(&l->semaphore)->waiters;
    //for every thread waiting for the lock that current thread own
    for(iter_waiting = list_begin(waiting);
    iter_waiting != list_tail(waiting); iter_waiting = iter_waiting->next)
    {
      //use recursion for deeper chain
      t = list_entry(iter_waiting, struct thread, elem);
      if (t->priority > max_priority){
        max_priority_thread = thread_get_priority_donation(t, depth-1);
        max_priority = max_priority_thread->priority;
      }
    }
  }
  return max_priority_thread;
}

/************************************************************************
* FUNCTION : thread_set_nice                                            *
* Purporse : Sets the current thread's nice value to NICE and           *
*           update priorities and Yields                                *
************************************************************************/
/*added function */
void
thread_set_nice (int nice) 
{
  /* Not yet implemented. */
  ///WHERE WE ADDED/////////
  enum intr_level old_level = intr_disable ();
  struct thread *t = thread_current();

  int priority = calc_priority(t->recent_cpu, nice);
  t->nice = nice;
  t->priority = priority;
  update_priorities();
  if( list_empty(&ready_list) == false)
  {
    update_priorities();
    struct thread *front_of_ready = list_entry(list_front(&ready_list), struct thread, elem);
     if (t->priority <= front_of_ready->priority)
    {
      if(intr_context() == false)
      {
        thread_yield();
      }
      else if (intr_context() == true)
      {
        intr_yield_on_return();
      }
    
   }
  }
  intr_set_level (old_level);
  ///WHERE WE ADDED END/////
}

/* Returns the current thread's nice value. */
/*fixed function */
int
thread_get_nice (void) 
{
  ///WHERE WE ADDED/////////
  int tmp = thread_current()->nice;
  return tmp;
  ///WHERE WE ADDED END/////
}
/* Returns 100 times the system load average. */
 /*fixed function */
int
thread_get_load_avg (void) 
{
  ///WHERE WE ADDED/////////
  int temp_load_avg = load_avg;
  return (temp_load_avg * 100 + FP/2) /FP ;
  ///WHERE WE ADDED END/////
}
/* Returns 100 times the current thread's recent_cpu value. */
 /*fixed function */
int
thread_get_recent_cpu (void) 
{
  ///WHERE WE ADDED/////////
  int temp_recent_cpu = thread_current()->recent_cpu;
  return (temp_recent_cpu * 100 + FP/2) /FP;
  ///WHERE WE ADDED END/////
}

/************************************************************************
* FUNCTION : increment_recent_cpu                                       *
* Input : thread* t                                                     *
* Output : NONE                                                         *
* Purporse : increment recent_cpu (recent_cpu ++)                       *
************************************************************************/
 /*added function */
void increment_recent_cpu(struct thread *t)
{
  if (t != idle_thread)
  {
    t->recent_cpu += 0x8000;
  }
}

/************************************************************************
* FUNCTION : update_load_avg                                            *
* Input : NONE                                                          *
* Output : NONE                                                         *
* Purporse : update load_avg when function is Called                    *
************************************************************************/
 /*added function */
void update_load_avg()
{
  int ready_threads = list_size(&ready_list);
  if (thread_current() != idle_thread)
    ready_threads++;
  //load_avg = (59/60) * load_avg + (1/60) *ready_threads;
  load_avg = (59*load_avg)/60 + (ready_threads * FP)/60;
}

/************************************************************************
* FUNCTION : update_recent_cpus                                         *
* Input : NONE                                                          *
* Output : NONE                                                         *
* Purporse : update Recent_cpu values of all threads                    *
* when function is Called                                               *
************************************************************************/
 /*added function */
void update_recent_cpus(){
  //recent_cpu = (2*load_avg)/2*load_avg + 1) * recent_cpu + nice
  //recent_cpu = coeff * recent_cpu + nice
  int coeff, nice, recent;
  struct thread *t;
  struct list_elem *iter;

  t = thread_current();

  if (t != idle_thread)
  {
    recent = t->recent_cpu;
    nice = t->nice;
    coeff = (((int64_t)(2*load_avg)) * FP )/ (2*load_avg + FP);
    t -> recent_cpu = ((int64_t)coeff) * recent / FP + nice * FP;
  }
  /* update for sleep list*/
  for(iter = list_begin(&sleep_list);
    iter != list_tail(&sleep_list); iter = iter->next)
  {

    t = list_entry(iter, struct thread, elem);
    recent = t->recent_cpu;
    nice = t->nice;
    coeff = (((int64_t)(2*load_avg)) * FP )/ (2*load_avg + FP);
    t -> recent_cpu = ((int64_t)coeff) * recent / FP + nice * FP;
  }

  /* update for ready list*/
  for(iter = list_begin(&ready_list);
    iter != list_tail(&ready_list); iter = iter->next)
  {
    t = list_entry(iter, struct thread, elem);
    recent = t->recent_cpu;
    nice = t->nice;
    coeff = (((int64_t)(2*load_avg)) * FP )/ (2*load_avg + FP);
    t -> recent_cpu = ((int64_t)coeff) * recent / FP + nice * FP;
  }
}
/************************************************************************
* FUNCTION : update_priorities                                          *
* Input : NONE                                                          *
* Output : NONE                                                         *
* Purporse : update priority values of all threads                      *
* when function is Called                                               *
************************************************************************/
 /*added function */
void update_priorities(void)
{
  //enum intr_level old_level = intr_disable ();
  int nice, recent, priority;
  struct thread *t;
  struct list_elem *iter;

  t = thread_current();

  if (t != idle_thread)
  {
    recent = t->recent_cpu;
    nice = t->nice;
    priority = calc_priority(recent, nice);
    t-> priority = priority;
  }
  /* update for sleep list*/
  for(iter = list_begin(&sleep_list);
    iter != list_tail(&sleep_list); iter = iter->next)
  {
    t = list_entry(iter, struct thread, elem);
    recent = t->recent_cpu;
    nice = t->nice;
    priority = calc_priority(recent, nice);
    t-> priority = priority;
  }
  /* update for ready list*/
  for(iter = list_begin(&ready_list);
    iter != list_tail(&ready_list); iter = iter = iter->next)
  {
    t = list_entry(iter, struct thread, elem);
    recent = t->recent_cpu;
    nice = t->nice;
    priority = calc_priority(recent, nice);
    t-> priority = priority;
  }

  list_sort(&ready_list, (list_less_func *) &priority_less_func, NULL);
}
/************************************************************************
* FUNCTION : calc_priority                                              *
* Input : recent_cpu,  nice                                             *
* Output : priority                                                     *
* Purporse : return priority calculate by recent_cpu                    * 
*           and nice of a thread                                        *             
************************************************************************/
 /*added function */
int calc_priority(int recent_cpu, int nice)
{
  int priority;

  priority = PRI_MAX*FP - recent_cpu/4 - (nice*2)*FP;
  priority = (priority + FP/2)/ FP;

  if (priority < PRI_MIN)
    priority = PRI_MIN;
  else if (priority > PRI_MAX)
    priority = PRI_MAX;

  return priority;
}


/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);
                    
  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Since `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
 /*fixed function */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->magic = THREAD_MAGIC;

  ///WHERE WE ADDED/////////
  //IMLEMENTIAION TO INITIALIZE recent_cpu to 0//
  //FOR BSD Scheduler//
  if (thread_start_complete == 1)
  {
    t->nice = thread_current()->nice;
  }
  t->recent_cpu = 0;
  //FOR PRIORITY DONTATION//
  list_init (&t->lock_holdings);
  t->priority_rollback = priority;
  //FOR PROCESS INHERITANE in Proj2//
  list_init (&t->child_procs);
  sema_init(&t->sema_wait, 1);
  if (thread_start_complete == 1)
  {
    list_push_back (&thread_current()->child_procs, &t->child_elem);
    t->parent_proc = thread_current();
  }
  else
  {
    //main process does not have parent process
    t->parent_proc = NULL;
  }
  t->is_wait_called = false;
  t->is_process = false;
  t->exit_status = 0;
  t->is_loaded = 2;
  //To check executable
  t->executable = NULL;
  sema_init(&t->loading_safer,1);
  //initialize only in Main thread.
  if (thread_start_complete == 0)
  {
    //FOR GLOBAL FILESYS LOCK in proj2 only 'main' init this//
    sema_init(&filesys_global_lock, 1);
  }
  ///WHERE WE ADDED END/////
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
schedule_tail (struct thread *prev) 
{
  struct thread *curr = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  curr->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread)  
    {
      ASSERT (prev != curr);
      palloc_free_page(prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.
   
   It's not safe to call printf() until schedule_tail() has
   completed. */
static void
schedule (void) 
{
  struct thread *curr = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (curr->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (curr != next)
    prev = switch_threads (curr, next);
  schedule_tail (prev); 
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);

/************************************************************************
* FUNCTION : priority_less_func                                         *
* Purporse Function to compare priorities inside ready_list             *
************************************************************************/
 /*added function */
bool
priority_less_func(const struct list_elem *a, const struct list_elem *b, void *aux)
{
  struct thread *alpha = list_entry(a, struct thread, elem);
  struct thread *beta = list_entry(b, struct thread, elem);

  if (alpha->priority > beta->priority)
    {
      return true;
    }
  else return false;
}
