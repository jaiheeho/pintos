#include <hash.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "userprog/p"


static unsigned spte_hash_func(const struct hash_elem *e, void *aux);
static bool spte_less_func(const struct hash_elem *a,
        const struct hash_elem *b,
        void *aux);
static void spte_destroyer_func(struct hash_elem *e, void *aux);
bool install_page (void *upage, void *kpage, bool writable);

static unsigned spte_hash_func(const struct hash_elem *e, void *aux)
{
  struct spte* s = hash_entry(e, struct spte, elem);
  return hash_int((int)s->user_addr);
}

static bool spte_less_func(const struct hash_elem *a,
        const struct hash_elem *b,
        void *aux)
{
  struct spte *alpha = hash_entry(a, struct spte, elem);
  struct spte *beta = hash_entry(b, struct spte, elem);

  if(alpha->user_addr < beta->user_addr)
    return true;
  else return false;

}

static void spte_destroyer_func(struct hash_elem *e, void *aux)
{
  // 0) get the spte
  struct spte *target = hash_entry(e, struct spte, elem);

  // 1) free the underlying frame
  frame_free(target->fte);

  // 2) detach from pt(this is also done in frame_free. doublechecking)
  pagedir_clear_page(thread_current()->pagedir, target->user_addr);

  // 3) free spte
  free(target);
}

void sup_page_table_init(struct hash* sup_page_table)
{
  hash_init(sup_page_table, spte_hash_func, spte_less_func, NULL);
}

void sup_page_table_free(struct hash* sup_page_table)
{
  hash_destroy(sup_page_table, spte_destroyer_func);
}


int load_page(void* faulted_user_addr)
{

  bool writable = true;

  //get the spte for this addr
  struct hash *spt = &thread_current()->spt;
  void* faulted_user_page = pg_round_down(faulted_user_addr);

  // find the spte with infos above(traverse spt)
  struct spte spte_temp;
  struct hash_elem *e;
  struct spte* spte_target;

  spte_temp.user_addr = faulted_user_page;
  e = hash_find(spt, &spte_temp.elem);

  if(e == NULL)
    {
      // no such page. check validity of addr and load new page?
      // create new spte
      struct spte* new_spte = (struct spte*)malloc(sizeof(struct spte));
      new_spte->user_addr =faulted_user_page;
      new_spte->status = ON_MEM;
      new_spte->present = true;
      new_spte->dirty = false;

      // insert in spt
      hash_insert(spt, &(new_spte->elem));

      // load n. if this fails, kernel will panic.
      // thus, we dont have to cleanup new_spte
      void* new_frame = frame_allocate(new_spte);
      new_spte->phys_addr = new_frame;

      //install the page in user page table
      install_page(new_spte->user_addr, new_spte->phys_addr, writable);

    }
  else  // page is in spte.(in swap space)
    {
      spte_target = hash_entry(e, struct spte, elem);
      
      if(spte_target->status == ON_MEM)
  {
    // page is already on memory. wth?
  }
      else if(spte_target->status == ON_SWAP)
  {
    // the page is in swap space. bring it in
    void* new_frame = frame_allocate(spte_target);
    swap_remove(new_frame, spte_target->swap_idx);
    install_page(spte_target->user_addr, spte_target->phys_addr, writable);
  }
      
    }
}


int stack_growth(void *user_esp)
{

  void* new_stack_page = pg_round_down(user_esp) - PGSIZE;
  load_page(new_stack_page);

}
bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}