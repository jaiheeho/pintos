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

static struct list frame_table;
static struct semaphore frame_table_lock;
static struct list_elem *clock_head; 

/************************************************************************
* FUNCTION : frame_table_init                                           *
* Purpose : initialize frame-table                                      *
************************************************************************/
void frame_table_init()
{
  list_init(&frame_table);
  sema_init(&frame_table_lock,1);
  clock_head = list_head(&frame_table);
}

/************************************************************************
* FUNCTION : frame_table_free                                           *
* Purpose : free entire frame_table                                     *
************************************************************************/
// is this necessary??
void frame_table_free()
{
  sema_down(&frame_table_lock);
  sema_up(&frame_table_lock);
  //empty frame_table list;
  while (!list_empty (&frame_table))
  {
    struct list_elem *e = list_pop_front (&frame_table);
    struct fte *frame_entry = list_entry(e, struct fte, elem);
    palloc_free_page(frame_entry->frame_addr);
    palloc_free_page(frame_entry);
  }
}

/************************************************************************
* FUNCTION : frame_allocate                                             *
* Input : supplement_page pointer                                       *
* Output : allocated frame pointer for supplement_page                  *
* Purpose : allocating frame and add frame informatino to frame table   *
************************************************************************/
void* frame_allocate(struct spte *supplement_page)
{
  sema_down(&frame_table_lock);;
  void * new_frame=NULL;
  while(1)
  {
    new_frame = palloc_get_page(PAL_USER | PAL_ZERO);
    if(new_frame == NULL)
    {
      // all frame slots are full; commence eviction & retry
      frame_evict(supplement_page);
    }
    else
    {
      // frame allocation succeeded; add to frame table
      struct fte* new_fte_entry = (struct fte*)malloc(sizeof(struct fte));
      // configure elements of fte
      new_fte_entry->frame_addr = new_frame;
      new_fte_entry->use = 1;
      // insert into frame table
      if (clock_head == list_head(&frame_table))
      {
        list_push_back(&frame_table, &new_fte_entry->elem);
        clock_head = list_begin(&frame_table);
      }
      else {
        list_push_back(&frame_table, &new_fte_entry->elem);
      }

      //link to spte
      supplement_page->phys_addr = new_frame;
      supplement_page->fte = (void *)new_fte_entry;
      new_fte_entry->supplement_page = (struct spte *)supplement_page;
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
  // free palloc'd page
  palloc_free_page(fte_to_free->frame_addr);
  //detach fte from frame table list
  list_remove(&fte_to_free->elem);
  //detach frame from spte (this is for ensurance)
  pagedir_clear_page(thread_current()->pagedir, ((struct spte *)fte_to_free->supplement_page)->user_addr);
  // free malloc'd memory
  palloc_free_page(fte_to_free);
}

/************************************************************************
* FUNCTION : frame_evict                                                *
* Input : supplement_page pointer                                       *
* Purpose : evict frame from frame table and move to swap space using   *
* functions in swap.c, spte for the frame should be fixed and framd table*
* entry should be freed. we used (FIRST ALLOCATED REMOVE scheme)        *
************************************************************************/
void frame_evict(struct spte *supplement_page)
{
  //choose victim
  struct list_elem *iter;
  bool success;
  struct fte *frame_entry= list_entry(iter, struct fte, elem);

  //start from the clock_head
  for (iter = clock_head ; iter != list_end(&frame_table) ; iter = list_next(iter))
  {
    frame_entry= list_entry(iter, struct fte, elem);
    if(frame_entry->use == 1)
      frame_entry->use =0;
    else
    {
      if (list_next(iter) == list_end(&frame_table))
        clock_head = list_begin(&frame_table);
      else
        clock_head = list_next(clock_head);
      break;
    }
  }
  //start from the beginning of table.
  if (iter == list_end(&frame_table))
  {
    for (iter = list_begin(&frame_table);; iter != list_end(&frame_table) ; iter = list_next(iter))
    {
      frame_entry= list_entry(iter, struct fte, elem);
      if(frame_entry->use == 1)
        frame_entry->use =0;
      else
      {
        if (list_next(iter) == list_end(&frame_table))
          clock_head = list_begin(&frame_table);
        else
          clock_head = list_next(clock_head);
        break;
      }
    }
  }
  frame_entry= list_entry(iter, struct fte, elem);
  supplement_page->swap_idx = swap_alloc((char*)frame_entry->frame_addr);
  supplement_page->present = false;
  frame_free(frame_entry);
}
