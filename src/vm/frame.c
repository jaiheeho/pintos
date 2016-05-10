#include "userprog/pagedir.h"
#include "threads/palloc.h"
#include "frame.h"



unsigned frame_hash_func(const struct hash_elem *e, void *aux)
{
  struct fte* f = hash_entry(e, struct fte, elem);
  return hash_int(f->frame_addr);
}

bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b,
		     void *aux)
{
  

}

void frame_destroyer_func(struct hash_elem *e, void *aux)
{
  struct fte* f = hash_entry(e, struct fte, elem);
  // free the palloc'd frame
  palloc_free_page(f->frame_addr);
  // free fte
  free(f);
}






// must protect frame table with lock cuz frame table is shared
// across processes

void frame_table_init(struct hash* frame_table)
{
  hash_init(frame_table, frame_hash_func, frame_less_func, NULL);

}

void frame_table_free(struct hash* frame_table)
{
  hash_destroy(frame_table, frame_destroyer_func);

}




void* frame_allocate()
{

  void* new_frame = palloc_get_page(palloc_flags);

  if(new_frame == NULL)
    {
      // all frame slots are full; commence eviction


    }
  else
    {
      // frame allocation succeeded; add to frame table
      struct fte* new_fte_entry = (struct fte*)malloc(sizeof(struct fte));
      // configure elements of fte
      new_fte_entry->frame_addr = new_frame;
      // insert into frame table
      hash_insert(&frame_table, &new_fte_entry->elem);


    }



}

void frame_free(void* frame_to_free)
{
  // free frame table entry
  struct fte fte_temp;
  struct hash_elem *e;
  struct fte* fte_tobefreed;

  fte_temp.frame_addr = frame_to_free;
  e = hash_find(&frame_table, &fte_temp.hash_elem);
  fte_tobefreed = hash_entry(e, struct fte, elem);
  //detach fte from frame table
  hash_delete(&frame_table, &fte_tobefreed->hash_elem);

  // free palloc'd page too? or keep? -> freeing is easier I think
  palloc_free_page(frame_to_free);

  // free malloc'd memory
  free(fte_tobefreed);


}
