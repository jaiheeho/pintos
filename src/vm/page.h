#include <hash.h>
#include "vm/frame.h"
#include "threads/vaddr.h"


#define STACK_MAX 2000 * PGSIZE
#define STACK_STRIDE 32


typedef enum {
  ON_SWAP,
  ON_MEM,
  ON_DISK

} spte_status;

struct spte {

  spte_status status;
  void* user_addr;
  void* phys_addr;
  struct fte frame;
  bool present;
  bool dirty;


  struct hash_elem elem;

};

int load_page(void* user_addr);
int stack_growth(void *user_esp);
