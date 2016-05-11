#include <hash.h>
#include <vm/frame.h>
enum spte_status{

  ON_SWAP,
  ON_MEM,
  ON_DISK,
};

struct spte {
  enum spte_status status;
  void* user_addr;
  void* phys_addr;
  struct fte frame;
  bool present;
  bool dirty;
  int swap_idx;

  struct hash_elem elem;
};