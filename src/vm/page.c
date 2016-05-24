
#include <hash.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

struct spte* create_new_spte_insert_to_spt(void *user_addr);
struct hash_elem* found_hash_elem_from_spt(void *faulted_user_page);
bool install_page (void *upage, void *kpage, bool writable);
void spte_free(struct spte* spte_to_free);
int loading_from_executable(struct spte* spte_target);

/* *************************************************************
 * handlers to manage supplement hash table (which is hash)    *
 * *************************************************************/

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

  if(target->present == true)
    {
      // 1) free the underlying frame
      // 2) detach from pt(this is also done in frame_free. doublechecking)
      // printf("IN destroy: frame -free\n");
      frame_free_nolock(target->fte);
      //pagedir_clear_page(thread_current()->pagedir, target->user_addr);
    }
  else
    {
      // 1) free swap slot
      //printf("IN destroy: swap");
      if (!target->wait_for_loading)
      {
        swap_free_slot(target->swap_idx);
        //printf(" -> swap");
      }
      // printf("\n");
    }
  // 3) free spte
  free(target);
}
/* *************************************************************
 * handlers to manage supplement hash table (which is hash)END *
 * *************************************************************/

void sup_page_table_init(struct hash* sup_page_table)
{
  hash_init(sup_page_table, spte_hash_func, spte_less_func, NULL);
}

void sup_page_table_free(struct hash* sup_page_table)
{
  frame_table_locking();
  hash_destroy(sup_page_table, spte_destroyer_func);
  frame_table_unlocking();
}

/************************************************************************
* FUNCTION : load_page_for_write                                        *
* Input : faulted_user_addr                                             *
* Output : Success or FAIL                                              *
* Purpose : load page of faulted_user_addr  for writing                 *
************************************************************************/
int load_page_for_write(void* faulted_user_addr)
{
  // printf("load_page_for_write: faultaddr=%0x\n", faulted_user_addr);
  //printf("roundeddown: %0x\n", pg_round_down(faulted_user_addr));

  void* faulted_user_page = pg_round_down(faulted_user_addr);
  struct hash_elem *e = found_hash_elem_from_spt(faulted_user_page);

  // if faulted_user_addr is not in SPT
  if(e == NULL)
  {
    return load_page_new(faulted_user_page, true);
  }
  // page is in SPTE
  struct spte* spte_target = hash_entry(e, struct spte, elem);
  //(1) wait_for_loading flag is true -> lazy loading from the code
  //(2) SWAPED in, bring it into memory again
  if (spte_target->wait_for_loading)
  {
    printf("load_page_for_write: loading executable faultaddr=%0x\n", faulted_user_addr);
    return loading_from_executable(spte_target);
  }
  else
  {
    printf("load_page_for_write: SWAPfaultaddr=%0x\n", faulted_user_addr);

    //given address is not waiting for loading => just swap in
    if(pagedir_get_page(thread_current()->pagedir, spte_target->user_addr))
    {
       PANIC("Serious Problem\n");
    }
    if(!load_page_swap(spte_target))
    {
      return false;
    }
  }
  return true;
}

/************************************************************************
* FUNCTION : load_page_for_read                                         *
* Input : faulted_user_addr                                             *
* Output : Success or FAIL                                              *
* Purpose : load page of faulted_user_addr  for reading                 *
************************************************************************/
int load_page_for_read(void* faulted_user_addr)
{
  // printf("load_page_for_read: faultaddr=%0x\n", faulted_user_addr);

  //get the spte for this addr
  void* faulted_user_page = pg_round_down(faulted_user_addr);
  struct hash_elem *e = found_hash_elem_from_spt(faulted_user_page);

  //For reading memery we don't have to allocate new frame.
  if(e == NULL)
    return false;
  // page is in SPTE
  //first find spte_target from the spte of thread
  struct spte* spte_target = hash_entry(e, struct spte, elem);
  //(1) wait_for_loading flag is true -> lazy loading from the code
  if (spte_target->wait_for_loading)
  { 
    // printf("load_page_for_read: loading executable faultaddr=%0x\n", faulted_user_addr);
    return loading_from_executable(spte_target);
  }
  //(2)not waiting for loading, Swap in 
  else
  {
    // printf("load_page_for_read: SWAP faultaddr=%0x\n", faulted_user_addr);
    if(pagedir_get_page(thread_current()->pagedir, spte_target->user_addr))
    {
      PANIC("Serious Problem\n");
    }
    if(!load_page_swap(spte_target))
    {
      return false;
    }
  }
  return true;
}

int load_page_new(void* user_page_addr, bool writable)
{
  //Create new spte
  struct spte * new_spte = create_new_spte_insert_to_spt(user_page_addr);
  if(new_spte == NULL) return false;

  //Additional initialization (incuding allocating framd and install page) 
  new_spte->writable = writable;
  void* new_frame = frame_allocate(new_spte);
  new_spte->phys_addr = new_frame;
  if(install_page(user_page_addr, new_frame, writable) == false)
  {
    frame_free(new_frame);
    spte_free(new_spte);
    return false;
  }
  new_spte->frame_locked = false;
  new_spte->present = true;
  return true;
}


