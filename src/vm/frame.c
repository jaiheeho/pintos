#include "userprog/pagedir.h"
#include "threads/palloc.h"
#include "frame.h"
#include <list.h>




bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b,
		     void *aux)
{
  

}

void frame_destroyer_func(struct hash_elem *e, void *aux)
{
  struct fte* f = hash_entry(e, struct fte, elem);
  // free the palloc'd frame
  palloc_free_page(f->frame_addr);
  // free fte
  free(f);
}






// must protect frame table with lock cuz frame table is shared
// across processes

void frame_table_init(struct list* ft)
{
  list_init(ft);

}


// is this necessary??
void frame_table_free(struct list* ft)
{
  hash_destroy(ft, frame_destroyer_func);

}




void* frame_allocate(struct spte *sup_page)
{
  
  while(1)
    {
      void* new_frame = palloc_get_page(palloc_flags);
      
      if(new_frame == NULL)
	{
	  // all frame slots are full; commence eviction & retry
	  if (frame_evict())
	    {
	      // eviction succeeded. retry palloc
	      continue;
	    }
	  else
	    {
	      // eviction failed(swap space is full). kernel panic
	      PANIC("SWAP SPACE FULL\n");
	      return NULL; // must not reach here
	    }
	}
      else
	{
	  // frame allocation succeeded; add to frame table
	  struct fte* new_fte_entry = (struct fte*)malloc(sizeof(struct fte));
	  // configure elements of fte
	  new_fte_entry->frame_addr = new_frame;
	  // insert into frame table
	  list_push_back(&frame_table, &new_fte_entry->elem);
	  //link to spte
	  sup_page->phys_addr = new_frame;
	  sup_page->frame = new_fte_entry;
	  new_fte_entry->sup_page = sup_page;

	  break;
	}
    }

  return new_frame;
  
}

void frame_free(struct fte* fte_to_free)
{
  // free frame table entry



  // free palloc'd page
  palloc_free_page(fte_to_free->frame_addr);


  //detach fte from frame table
  list_remove(fte_to_free->elem);

  //detach frame from spte (this is for ensurance)
  pagedir_clear_page(thread_current()->pagedir, fte_to_free->sup_page->user_addr);

  // free malloc'd memory
  free(fte_to_free);


}

int frame_evict()
{
  

}
