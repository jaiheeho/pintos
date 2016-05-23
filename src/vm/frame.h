#include <hash.h>
#include "vm/page.h"

void frame_table_init(void);
void frame_table_free(void);
void frame_table_locking(void);
void frame_table_unlocking(void);
void* frame_allocate(struct spte*);
void frame_evict(void);
void frame_free(struct fte*);
void frame_free_nolock(struct fte* fte_to_free);

