#include <hash.h>
#include "vm/frame.h"


typedef enum {
  ON_SWAP;
  LOADED;
  ON_DISK;

} status;

struct spte {

  spte_status status;
  void* user_addr;
  void* phys_addr;
  bool present;
  bool dirty;


  struct hash_elem elem;

}
