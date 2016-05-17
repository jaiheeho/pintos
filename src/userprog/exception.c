#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
/*ADDED HEADERS*/
#include "userprog/syscall.h" // ADDED HEADER
#include "vm/page.h"
#include "threads/vaddr.h"

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.
   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.
   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.
   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.
   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));
  
  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;
  
  /***** ADDED CODE *****/
  /*Deferencing NULL should be exited instead of killed (test : bad_read)*/
  /*Deferencing addr above 0xC0000000 should be exited instead of killed (test : bad_read)*/
  //printf("faulted_addr: %0x\n", fault_addr);
  //printf("Errorcode : %d %d %d\n", not_present, write, user);
  // printf("tid: %d\n", thread_current()->tid);
  
  /* this case is for invalid fauled_addr exit process and release the lock if thread has lock
  (1) fauld_addr is NULL
  (2) fauld_addr is above PHYS_BASE
  (3) fault_addr is below cod segment
  (4) page_fault is due to writing r/o page
  */
  if (fault_addr == NULL || fault_addr >= PHYS_BASE
      || fault_addr < (void*)0x08048000 || (!not_present))
  {
    if(!user)
  	{
  	  sema_up(&filesys_global_lock);
  	}
    exit(-1);
  }

  /* check whether invalid by exceeding STACK_MAX*/
  if( (uint32_t)PHYS_BASE - (uint32_t)fault_addr >= (uint32_t)STACK_MAX )
  {
    if(!load_page(fault_addr))
     PANIC("Exceeded STACK_MAX");
  }

  /* not_present | write | user = 100*/
  if ( not_present && write && !user)
  {
    if(!load_page_for_read(fault_addr))
    {
      sema_up(&filesys_global_lock);
      exit(-1);
    }
  }  
  /* not_present | write | user = 101*/
  if ( not_present && write && user)
  {
    if(!load_page_for_read(fault_addr))
      exit(-1);
  }
  /* not_present | write | user = 110*/
  // if ( not_present && write && !user)
  // {

  
  // }
  /* not_present | write | user = 111*/
  if ( not_present && !write /*&& user*/)
  {
    if (!load_page(fault_addr))
    {
      if ((uint32_t)f->esp - (uint32_t)fault_addr <= (uint32_t)STACK_STRIDE)
      {
        stack_growth(fault_addr);
      }
      else
      {
        if(!user)
        {
            sema_up(&filesys_global_lock);
        }
        // it is stack. but stride aint right
        //printf("page fault handler: stack stride problem. esp = %0x, fault_addr = %0x\n", f->esp, fault_addr);
        exit(-1);
      }
    }
  }

//   /***** END OF ADDED CODE *****/
// if((not_present) && (write))
// {
//     // valid to load page
//     // stack or heap(or other segment)?
//     if((((uint32_t)PHYS_BASE - (uint32_t)fault_addr) < (uint32_t)STACK_MAX))
//     {
//         if((((uint32_t)f->esp - (uint32_t)fault_addr) <= (uint32_t)STACK_STRIDE))
//         {
//             //printf("STACK_MAX = %d, STACK_STRID = %d\n", STACK_MAX, STACK_STRIDE);
//             //printf("GROW : esp = %0x || fault_addr = %0x", f->esp, fault_addr);
//             stack_growth(fault_addr);
//         }
//         else{
//             if((uint32_t)f->esp > (uint32_t)fault_addr)
//             {
//                 if(!user)
//                 {
//                     sema_up(&filesys_global_lock);
//                 }
//                 // it is stack. but stride aint right
//                 //printf("page fault handler: stack stride problem. esp = %0x, fault_addr = %0x\n", f->esp, fault_addr);
//                 exit(-1);
//             }
//             else
//             {
//                 //printf("load page in page_fault");
//                 // swap in the swapped out stack page
//                 if(!load_page(fault_addr))
//                 {
//                     PANIC("load page failed.");
//                 }
//             }
//         }
//     }
//     else
//     {
//         if(!load_page(fault_addr))
//         {
//             PANIC("load page failed.");
//         }
//     }
// }

// if((not_present) && (!write))
// {
//     if(!load_page_for_read(fault_addr))
//     {
//         if(!user)
//         {
//             sema_up(&filesys_global_lock);
//         }
//         // it is stack. but stride aint right
//         //printf("page fault handler: stack stride problem. esp = %0x, fault_addr = %0x\n", f->esp, fault_addr);
//         exit(-1);
//     }
// }
  //printf("page fault handler: end\n");
  
  if(0)
    {
      
      /* To implement virtual memory, delete the rest of the function
	 body, and replace it with code that brings in the page to
	 which fault_addr refers. */
      printf ("Page fault at %p: %s error %s page in %s context.\n",
	      fault_addr,
	      not_present ? "not present" : "rights violation",
	      write ? "writing" : "reading",
	      user ? "user" : "kernel");
      kill (f);
    }
}
