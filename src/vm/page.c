#include <hash.h>
#include "vm/frame.h"
#include "vm/page.h"




unsigned spte_hash_func(const struct hash_elem *e, void *aux)
{
  struct spte* s = hash_entry(e, struct spte, elem);
  return hash_int(f->user_addr);
}

bool spte_less_func(const struct hash_elem *a,
		    const struct hash_elem *b,
		    void *aux)
{



}

void spte_destroyer_func()
{


}



int load_page_on_fault(void* faulted_user_addr)
{

  bool writable = true;


  //get the spte for this addr
  struct sup_page_table* spt = thread_current()->spt;
  void* faulted_user_page = pg_round_down(faulted_user_addr);

  // find the spte with infos above(traverse spt)
  struct spte spte_temp;
  struct hash_elem *e;
  struct spte* spte_target;

  spte_temp.user_addr = faulted_user_page;
  e = hash_find(&spt, &spte_temp.hash_elem);

  if(e == NULL)
    {
      // no such page. check validity of addr and load new page?

      // load new frame
      void* new_frame = frame_allocate();
      
      // create spte for it
      struct spte* new_spte = (struct spte*)malloc(sizeof(struct spte));
      new_spte->user_addr =faulted_user_page;
      new_spte->phys_addr = new_frame;
      new_spte->status = LOADED;
      new_spte->present = true;
      new_spte->dirty = false;

      // insert in spt
      hash_insert(spt, &(new_spte->elem));


      //install the page in user page table
      install_page(new_spte->user_addr, new_spte->phys_addr, writable);

    }
  else
    {
      spte_target = hash_entry(e, struct fte, elem);
      
      if(spte->target.status == LOADED)
	{
	  // page is already on memory. wth?
	}
      else if(spte->target.status == ON_SWAP)
	{
	  // the page is in swap space. bring it in

	}
      
    }
}
