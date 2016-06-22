#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "filesys/cache.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);
static void initialize_root_dir(void);


//added internal functions proj4-3
struct dir* filesys_navigate_to_target(const char *path, char *name);



/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  filesys_disk = disk_get (0, 1);
  if (filesys_disk == NULL)
    PANIC ("hd0:1 (hdb) not present, file system initialization failed");

  inode_init ();
  buffer_cache_init();
  free_map_init ();
  if (format) 
    do_format ();
  free_map_open ();
  initialize_root_dir();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  disk_sector_t inode_sector = 0;
  char namebuf[64];
  struct dir *dir = filesys_navigate_to_target (name, namebuf);
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, namebuf, inode_sector, false));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

bool
filesys_create_directory (const char *name) 
{
  //printf("create_dir init: %s\n", name);
  char namebuf[64];
  disk_sector_t inode_sector = 0;
  struct dir *dir = filesys_navigate_to_target(name, namebuf);
  
  //printf("targetdir: %d\n",dir_get_inum_of_dir(dir));

  //printf("namebuf: %s\n", namebuf);

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && dir_create (inode_sector, 16)
                  && dir_add (dir, namebuf, inode_sector, true));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  
  if(success)
    {
      //add . and .. to created dir
      struct inode* newinode;
      struct dir* newdir;
      newinode = inode_open(inode_sector);
      newdir = dir_open(newinode);
      dir_add(newdir, ".", inode_sector, true);
      dir_add(newdir, "..", dir_get_inum_of_dir(dir), true);   
    }
  dir_close (dir);
  

  return success;
}



/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  //printf("%s\n", name);
  char namebuf[64];
  bool isdir;
  struct dir *dir = filesys_navigate_to_target (name, namebuf);
  struct inode *inode = NULL;
  //printf("dir: %d\n", dir);
  //printf("root inum: %d\n", dir_get_inum_of_dir(dir));
  if (dir != NULL)
    dir_lookup (dir, namebuf, &inode, &isdir);
  dir_close (dir);

  if(isdir != true)
    return file_open(inode);
  else
    return NULL;
}


/* Similar to filesys_open, but opens directories*/
struct dir *
filesys_open_directory (const char *name)
{
  char namebuf[64];
  bool isdir;
  struct dir *dir = filesys_navigate_to_target (name, namebuf);
  struct inode *inode = NULL;
  if (dir != NULL)
    dir_lookup (dir, namebuf, &inode, &isdir);
  dir_close (dir);

  if(isdir == true)
    return dir_open(inode);
  else
    return NULL;
}



/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  char namebuf[64];
  struct dir *dir = filesys_navigate_to_target(name, namebuf);
  struct inode *inode = NULL;
  bool success = false;
  bool isdir;
  if (dir != NULL)
    dir_lookup (dir, namebuf, &inode, &isdir);
  else return false;

  if(inode != NULL)
    {
      if(isdir == true)
	{
	  // the inode is a directory; we must tread carefully
	  struct dir* dir_to_remove = dir_open(inode);
	  if(dir_is_empty(dir_to_remove) == true)
	    {
	      // safe to remove directory
	      dir_close(dir_to_remove);
	      success = dir_remove(dir, namebuf);
	    }
	  else
	    {
	      // not safe to remove.
	      dir_close(dir_to_remove);
	      success = false;
	    }
	}
      else
	{
	  // remove the file
	  //printf("filesys_remove: remove the file %s\n", namebuf);
	  inode_close(inode);
	  success = dir_remove (dir, namebuf); 
	}
    }
  else return false;
  
  dir_close(dir);
  
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

struct dir* filesys_navigate_to_target(const char *path, char *name)
{

  //printf("nav_to_target: init path= %s\n", path);

  char parse_buf[64];
  strlcpy(parse_buf, path, 63);

  struct dir *curdir = NULL;
  struct dir *target_dir = NULL;

  char* strtok_r_ptr;
  char* token_ptr = NULL;
  char* token_ptr_next = NULL;



  if(parse_buf[0] == '/' || dir_get_pwd() == NULL)
    {
      //start from root
      //printf("navi: start from root\n");
      curdir = dir_open_root();
      target_dir = curdir;
    }
  else
    {
      //start from pwd
      //printf("navi: start from pwd");
      curdir = dir_reopen(dir_get_pwd());
      target_dir = curdir;
    }

  if((token_ptr = strtok_r(parse_buf, "/", &strtok_r_ptr)) == NULL)
    {
      dir_close(curdir);
      return NULL;
    }
  else
    {
      token_ptr_next = strtok_r(NULL, "/", &strtok_r_ptr);

    }
  //printf("token_ptr: %s, token_ptr_next: %s\n", token_ptr, token_ptr_next);


  while(token_ptr_next != NULL)
    {
      struct inode *inode;
      bool isdir;

      
      
      // lookup the  token
      if(dir_lookup(curdir, token_ptr, &inode, &isdir)
	 == true)
	{
	  if(isdir == true)
	    {
	      // close the old and open the new directory
	      dir_close(curdir);
	      curdir = dir_open(inode);
	    }
	  else
	    {
	      //the entry is a file, but not the end of path string
	      //this is invalid
	      inode_close(inode);
	      dir_close(curdir);
	      return NULL;
	    }
	  
	  
	}
      else
	{
	  // no such entry
	  dir_close(curdir);
	  return NULL;
	}
      
      
      
      target_dir = curdir;
      token_ptr = token_ptr_next;
      token_ptr_next = strtok_r(NULL, "/", &strtok_r_ptr);
      
    }
  
  
  //copy last path name
  strlcpy(name, token_ptr, 63);
  
  return target_dir;



}


/*
bool filesys_chdir(char *dir)
{
  struct dir* temp = thread_current()->pwd;
  struct dir* temp2;
  if((temp2 = filesys_open_directory(dir)) == NULL)
    {
      return false;
    }
  else
    {
      thread_current()->pwd = temp2;
      dir_close(temp);
      return true;
    }
}
*/


static void initialize_root_dir(void)
{
  struct dir* rootdir;
  rootdir = dir_open_root();
  dir_add(rootdir, ".", ROOT_DIR_SECTOR, true);
  dir_add(rootdir, "..", ROOT_DIR_SECTOR, true);   
  
  
}
