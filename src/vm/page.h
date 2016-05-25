#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "threads/thread.h"

#define STACK_MAX 2000 * PGSIZE
#define STACK_STRIDE 32

struct fte;

/* loading information for each page for lazy loading*/
struct lazy_loading_info{
  size_t page_read_bytes;     //read bytes, zero bytes, offset  and executable file for the page loading
  size_t page_zero_bytes;
  off_t ofs;
  struct file *loading_file;
};

/* Supplementary page table entry */
struct spte {
  void* user_addr;                //user address for spte
  void* phys_addr;                //physical address for spte
  struct fte* fte;                //linking fte 
  bool present;                   //check present bit of user address
  bool dirty;                     //dirty bit
  int swap_idx;                   // swap slot number
  bool writable;                  //writable check for the page
  bool frame_locked;              //requires frame_lock to evade fram -eviction before installation
  struct hash_elem elem;
  bool wait_for_loading;          //lazy loading flag
  bool for_mmap;
  struct lazy_loading_info loading_info;  //lazy loding information

};

/* frame table entry */
struct fte {
  void* frame_addr;             //physical address
  struct spte* supplement_page; //linking spte
  struct thread* thread;        //linking thread
  bool frame_locked;            //check frame-lock for the frame.
  struct list_elem elem;
};

void sup_page_table_init(struct hash*);
void sup_page_table_free(struct hash*);
int load_page_for_write(void*);
int load_page_for_read(void*);
void stack_growth(void*);
int load_page_swap(struct spte*);
int load_page_new(void*, bool);
int load_page_file(uint8_t*, struct file*, off_t, uint32_t, uint32_t, bool);
int load_page_file_lazy(uint8_t*, struct file*, off_t, uint32_t, uint32_t, bool);
struct hash_elem* found_hash_elem_from_spt(void *faulted_user_page);

#endif
