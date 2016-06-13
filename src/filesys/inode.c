#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"
#include "devices/disk.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define INDIRECT_MAX_SIZE 0x00004000 /* 64KB*/
#define MAX_SIZE          0x00800000 /* 8MB*/
/* On-disk inode.
   Must be exactly DISK_SECTOR_SIZE bytes long. */
// struct inode_disk
//   {
//     disk_sector_t start;                /* First data sector. */
//     off_t length;                        File size in bytes. 
//     unsigned magic;                     /* Magic number. */
//     uint32_t unused[125];               /* Not used. */
//   };

struct inode_disk
{
  struct inode_disk *links[128];
};


/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };
bool inode_free_map_allocate(size_t, struct inode_disk *);
void inode_free_map_release(size_t, struct inode_disk *);
void inode_free_disk_inode(struct inode_disk *disk_inode);

/* Returns the disk sector that contains byte offset POS within
   INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
// static disk_sector_t
// byte_to_sector (const struct inode *inode, off_t pos) 
// {
//   ASSERT (inode != NULL);
//   if (pos < inode->data.length)
//     return inode->data.start + pos / DISK_SECTOR_SIZE;
//   else
//     return -1;
// }


static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  int length = (int) inode->data.links[0]->links[0]->links[0];
  // printf("in byte_to_sector :pos : %d, length%d\n",pos, length);
  if (pos < length)
  {
    length = pos / DISK_SECTOR_SIZE + 1;
    int double_indirect_size = length / INDIRECT_MAX_SIZE ;
    int indirect_size = (length % INDIRECT_MAX_SIZE) / (DISK_SECTOR_SIZE/4) ;
    int direct_size = (length % INDIRECT_MAX_SIZE) % (DISK_SECTOR_SIZE/4) ;
    // printf("byte_to_sector: %u\n", (disk_sector_t)inode->data.links[double_indirect_size]->links[indirect_size]->links[direct_size]);
    return (disk_sector_t)inode->data.links[double_indirect_size]->links[indirect_size]->links[direct_size];
  }
  else
  {
    // printf("hererer1\n");
    return -1;
  }
}

static disk_sector_t
byte_to_sector_disk (const struct inode_disk *disk_inode, off_t pos) 
{
  ASSERT (disk_inode != NULL);
  int length = (int) disk_inode->links[0]->links[0]->links[0];
  // printf("in byte_to_sector_disk :pos : %d, length%d\n",pos, length);
  if (pos < length)
  {
    length = pos / DISK_SECTOR_SIZE + 1;
    int double_indirect_size = length / INDIRECT_MAX_SIZE ;
    int indirect_size = (length % INDIRECT_MAX_SIZE) / (DISK_SECTOR_SIZE/4) ;
    int direct_size = (length % INDIRECT_MAX_SIZE) % (DISK_SECTOR_SIZE/4) ;

    // printf("length : %d, double_indirect_size: %d, indirect_size; %d, direct_size:%d \n",
      // length, double_indirect_size, indirect_size, direct_size);

    // printf("byte_to_sector_disk: %u\n", 
      // (disk_sector_t)disk_inode->links[double_indirect_size]->links[indirect_size]->links[direct_size]);
    return (disk_sector_t)disk_inode->links[double_indirect_size]->links[indirect_size]->links[direct_size];
  }
  else
  {
    // printf("hererer2\n");
    return -1;
  }
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   disk.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length)
{
  struct inode_disk * disk_inode = NULL;

  bool success = false;

  ASSERT (length >= 0);
  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);

  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      // printf("length of inode :%d , sectors: %d\n", length, sectors);
      success = inode_free_map_allocate (sectors, disk_inode);
      disk_inode->links[0]->links[0]->links[0] = length;
      // printf("length of inode :%d , sectors: %d\n", length, sectors);
      // printf("inode sector : %d\n", sector);
      if (success)
      {
        static char zeros[DISK_SECTOR_SIZE];
        size_t i;
        if (buffer_cache_inited == false)
        {
          disk_write (filesys_disk, sector, disk_inode);

          struct inode_disk bounce;
          disk_read (filesys_disk, sector, &bounce);
          // printf("test: %d\n", bounce.links[0]->links[0]->links[0]);

          if (sectors > 0) 
            {
              for (i = 0; i < sectors; i++) 
              {
                disk_write (filesys_disk, byte_to_sector_disk(disk_inode, (off_t)i*DISK_SECTOR_SIZE), zeros); 
              }
            }
          success = true; 
        }
        else
        {
          buffer_cache_write(sector, (char *)disk_inode , DISK_SECTOR_SIZE, 0);
          size_t i;
          for (i = 0; i < sectors; i++) 
            buffer_cache_write(byte_to_sector_disk(disk_inode, (off_t)i*DISK_SECTOR_SIZE), zeros , DISK_SECTOR_SIZE, 0);

          success = true;
        }
      } 
      // inode_free_disk_inode(disk_inode);
      // if (free_map_allocate (sectors, &disk_inode->start))
      // {
      //   static char zeros[DISK_SECTOR_SIZE];
      //   size_t i;
      //   if (buffer_cache_inited == false)
      //   {
      //     disk_write (filesys_disk, sector, disk_inode);
      //     if (sectors > 0) 
      //       {
      //         for (i = 0; i < sectors; i++) 
      //           disk_write (filesys_disk, disk_inode->start + i, zeros); 
      //       }
      //     success = true; 
      //   }
      //   else
      //   {
      //     buffer_cache_write(sector, (char *)disk_inode , DISK_SECTOR_SIZE, 0);
      //     size_t i;
      //     for (i = 0; i < sectors; i++) 
      //       buffer_cache_write(disk_inode->start + i, zeros , DISK_SECTOR_SIZE, 0);

      //     success = true;
      //   }
      // } 
      // free (disk_inode);
    }

  return success;
}

bool inode_free_map_allocate(size_t length, struct inode_disk *disk_inode)
{
  length = length + 1;
  int double_indirect_size = length / INDIRECT_MAX_SIZE + 1;
  int indirect_size = (length % INDIRECT_MAX_SIZE) / (DISK_SECTOR_SIZE/4) + 1;
  int direct_size = (length % INDIRECT_MAX_SIZE) % (DISK_SECTOR_SIZE/4);

  // printf("length : %d, double_indirect_size: %d, indirect_size; %d, direct_size:%d \n",
  //   length, double_indirect_size, indirect_size, direct_size);

  struct inode_disk * direct = NULL;
  struct inode_disk * indirect = NULL;
  struct inode_disk * double_indirect = NULL;

  int i,j,k;
  ASSERT(double_indirect_size <= DISK_SECTOR_SIZE/4);
  ASSERT(indirect_size <= DISK_SECTOR_SIZE/4);
  ASSERT(direct_size <= DISK_SECTOR_SIZE/4);

  for (i = 0; i < double_indirect_size-2; i ++)
  {
    double_indirect = calloc (1, sizeof (struct inode_disk));
    if (!double_indirect)
      return false;
    disk_inode->links[i] = double_indirect;

    for (j=0; j < DISK_SECTOR_SIZE/4-1; j++)
    {
      indirect = calloc (1, sizeof (struct inode_disk));
      if (!indirect)
        return false;
      double_indirect->links[j] = indirect;
      for (k = 0; k < DISK_SECTOR_SIZE/4-1; k++)
      {
        if( i == 0 && j == 0 && k == 0)
          continue;
        if(!free_map_allocate(1,(disk_sector_t *)&indirect->links[k]))
          return false;
        // printf("alloced : %u\n", (disk_sector_t)indirect->links[k]);
      }
    }
  }

  double_indirect = calloc (1, sizeof (struct inode_disk));
  if (!double_indirect)
    return false;
  disk_inode->links[double_indirect_size-1] = double_indirect;

  for (j=0; j < indirect_size-2; j++)
  {
    indirect = calloc (1, sizeof (struct inode_disk));
    if (!indirect)
      return false;
    double_indirect->links[j] = indirect;
    for (k = 0; k < DISK_SECTOR_SIZE/4-1; k++)
    {
      if( double_indirect_size == 1 && j == 0 && k == 0)
        continue;
      if(!free_map_allocate(1,(disk_sector_t *)&indirect->links[k]))
        return false;
      // printf("alloced : %u\n", (disk_sector_t)indirect->links[k]);
    }
  }

  indirect = calloc (1, sizeof (struct inode_disk));
  if (!indirect)
    return false;
  double_indirect->links[indirect_size-1] = indirect;
  if(double_indirect_size == 1 && indirect_size == 1)
    direct_size++;

  for (k = 0; k < direct_size-1; k++)
  {
    if(double_indirect_size == 1 && indirect_size == 1 && k == 0)
      continue;    
    if(!free_map_allocate(1,(disk_sector_t *)&indirect->links[k]))
      return false;
  }

  return true;
}

void inode_free_map_release(size_t length, struct inode_disk *disk_inode)
{
  int double_indirect_size = length / INDIRECT_MAX_SIZE + 1;
  int indirect_size = (length % INDIRECT_MAX_SIZE) / (DISK_SECTOR_SIZE/4)  + 1;
  int direct_size = (length % INDIRECT_MAX_SIZE) % (DISK_SECTOR_SIZE/4) + 1;

  // struct inode_disk * direct = NULL;
  struct inode_disk * indirect = NULL;
  struct inode_disk * double_indirect = NULL;

  int i,j,k;
  ASSERT(double_indirect_size <= DISK_SECTOR_SIZE/4);
  ASSERT(indirect_size <= DISK_SECTOR_SIZE/4);
  ASSERT(direct_size <= DISK_SECTOR_SIZE/4);


  for (i = 0; i < double_indirect_size-2; i ++)
  {
    double_indirect = disk_inode->links[i];
    if (double_indirect)
      for (j=0; j < DISK_SECTOR_SIZE/4-1; j++)
      {
        indirect = (struct inode_disk *)double_indirect->links[i];
        if (indirect)
          for (k = 0; k < DISK_SECTOR_SIZE/4-1; k++)
          {
            if( i == 0 && j == 0 && k == 0)
              continue;
            if (indirect->links[k])
              free_map_release((disk_sector_t)indirect->links[k],1);
          }
      }
  }

  double_indirect = disk_inode->links[double_indirect_size-1];
  if (!double_indirect)
    return;

  for (j=0; j < indirect_size-2; j++)
  {
    indirect = (struct inode_disk *) double_indirect->links[j];
    if (!indirect)
      continue;
    for (k = 0; k < DISK_SECTOR_SIZE/4-1; k++)
    {
      if( double_indirect_size == 1 && j == 0 && k == 0)
        continue;
      if(indirect->links[k])
        free_map_release((disk_sector_t)indirect->links[k],1);
    }
  }

  indirect = double_indirect->links[indirect_size-1];
  if (!indirect)
    return;

  if(double_indirect_size == 1 && indirect_size == 1)
    direct_size++;
  for (k = 0; k < direct_size-1; k++)
  {
    if(double_indirect_size == 1 && indirect_size == 1 && k == 0)
      continue;  
    if (indirect->links[k])
      free_map_release((disk_sector_t)indirect->links[k],1);
  }

}


void inode_free_disk_inode(struct inode_disk *disk_inode)
{
  int length = bytes_to_sectors((off_t)disk_inode->links[0]->links[0]->links[0]);
  int double_indirect_size = length / INDIRECT_MAX_SIZE + 1;
  int indirect_size = (length % INDIRECT_MAX_SIZE) / (DISK_SECTOR_SIZE/4)  + 1;

  // struct inode_disk * direct = NULL;
  struct inode_disk * indirect = NULL;
  struct inode_disk * double_indirect = NULL;

  int i,j;
  ASSERT(double_indirect_size <= DISK_SECTOR_SIZE/4);
  ASSERT(indirect_size <= DISK_SECTOR_SIZE/4);

  for (i = 0; i < double_indirect_size-2; i ++)
  {
    double_indirect = disk_inode->links[i];
    if (double_indirect)
      for (j=0; j < DISK_SECTOR_SIZE/4-1; j++)
      {
        indirect = (struct inode_disk *)double_indirect->links[i];
        if (indirect)
          free(indirect);
      }
      free(double_indirect);
  }

  double_indirect = disk_inode->links[double_indirect_size-1];
  if (!double_indirect)
    return;

  for (j=0; j < indirect_size-2; j++)
  {
    indirect = (struct inode_disk *) double_indirect->links[j];
    if (!indirect)
      free(indirect);
  }
  free(double_indirect);
}

 
 /* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) 
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }
  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  if(!buffer_cache_read(inode->sector, (char *)&inode->data, DISK_SECTOR_SIZE, 0))
    disk_read (filesys_disk, inode->sector, &inode->data);

  // printf("test in open: sector : %d size : %d\n", sector, inode->data.links[0]->links[0]->links[0]);


  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  off_t bytes_read = 0;
  int length = inode_length (inode);
  int inode_left = length;
  /* before close inode flush buffer cache*/

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {

      while (inode_left >0 && buffer_cache_inited)
      {
        disk_sector_t sector_idx = byte_to_sector (inode, bytes_read);
        buffer_cache_elem_free(sector_idx);
        bytes_read += DISK_SECTOR_SIZE;
        inode_left -= DISK_SECTOR_SIZE;
      }
  
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          inode_free_map_release(bytes_to_sectors(length), &inode->data);
          // free_map_release (inode->data.start,
          //                   bytes_to_sectors (inode->data.length)); 
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      /* before directly access to the disk, we will check buffer cache*/
      if (!buffer_cache_read(sector_idx, buffer + bytes_read, chunk_size, sector_ofs))
      {
        if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
          {
            /* Read full sector directly into caller's buffer. */
            disk_read (filesys_disk, sector_idx, buffer + bytes_read); 
          }
        else 
          {
            /* Read sector into bounce buffer, then partially copy
               into caller's buffer. */
            if (bounce == NULL) 
              {
                bounce = malloc (DISK_SECTOR_SIZE);
                if (bounce == NULL)
                  break;
              }
            disk_read (filesys_disk, sector_idx, bounce);
            memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
          }
      }      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      // printf("power1 : offset : %d \n", offset);
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;
      // printf("power2 : sector_idx : %d\n",sector_idx);

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      // printf("power2 : inode_left : %d\n",inode_left);

      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      /* before directly access to the disk, we will check buffer cache*/
      if (!buffer_cache_write(sector_idx, (char *)buffer + bytes_written ,chunk_size, sector_ofs))
      {
        if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          /* Write full sector directly to disk. */
          disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
        }
        else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            disk_read (filesys_disk, sector_idx, bounce);
          else
            memset (bounce, 0, DISK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          disk_write (filesys_disk, sector_idx, bounce); 
        }
      }
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return (off_t) inode->data.links[0]->links[0]->links[0];
}
