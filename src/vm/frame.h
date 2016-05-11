#include <hash.h>
#include "vm/page.h"

// struct fte {
//   void* frame_addr;
//   struct list_elem elem;
//   struct spte supplement_page;
// };

void frame_table_init();
void frame_table_free();
void* frame_allocate(struct spte *supplement_page);
int frame_evict(struct spte *supplement_page);
