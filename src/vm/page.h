#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "threads/thread.h"

#define STACK_MAX 2000 * PGSIZE
#define STACK_STRIDE 32

enum spte_status{
  ON_SWAP,
  ON_MEM,
  ON_DISK
};

struct fte;

/* loading information for each page for lazy loading*/
struct lazy_loading_info{
  size_t page_read_bytes;
  size_t page_zero_bytes;
  off_t ofs;
  struct file *executable;
};

/* Supplementary page table entry */
struct spte {
  enum spte_status status;  //deprecated
  void* user_addr;
  void* phys_addr;
  struct fte* fte;
  bool present;
  bool dirty;
  size_t swap_idx; // swap slot number
  bool writable;
  bool frame_locked;
  struct hash_elem elem;
  bool wait_for_loading;
  struct lazy_loading_info loading_info;
};

/* frame table entry */
struct fte {
  void* frame_addr;
  struct spte* supplement_page;
  struct thread* thread;
  bool frame_locked;
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
#endif
