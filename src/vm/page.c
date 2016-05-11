#include <hash.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"

#define STACK_MAX 8000000

static unsigned spte_hash_func(const struct hash_elem *e, void *aux);
static bool spte_less_func(const struct hash_elem *a,
        const struct hash_elem *b,
        void *aux);
static void spte_destroyer_func(struct hash_elem *e, void *aux);


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
  e = hash_find(spt, &spte_temp.hash_elem);

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
      spte_target = hash_entry(e, struct fte, hash_elem);
      
      if(spte->target.status == ON_MEM)
  {
    // page is already on memory. wth?
  }
      else if(spte->target.status == ON_SWAP)
  {
    // the page is in swap space. bring it in
    void* new_frame = frame_allocate(spte_target);
    some_swap_function();
    install_page(spte_target->user_addr, sptr_target->phys_addr, writable);
  }
      
    }
}


int stack_growth(void *user_esp)
{

  if((PHYS_BASE - user_esp) > STACK_MAX)
  

  void* new_stack_page = pg_round_down(user_esp);


  load_page();
  

  //get the spte for this addr
  struct sup_page_table* spt = thread_current()->spt;
  void* faulted_user_page = pg_round_down(faulted_user_addr);

  // find the spte with infos above(traverse spt)
  struct spte spte_temp;
  struct hash_elem *e;
  struct spte* spte_target;

  spte_temp.user_addr = faulted_user_page;
  e = hash_find(&spt, &spte_temp.hash_elem);

}