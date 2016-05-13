
#include <hash.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include <string.h>

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
/*
void page_free(struct hash*)
{


}
*/

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
  //printf("load_page: faultaddr=%0x\n", faulted_user_addr);
  //printf("roundeddown: %0x\n", pg_round_down(faulted_user_addr));

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
      new_spte->writable = writable;
      new_spte->swap_idx = -1;

      // insert in spt
      hash_insert(spt, &(new_spte->elem));

      // load n. if this fails, kernel will panic.
      // thus, we dont have to cleanup new_spte
      void* new_frame = frame_allocate(new_spte);
      new_spte->phys_addr = new_frame;
      
      //install the page in user page table
      install_page(new_spte->user_addr, new_spte->phys_addr, writable);
      printf("A\n");
      
    }
  else  // page is in spte.(in swap space)
    {
            printf("B\n");

      spte_target = hash_entry(e, struct spte, elem);
      if(pagedir_get_page(thread_current()->pagedir, spte_target->user_addr))
	{
	  //printf("AAAAAAAAAAAAA\n");
	}


      //printf("load_page: spte_target: user_addr=%0x, present=%d, swap_idx=%d\n", 
      // spte_target->user_addr, spte_target->present, spte_target->swap_idx);
      
      if(!load_page_swap(spte_target))
	{
	  return 0;
	}
      
      /*
      if(spte_target->status == ON_MEM)
	{
	  // page is already on memory. wth?
	  PANIC("...");
	}
      else if(spte_target->status == ON_SWAP)
	{
	  // the page is in swap space. bring it in
	  void* new_frame = frame_allocate(spte_target);
	  swap_remove(new_frame, spte_target->swap_idx);
	  install_page(spte_target->user_addr, spte_target->phys_addr, writable);
	}
      */
      
    }
  return 1;
}

int load_page_new(void* user_page_addr, bool writable)
{
  //printf("load_page_new: %0x", user_page_addr);
  // create new spte
  struct spte* new_spte = (struct spte*)malloc(sizeof(struct spte));
  //printf("CP0\n");
  if(new_spte == NULL) return 0;
  new_spte->user_addr = user_page_addr;
  new_spte->status = ON_MEM;
  new_spte->present = true;
  new_spte->dirty = false;
  new_spte->writable = writable;
  new_spte->swap_idx = -1;
  //printf("CP1\n");
  //get the spte for this addr
  struct hash *spt = &thread_current()->spt;

  //insert
  hash_insert(spt, &(new_spte->elem));
  //printf("CP2\n");
  // load n. if this fails, kernel will panic.
  // thus, we dont have to cleanup new_spte
  void* new_frame = frame_allocate(new_spte);
  new_spte->phys_addr = new_frame;
  //printf("CP3\n");
  //install the page in user page table
  if(install_page(new_spte->user_addr, new_spte->phys_addr, writable) == false)
    {
      palloc_free_page(new_frame);
      return 0;

    }
  return 1;
  
}



int load_page_file(void* user_page_addr, struct file *file, off_t ofs,
		   uint32_t page_read_bytes, uint32_t page_zero_bytes,
		   bool writable)
{
  //printf("load_page_file: %0x\n", user_page_addr);
  //printf("rounded down: %0x\n", pg_round_down(user_page_addr));
  // create new spte
  struct spte* new_spte = (struct spte*)malloc(sizeof(struct spte));
  //printf("CP0\n");
  if(new_spte == NULL) return 0;
  new_spte->user_addr = user_page_addr;
  new_spte->status = ON_MEM;
  new_spte->present = true;
  new_spte->dirty = false;
  new_spte->writable = writable;
  new_spte->swap_idx = -1;
  //printf("CP1\n");
  //get the spte for this addr
  struct hash *spt = &thread_current()->spt;

  //insert
  hash_insert(spt, &(new_spte->elem));
  //printf("CP2\n");
  // load n. if this fails, kernel will panic.
  // thus, we dont have to cleanup new_spte
  void* new_frame = frame_allocate(new_spte);
  //printf("CP2.5\n");
  new_spte->phys_addr = new_frame;
  //printf("CP3\n");


  //load from file
  if(file_read(file, new_frame, page_read_bytes) != (int) page_read_bytes)
    {
      printf("FILE READ FAIL\n");
      return false;

    }
  memset(new_frame + page_read_bytes, 0, page_zero_bytes);


  //install the page in user page table
  if(install_page(new_spte->user_addr, new_spte->phys_addr, writable) == false)
    {
      palloc_free_page(new_frame);
      return 0;

    }

  return 1;
  
}






int load_page_swap(struct spte* spte_target)
{
  //printf("load_page_swap: init\n");
  bool writable = spte_target->writable;
 
  if(spte_target->present == false)
    {
      // the page is in swap space. bring it in
      void* new_frame = frame_allocate(spte_target);
      if(new_frame == NULL)
	{
	  //printf("load_page_swap: frame allocate error\n");
	  return 0;
	}
      swap_remove(new_frame, spte_target->swap_idx);
      install_page(spte_target->user_addr, spte_target->phys_addr, writable);
    }
  else
    {
      //printf("load_page_swap : present bit is true??\n");
      return 0;
    }

  //printf("load_page_swap: end\n");
  return 1;
}


int stack_growth(void *user_addr)
{

  void* new_stack_page = pg_round_down(user_addr);
  //printf("STACK_GROWTH: user_addr = %0x, newstackpage  = %0x\n", user_addr, new_stack_page);
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
