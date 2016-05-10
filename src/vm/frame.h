#include <hash.h>

struct fte {
  void* frame_addr;
  struct hash_elem elem;



};



void frame_table_init(struct hash* frame_table);
void frame_table_free(struct hash* frame_table);
void* frame_allocate();
void frame_free();
