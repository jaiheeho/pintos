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

struct lazy_loading_info{
  uint32_t page_read_bytes;
  uint32_t page_zero_bytes;
  int32_t ofs;
};
/* Supplementary page table entry */
struct spte {
  enum spte_status status;  //deprecated
  void* user_addr;
  void* phys_addr;
  void* fte;
  bool present;
  bool dirty;
  int swap_idx; // swap slot number
  bool writable;
  bool frame_locked;
  struct hash_elem elem;
  bool wait_for_loading;
  struct lazy_loading_info loading_info;
};

/* frame table entry */
struct fte {
  void* frame_addr;
  void* supplement_page;
  struct thread* thread;
  int use;
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
int load_page_file(void*, struct file*, off_t, uint32_t, uint32_t, bool);
int load_page_file_lazy(void*, struct file*, off_t, uint32_t, uint32_t, bool);
#endif
