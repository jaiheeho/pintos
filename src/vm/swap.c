#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "devices/disk.h"
#include "threads/sync.h"
#include "vm/swap.h"

#define SECTORSINPAGE 8
static uint32_t swap_size;
static struct disk *swap_disk;
static struct semaphore swap_lock;
static struct bitmap *swap_bitmap;

/************************************************************************
* FUNCTION : swap_table_init                                         		    *
* Input : file_name                                                     *
* Output : new process id if successful                                 *
* Purpose : initialize swap-table										*
************************************************************************/
void swap_table_init()
{
	sema_init(&swap_lock,1);
	/* get swap disk */
	swap_disk = disk_get(1,1);
	swap_size = disk_size(swap_disk)/8;
	/* get bitmap for swap slots*/
	swap_bitmap = bitmap_create(swap_size);
	if (swap_bitmap == NULL)
		PANIC ("CANNOT ACHIEVE SWAPTABLE");
}

/************************************************************************
* FUNCTION : swap_init                                         		    *
* Input : file_name                                                     *
* Output : new process id if successful                                 *
* Purpose : initialize swap-table										*
************************************************************************/
int swap_alloc(char *addr){
	sema_down(&swap_lock);
	size_t idx;
	int i;
	int sector_num;
	//scan swap_table which is not allocated and flip the bit first False bit to True
	idx = bitmap_scan_and_flip (&swap_bitmap, 0, 1, False);

	//If swot if full, panic!
	if (idx == BITMAP_ERROR)
		PANIC ("CANNOT ALLOCATE SWAPTABLE");

	//write frame to swap disk , each fram has to be write 8 times.
	for (i = 0; i < SECTORSINPAGE; i++)
	{
		sector_num = idx * SECTORSINPAGE + i;
		disk_write (swap_disk, sector_num, addr + DISK_SECTOR_SIZE);
	}
	sema_up(&swap_lock);
	return idx;
}

/************************************************************************
* FUNCTION : swap_init                                         		    *
* Input : file_name                                                     *
* Output : new process id if successful                                 *
* Purpose : initialize swap-table										*
************************************************************************/
void swap_remove(char *addr, size_t idx){
	sema_down(&swap_lock);
	int i;
	int sector_num;

	//check whether swap index is correct
	if (bitmap_test(swap_disk, idx) == False)
		PANIC ("EMPTY SWAP SPACE");
	//fip the swop talbe from True to False so that make swap-table slot empty
	bitmap_flip(idx);

	//read from the swap_disk and write to frame
	for (i = 0; i < SECTORSINPAGE; i++)
	{
		sector_num = idx * SECTORSINPAGE + i;
		disk_read (swap_disk, sector_num, addr + DISK_SECTOR_SIZE);
	}
	sema_up(&swap_lock);
}

/************************************************************************
* FUNCTION : swap_table_free                                         		    *
* Input : file_name                                                     *
* Output : new process id if successful                                 *
* Purpose : initialize swap-table										*
************************************************************************/
void swap_table_free(){
	//destroty swap_table (bitmap destroy)
	bitmap_destroy(&swap_bitmap);
	//in case of unnecessary lock.
	sema_up(&swap_lock);
}

