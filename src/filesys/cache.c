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
static int buffer_cache_clock_head;

void buffer_cache_elem_init(int i);
int buffer_cache_allocate(disk_sector_t sector, int option);
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
  buffer_cache_clock_head = 0;
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
	  iter = buffer_cache_allocate(sector, 1); 
	}
    }
  else
    {
      iter = buffer_cache_allocate(sector, 1);    
    }
  
  memcpy(buffer, (buffer_cache[iter].data + off), size);
  
  sema_up(&(buffer_cache[iter].lock));
  return size;

}



int buffer_cache_write(disk_sector_t sector, char * buffer,
		       size_t size, int off, int option)
{
 
  int iter = 0;
  bool found = false;


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
    if(buffer_cache[iter].occupied != false
       && buffer_cache[iter].sector != INVALID_SECTOR)
  	{
  	  buffer_cache[iter].is_accessed = true;
  	}
    else
  	{
  	  sema_up(&(buffer_cache[iter].lock));
  	  iter = buffer_cache_allocate(sector, option); 
  	}
  }
  else
  {
    iter = buffer_cache_allocate(sector, option);    
  }
  
  memcpy((buffer_cache[iter].data + off), buffer, size);
  buffer_cache[iter].is_accessed = true;
  buffer_cache[iter].is_dirty = true;
  
  sema_up(&(buffer_cache[iter].lock));
  return size;

}


int buffer_cache_allocate(disk_sector_t sector, int option)
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



  if(option == 0)
    {
      memset(buffer_cache[iter].data, 0, DISK_SECTOR_SIZE);
    }
  else if(option == 1)
    {
      //load data to the cache_elem slot
      disk_read(filesys_disk, sector, buffer_cache[iter].data);
    }
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

  //printf("buffer_cache_evict: init, head=%d\n", buffer_cache_clock_head);
  
  int iter = buffer_cache_clock_head;
  int clock_head_stored = buffer_cache_clock_head;
  int choice_of_victim = -1;

  for(;iter < BUFFER_CACHE_MAX; iter = (iter + 1) % BUFFER_CACHE_MAX)
    {

      
      if(iter == clock_head_stored)
	choice_of_victim++;
      
      

      //printf("buffer_cache_evict: iter=%d, victim=%d\n", iter, choice_of_victim);
      //printf("clock_head_stored = %d\n", clock_head_stored);
      if(!sema_try_down(&buffer_cache[iter].lock))
  	{
	  //printf("trydown\n");
  	  continue;
  	}

      if(choice_of_victim == 0)
  	{
  	  if((buffer_cache[iter].is_dirty == false) &&
	     (buffer_cache[iter].is_accessed == false))
	    {
	      break;
	    }
  	  else
	    {     
	      buffer_cache[iter].is_accessed = false;
	      sema_up(&(buffer_cache[iter].lock));
	      continue;
	    }
  	}
      else if(choice_of_victim == 1)
  	{
	  
  	  if((buffer_cache[iter].is_dirty == false))
	    {
	      break;
	    }
  	  else
	    {     
	      sema_up(&(buffer_cache[iter].lock));
	      continue;
	    }
	  
  	}
      else if(choice_of_victim == 2)
	{
	  break;
	}
      else PANIC("buffer_cache_evict: this should not happen\n");

      
    }


  //sema_down(&(buffer_cache[iter].lock));
  //if necessary, write out to disk
  if(buffer_cache[iter].is_dirty == true)
    {
      disk_write(filesys_disk, buffer_cache[iter].sector, buffer_cache[iter].data);
    }


  buffer_cache_clock_head = (iter + 1) % BUFFER_CACHE_MAX;


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
      // if(buffer_cache[iter].is_dirty == true)
      //   {
          disk_write(filesys_disk, buffer_cache[iter].sector, buffer_cache[iter].data);
        // }
	  
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

  if(!sema_try_down(&buffer_cache_global_lock))
    return;

  for(iter = 0; iter < BUFFER_CACHE_MAX; iter++)
  {
    if(buffer_cache[iter].sector != INVALID_SECTOR)
  	{	  
  	  if(sema_try_down(&(buffer_cache[iter].lock)))
    	{ 
    	  //if necessary, write out to disk
    	  if(buffer_cache[iter].is_dirty == true)
    	    {
    	      disk_write(filesys_disk, buffer_cache[iter].sector, buffer_cache[iter].data);
    	    }
    	  sema_up(&buffer_cache[iter].lock);
      }
  	}
  }
  sema_up(&buffer_cache_global_lock);
}


void buffer_cache_flush()
{
  //printf("buffer_cache_flush: init\n");
  int iter = 0;

  sema_up(&buffer_cache_global_lock);

  for(iter = 0; iter < BUFFER_CACHE_MAX; iter++)
    {
      if(buffer_cache[iter].sector != INVALID_SECTOR)
  {   
    sema_up(&(buffer_cache[iter].lock));
    //if necessary, write out to disk
    if(buffer_cache[iter].is_dirty == true)
      {
        disk_write(filesys_disk, buffer_cache[iter].sector, buffer_cache[iter].data);
      }
  }
    }
}



