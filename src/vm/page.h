#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

#define STACK_MAX 2000 * PGSIZE
#define STACK_STRIDE 32
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
  void* supplement_page;
  int use;
  struct list_elem elem;
};

void sup_page_table_init(struct hash*);
void sup_page_table_free(struct hash*);
int load_page(void*);
int stack_growth(void*);


#endif
