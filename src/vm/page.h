#include <hash.h>
enum spte_status{

  ON_SWAP,
  ON_MEM,
  ON_DISK,
};
struct fte;
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

struct fte {
  void* frame_addr;
  struct list_elem elem;
  struct spte supplement_page;
};
