#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <list.h>
#include "userprog/pagedir.h"
#include "threads/palloc.h"
#include "vm/frame.h"
#include "threads/synch.h"

static struct list frame_table;
static struct semaphore frame_table_lock;
void frame_destroyer_func(struct list_elem *e, void *aux);

/************************************************************************
* FUNCTION : frame_destroyer_func                                                *
* Input : file_name                                                     *
* Output : new process id if successful                                 *
* Purpose : initialize swap-table                   *
************************************************************************/
void frame_destroyer_func(struct list_elem *e, void *aux)
{
  struct fte* frame_entry = list_entry(e, struct fte, elem);
  // free the palloc'd frame
  palloc_free_page(frame_entry->frame_addr);
  // free fte
  free(frame_entry);
}

/************************************************************************
* FUNCTION : frame_table_init                                                *
* Input : file_name                                                     *
* Output : new process id if successful                                 *
* Purpose : initialize swap-table                   *
************************************************************************/
// must protect frame table with lock cuz frame table is shared
// across processes
void frame_table_init()
{
  list_init(&frame_table);
  sema_init(&frame_table_lock,1);
}

/************************************************************************
* FUNCTION : frame_table_free                                                *
* Input : file_name                                                     *
* Output : new process id if successful                                 *
* Purpose : initialize swap-table                   *
************************************************************************/
// is this necessary??
void frame_table_free()
{
  sema_down(&frame_table_lock);
  sema_up(&frame_table_lock);
  while (!list_empty (&frame_table))
  {
    struct list_elem *e = list_pop_front (&frame_table);
    struct fte *frame_entry = list_entry(e, struct fte, elem);
    palloc_free_page(frame_entry->frame_addr);
    free(frame_entry);
  }
}

/************************************************************************
* FUNCTION : frame_allocate                                                *
* Input : file_name                                                     *
* Output : new process id if successful                                 *
* Purpose : initialize swap-table                   *
************************************************************************/
void* frame_allocate(struct spte *supplement_page)
{
  sema_down(&frame_table_lock);
  while(1)
  {
    void* new_frame = palloc_get_page(PAL_USER | PAL_ZERO);
    if(new_frame == NULL)
    {
      // all frame slots are full; commence eviction & retry
      frame_evict(supplement_page);
    }
    else
    {
      // frame allocation succeeded; add to frame table
      struct fte* new_fte_entry = (struct fte*)palloc_get_page(PAL_USER);
      // configure elements of fte
      new_fte_entry->frame_addr = new_frame;
      // insert into frame table
      list_push_back(&frame_table, &new_fte_entry->elem);
      
      //link to spte
      supplement_page->phys_addr = new_frame;
      supplement_page->frame = (void *)new_fte_entry;
      new_fte_entry->supplement_page = (struct spte *)supplement_page;
      break;
    }
  }
  sema_up(&frame_table_lock);
  return new_frame;
}

/************************************************************************
* FUNCTION : frame_table_free                                                *
* Input : file_name                                                     *
* Output : new process id if successful                                 *
* Purpose : initialize swap-table                   *
************************************************************************/
void frame_free(struct fte* fte_to_free)
{
  // free frame table entry
  // free palloc'd page
  palloc_free_page(fte_to_free->frame_addr);
  //detach fte from frame table list
  list_remove(fte_to_free->elem);
  //detach frame from spte (this is for ensurance)
  pagedir_clear_page(thread_current()->pagedir, fte_to_free->supplement_page->user_addr);
  // free malloc'd memory
  free(fte_to_free);
}

int frame_evict(struct spte *supplement_page)
{
  struct list_elem *e = list_pop_front (&frame_table);
  struct fte *frame_entry = list_entry(e, struct fte, elem);
  supplement_page->swap_idx = swap_alloc((char*)frame_entry->frame_addr);
  supplement_page->present = false;
  frame_free(frame_entry);
}
