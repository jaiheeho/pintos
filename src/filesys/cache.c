#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "threads/synch.h"
#include "filesys/cache.h"
#include "filesys/filesys.h"

#define BUFFER_CACHE_MAX 64
#define INVALID_SECTOR 0xFFFFFFFF

bool buffer_cache_inited = false;

struct buffer_cache_elem {
  bool occupied;
  disk_sector_t sector;
  bool is_accessed;
  bool is_dirty;
  char data[DISK_SECTOR_SIZE];
  struct semaphore lock;
};

static struct buffer_cache_elem buffer_cache[BUFFER_CACHE_MAX];
static struct semaphore buffer_cache_global_lock;

void buffer_cache_elem_init(int i);
int buffer_cache_allocate(disk_sector_t sector);
int buffer_cache_evict(void);

void buffer_cache_init()
{
  sema_init(&buffer_cache_global_lock, 1);
  sema_down(&buffer_cache_global_lock);
  int i = 0;
  for(i = 0; i < BUFFER_CACHE_MAX; i++)
    {
      buffer_cache_elem_init(i);
      sema_init(&(buffer_cache[i].lock), 1);
    }
  buffer_cache_inited = true;
  sema_up(&buffer_cache_global_lock);
}

void buffer_cache_elem_init(int i)
{
  
  buffer_cache[i].occupied = false;
  buffer_cache[i].is_accessed = false;
  buffer_cache[i].is_dirty = false;
  buffer_cache[i].sector = INVALID_SECTOR;
}

int buffer_cache_read(disk_sector_t sector, char * buffer, size_t size, int off)
{
 
  int iter = 0;
  bool found = false;

  if (!buffer_cache_inited)
    return 0;

  for(iter = 0; iter < BUFFER_CACHE_MAX; iter++)
    {
      if(buffer_cache[iter].sector == sector)
	{
	  found = true;
	  break;
	}
    }

  if(found == true)
    {
      sema_down(&(buffer_cache[iter].lock));
      if(buffer_cache[iter].occupied != false && 
	 buffer_cache[iter].sector != INVALID_SECTOR)
	{
	  buffer_cache[iter].is_accessed = true;
	}
      else
	{
	  sema_up(&(buffer_cache[iter].lock));
	  iter = buffer_cache_allocate(sector); 
	}
    }
  else
    {
      iter = buffer_cache_allocate(sector);    
    }
  
  memcpy(buffer, (buffer_cache[iter].data + off), size);
  
  sema_up(&(buffer_cache[iter].lock));
  return size;

}



int buffer_cache_write(disk_sector_t sector, char * buffer, size_t size, int off)
{
 
  int iter = 0;
  bool found = false;

  if (!buffer_cache_inited)
    return 0;

  for(iter = 0; iter < BUFFER_CACHE_MAX; iter++)
  {
    if(buffer_cache[iter].sector == sector)
  	{
  	  found = true;
  	  break;
  	}
  }

  if(found == true)
  {
    sema_down(&(buffer_cache[iter].lock));
    if(buffer_cache[iter].occupied != false && buffer_cache[iter].sector != INVALID_SECTOR)
  	{
  	  buffer_cache[iter].is_accessed = true;
  	}
    else
  	{
  	  sema_up(&(buffer_cache[iter].lock));
  	  iter = buffer_cache_allocate(sector); 
  	}
  }
  else
  {
    iter = buffer_cache_allocate(sector);    
  }
  
  memcpy((buffer_cache[iter].data + off), buffer, size);
  buffer_cache[iter].is_dirty == true;
  
  sema_up(&(buffer_cache[iter].lock));
  return size;

}


int buffer_cache_allocate(disk_sector_t sector)
{
  sema_down(&buffer_cache_global_lock);

  int iter = 0;
  bool found = false;
  for(iter = 0; iter < BUFFER_CACHE_MAX; iter++)
  {
    if(buffer_cache[iter].occupied == false)
  	{
  	  sema_down(&(buffer_cache[iter].lock));
  	  found = true;
  	  break;
  	}
  }
  if(found == false)
  {
    iter = buffer_cache_evict();
  }

  //load data to the cache_elem slot
  disk_read(filesys_disk, sector, buffer_cache[iter].data);
  
  //initialize cache elem metadata
  
  buffer_cache[iter].occupied = true;
  buffer_cache[iter].is_accessed = false;
  buffer_cache[iter].is_dirty = false;
  buffer_cache[iter].sector = sector;

  sema_up(&buffer_cache_global_lock);
  return iter;
}

int buffer_cache_evict()
{

  int iter = 0;
  bool choice_of_victim = false;
  for(iter = 0; iter < BUFFER_CACHE_MAX; iter++)
  {
    if(sema_try_down(&buffer_cache[iter].lock))
  	{
  	  sema_up(&buffer_cache[iter].lock);
  	  continue;
  	}
    if(choice_of_victim == false)
  	{
  	  if(buffer_cache[iter].is_dirty == true)
	    {
	      continue;
	    }
  	  else
	    {
	      break;
	    }
  	}
    else
  	{
  	  break;
  	}
    if(iter == (BUFFER_CACHE_MAX - 1))
  	{
      choice_of_victim = true;
      break;
  	}
  }
  printf("iter %d\n",iter);
  if (iter == BUFFER_CACHE_MAX)
    iter = 0;
  sema_down(&(buffer_cache[iter].lock));
  //if necessary, write out to disk
  if(buffer_cache[iter].is_dirty == true)
  {
    disk_write(filesys_disk, buffer_cache[iter].sector, buffer_cache[iter].data);
  }
  return iter;

}
void buffer_cache_elem_free(disk_sector_t sector)
{

  int iter = 0;

  sema_down(&buffer_cache_global_lock);

  for(iter = 0; iter < BUFFER_CACHE_MAX; iter++)
    {
      if(buffer_cache[iter].sector == sector)
	{	  
	  sema_down(&(buffer_cache[iter].lock));
	  
	  //if necessary, write out to disk
	  if(buffer_cache[iter].is_dirty == true)
	    {
	      disk_write(filesys_disk, buffer_cache[iter].sector, buffer_cache[iter].data);
	    }
	  
	  buffer_cache_elem_init(iter);
	  sema_up(&buffer_cache[iter].lock);
	  break;
	}
    }
  sema_up(&buffer_cache_global_lock);

}


void buffer_cache_free()
{

  int iter = 0;

  sema_down(&buffer_cache_global_lock);

  for(iter = 0; iter < BUFFER_CACHE_MAX; iter++)
    {
      if(buffer_cache[iter].sector != INVALID_SECTOR)
	{	  
	  sema_down(&(buffer_cache[iter].lock));
	  
	  
	  //if necessary, write out to disk
	  if(buffer_cache[iter].is_dirty == true)
	    {
	      disk_write(filesys_disk, buffer_cache[iter].sector, buffer_cache[iter].data);
	    }
	  sema_up(&buffer_cache[iter].lock);
	}
    }
  sema_up(&buffer_cache_global_lock);

}


