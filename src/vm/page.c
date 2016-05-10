#include <hash.h>
#include "vm/frame.h"
#include "vm/page.h"




unsigned spte_hash_func(const struct hash_elem *e, void *aux)
{
  struct spte* s = hash_entry(e, struct spte, elem);
  return hash_int(f->user_addr);
}



int load_page_on_fault(void* faulted_user_addr)
{
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