int load_page_file(uint8_t* user_page_addr, struct file *file, off_t ofs,
		   uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable)
{
  //Create new spte
  struct spte * new_spte = create_new_spte_insert_to_spt(user_page_addr);
  if(new_spte == NULL) return false;

  //Additional initialization (incuding allocating framd and install page) 
  new_spte->writable = writable;
  uint8_t* new_frame = (uint8_t*)frame_allocate(new_spte);

  //Load from file
  if(file_read(file, new_frame, page_read_bytes) != (int) page_read_bytes)
  {
    printf("FILE READ FAIL\n");
    frame_free((void*)new_frame);
    spte_free(new_spte);
    return false;
  }
  memset(new_frame + page_read_bytes, 0, page_zero_bytes);
  if(install_page(user_page_addr, new_frame, writable) == false)
  {
    frame_free((void*)new_frame);
    spte_free(new_spte);
    return false;
  }
  new_spte->frame_locked = false;
  new_spte->present = true;
  return true;
}

int load_page_file_lazy(uint8_t* user_page_addr, struct file *file, off_t ofs,
       uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable)
{
  // create new spte
  struct spte * new_spte = create_new_spte_insert_to_spt(user_page_addr);
  if(new_spte == NULL) return false;

  //additional initialization
  //wait_for_loading flag is important
  new_spte->present = false;
  new_spte->wait_for_loading = true;
  new_spte->writable = writable;

  //for loading
  new_spte->loading_info.page_read_bytes = page_read_bytes;
  new_spte->loading_info.page_zero_bytes = page_zero_bytes;
  new_spte->loading_info.ofs = ofs;
  new_spte->loading_info.executable = file;

  new_spte->frame_locked = false;
  return true;
}

int 
loading_from_executable(struct spte* spte_target)
{
  //given address if waiting for loading. find elf  and allocate frame, read data from the disk to memory.
  // printf("loading_from_executable START\n");

  struct file *executable = thread_current()->executable;
  if (executable != spte_target->loading_info.executable)
    PANIC("EXECUTABLE ERROR\n");

  uint8_t* new_frame = (uint8_t *)frame_allocate(spte_target);

  //changing wait_for_loading flag and initialize values;
  size_t page_read_bytes = spte_target->loading_info.page_read_bytes;
  size_t page_zero_bytes = spte_target->loading_info.page_zero_bytes;
  off_t ofs = spte_target->loading_info.ofs;
  bool writable = spte_target->writable;

  //reading
  file_seek (executable, ofs);
  if(file_read(executable, new_frame, page_read_bytes) != (int) page_read_bytes)
  {
    // printf("FILE READ FAIL\n");
    PANIC("FILE READ FAIL\n");
    frame_free((void*)new_frame);
    return false;
  }
  //set rest of bits to zero 
  memset(new_frame + page_read_bytes, 0, page_zero_bytes);
  //install the page in user page table
  if(install_page( spte_target->user_addr, new_frame, writable) == false)
  {
    printf("INSTALL PAGE IN LOADING FROM EXEC: FAIL\n");
    PANIC("INSTALL PAGE IN LOADING FROM EXEC: FAIL\n");
    frame_free((void*)new_frame);
    return false;
  }
  spte_target->wait_for_loading = false;
  spte_target->present = true;
  // printf("loading_from_executable END\n");
  return true;
}

int load_page_swap(struct spte* spte_target)
{
  //printf("load_page_swap: init\n");
  bool writable = spte_target->writable;
 
  if(spte_target->present == false)
  {
    // the page is in swap space. bring it in
    void* new_frame = frame_allocate(spte_target);
    swap_remove((char*)new_frame, spte_target->swap_idx);
    if (spte_target->fte->supplement_page != spte_target)
      PANIC("frame alloc fail in load page swap\n");
    install_page(spte_target->user_addr, spte_target->phys_addr, writable);
    spte_target->present = true;
  }
  else
  {
    PANIC("SOMETHING CANNOT HAPPEN IN LOAD_PAGE_SWAP\n");
  }
  spte_target->frame_locked = false;
  return true;
}

/************************************************************************
* FUNCTION : create_new_spte_insert_to_spt                              *
* Input : user_addr                                                     *
* Purpose : allocate new spt entry , initialize basic elements and      *
* insert to supplement hash supplement hash table                       *
************************************************************************/
struct spte* 
create_new_spte_insert_to_spt(void *user_addr)
{
  struct spte* new_spte = (struct spte*)malloc(sizeof(struct spte));
  struct hash *spt = &thread_current()->spt;
  if(new_spte == NULL) return NULL;
  new_spte->user_addr = user_addr;
  new_spte->status = ON_MEM;
  new_spte->present = false;
  new_spte->dirty = false;
  new_spte->swap_idx = -1;
  new_spte->wait_for_loading = false;
  new_spte->frame_locked = false;
  //insert
  hash_insert(spt, &(new_spte->elem));
  return new_spte;
}

/************************************************************************
* FUNCTION : found_hash_elem_from_spt                                   *
* Input : faulted_user_page                                             *
* Output : hash_elem we found (else NULL)                               *
* Purpose : find hash elem with faulted_user_page and return it         *
************************************************************************/
struct hash_elem* 
found_hash_elem_from_spt(void *faulted_user_page)
{
  //get the spte for this addr
  struct hash *spt = &thread_current()->spt;
  // find the spte with infos above(traverse spt)
  struct spte spte_temp;
  struct hash_elem *e;
  spte_temp.user_addr = faulted_user_page;
  e = hash_find(spt, &spte_temp.elem);
  return e;
}

void 
stack_growth(void *user_addr)
{
  void* new_stack_page = pg_round_down(user_addr);
  load_page_for_write(new_stack_page);
}

void 
spte_free(struct spte* spte_to_free)
{
  struct hash *spt = &thread_current()->spt;
  // find the spte with infos above(traverse spt)
  struct hash_elem *e = hash_find(spt, &spte_to_free->elem);
  hash_delete(spt, e);
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
