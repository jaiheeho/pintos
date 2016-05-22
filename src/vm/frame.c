#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <list.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/malloc.h"

static struct list frame_table;
static struct semaphore frame_table_lock;
static struct list_elem *clock_head; 

void frame_free_nolock(struct fte* fte_to_free);

/************************************************************************
* FUNCTION : frame_table_init                                           *
* Purpose : initialize frame-table                                      *
************************************************************************/
void frame_table_init()
{
  list_init(&frame_table);
  sema_init(&frame_table_lock,1);
  clock_head = NULL;
}

/************************************************************************
* FUNCTION : frame_table_free                                           *
* Purpose : free entire frame_table                                     *
************************************************************************/
// is this necessary??
void frame_table_free()
{
  sema_down(&frame_table_lock);
  //empty frame_table list;
  while (!list_empty (&frame_table))
  {
    struct list_elem *e = list_pop_front (&frame_table);
    struct fte *frame_entry = list_entry(e, struct fte, elem);
    palloc_free_page(frame_entry->frame_addr);
    palloc_free_page(frame_entry);
  }
  sema_up(&frame_table_lock);
}

/************************************************************************
* FUNCTION : frame_allocate                                             *
* Input : supplement_page pointer                                       *
* Output : allocated frame pointer for supplement_page                  *
* Purpose : allocating frame and add frame informatino to frame table   *
************************************************************************/
void* frame_allocate(struct spte* supplement_page)
{
  //printf("frame_allocate: \n");
  sema_down(&frame_table_lock);
  void * new_frame=NULL;
  while(1)
  {
    new_frame = palloc_get_page(PAL_USER | PAL_ZERO);
    if(new_frame == NULL)
    {
      // all frame slots are full; commence eviction & retry
      //printf("frame_allocate: need eviction!!\n");
      frame_evict();
    }
    else
    {
        // printf("here2-0-4\n");

      // frame allocation succeeded; add to frame table
      struct fte* new_fte_entry = (struct fte*)malloc(sizeof(struct fte));
      if(new_fte_entry == NULL)
    	{
    	   printf("cannot allocate new fte!!\n");
    	   PANIC("Cannot allocate new fte!!");
    	}
      // configure elements of fte
      new_fte_entry->frame_addr = new_frame;
      new_fte_entry->use = 1;
      new_fte_entry->thread = thread_current();
      supplement_page->frame_locked = true;

      //link to spte
      supplement_page->phys_addr = new_frame;
      supplement_page->fte = new_fte_entry;
      new_fte_entry->supplement_page = supplement_page;
      
      // insert into frame table 
      //if table is empty, adjust clck_head;
      if (list_empty(&frame_table) || clock_head == NULL)
      {
        list_push_back(&frame_table, &new_fte_entry->elem);
        clock_head = list_begin(&frame_table);
      }
      else
      {
        list_push_back(&frame_table, &new_fte_entry->elem);
      }
      break;
    }              
  }
  sema_up(&frame_table_lock);
  return new_frame;
}

/************************************************************************
* FUNCTION : frame_free                                                 *
* Input : fte pointer to freee                                          *
* Purpose : free a frame entry of frame table and free allocated pages  *
************************************************************************/
void frame_free(struct fte* fte_to_free)
{
  // free frame table entry
  sema_down(&frame_table_lock);
  //detach fte from frame table list
  list_remove(&fte_to_free->elem);
  //detach frame from spte (this is for ensurance)
  //printf("AAA: %0x\n", ((struct spte *)fte_to_free->supplement_page)->user_addr);
  pagedir_clear_page(fte_to_free->thread->pagedir, ((struct spte *)fte_to_free->supplement_page)->user_addr);
  // free palloc'd page
  palloc_free_page(fte_to_free->frame_addr);

  // free malloc'd memory
  free(fte_to_free);
  sema_up(&frame_table_lock);
}

/************************************************************************
* FUNCTION : frame_free_nolock                                          *
* Input : fte pointer to freee                                          *
* Purpose : free a frame entry of frame table and free allocated pages  *
************************************************************************/
void frame_free_nolock(struct fte* fte_to_free)
{
  // free frame table entry
  //detach fte from frame table list
  list_remove(&fte_to_free->elem);
  //detach frame from spte (this is for ensurance)
  //printf("AAA: %0x\n", ((struct spte *)fte_to_free->supplement_page)->user_addr);
  pagedir_clear_page(fte_to_free->thread->pagedir, ((struct spte *)fte_to_free->supplement_page)->user_addr);

  // free palloc'd page
  palloc_free_page(fte_to_free->frame_addr);

  // free malloc'd memory
  free(fte_to_free);
}

/************************************************************************
* FUNCTION : frame_evict                                                *
* Input : supplement_page pointer                                       *
* Purpose : evict frame from frame table and move to swap space using   *
* functions in swap.c, spte for the frame should be fixed and framd table*
* entry should be freed. we used (FIRST ALLOCATED REMOVE scheme)        *
************************************************************************/
void frame_evict()
{
  //choose victim
  struct list_elem *iter;
  struct fte *frame_entry;
  struct spte *supplement_page;
  struct thread *t;
  void* new;

  //printf("frame_evict:\n");
  //start from the beginning of table.
  if (clock_head == NULL && list_empty(&frame_table))
  {
    printf("????\n");
    return;
  }
  if (clock_head == NULL)
  {
    printf("????\n");
    clock_head = list_begin(&frame_table);
  }

  for (iter = clock_head ;;)
  {
    frame_entry= list_entry(iter, struct fte, elem);
    struct spte *paired_spte = frame_entry->supplement_page;  
    //if(paired_spte->user_addr >= (void*)0xb0000000
    //printf("user_addr: %0x\n", paired_spte->user_addr);
	  if(pagedir_is_accessed(frame_entry->thread->pagedir, paired_spte->user_addr) == true)
    {
      pagedir_set_accessed(frame_entry->thread->pagedir, paired_spte->user_addr, false);
    }
	  else
    {
      // not recently used. commence eviction
      break;
    }
    iter = list_next(iter);
    if(iter == list_end(&frame_table))
  	{
  	  iter = list_begin(&frame_table);
  	}  
  }
  
  frame_entry= list_entry(iter, struct fte, elem);
  supplement_page = frame_entry->supplement_page;
  t = frame_entry->thread;
  supplement_page->present = false; 

  //detach fte from frame table list

  if(list_next(iter) == list_end(&frame_table))
  {
    list_remove(&frame_entry->elem);
    if (list_empty(frame_table))
      clock_head == NULL;
    else
      clock_head = list_begin(&frame_table);
  }
  else
  {
    clock_head = list_next(iter);
    list_remove(&frame_entry->elem);
  }

  //detach frame from spte (this is for ensurance)
  pagedir_clear_page(t->pagedir, supplement_page->user_addr);

  supplement_page->swap_idx = swap_alloc((char*)frame_entry->frame_addr);
  // free palloc'd page
  palloc_free_page(frame_entry->frame_addr);
  // free malloc'd memory
  free(frame_entry);
}











