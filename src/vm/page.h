#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

enum spte_status{
  ON_SWAP,
  ON_MEM,
  ON_DISK
};
struct fte;

struct spte {
  enum spte_status status;
  void* user_addr;
  void* phys_addr;
  void* fte;
  bool present;
  bool dirty;
  int swap_idx;
  struct hash_elem elem;
};

struct fte {
  void* frame_addr;
  struct list_elem elem;
  void* supplement_page;
};

void sup_page_table_init(struct hash*);
void sup_page_table_free(struct hash*);
int load_page(void*);
int stack_growth(void*);


#endif