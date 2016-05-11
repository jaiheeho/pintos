#include <hash.h>

typedef enum {
  ON_SWAP;
  ON_MEM;
  ON_DISK;

} status;

struct spte {

  spte_status status;
  void* user_addr;
  void* phys_addr;
  struct fte frame;
  bool present;
  bool dirty;
  int swap_idx;

  struct hash_elem elem;
}