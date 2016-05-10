#include <hash.h>
#include "vm/frame.h"

struct spte {

  void* user_addr;
  void* phys_addr;
  bool present;
  bool dirty;


  struct hash_elem elem;

}
