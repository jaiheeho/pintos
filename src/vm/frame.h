#include <hash.h>
#include "vm/page.h"

void frame_table_init();
void frame_table_free();
void* frame_allocate(struct spte*);
int frame_evict(struct spte *);
