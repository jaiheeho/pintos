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
  


}
