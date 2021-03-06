#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H
#include <stdbool.h>
#include "devices/disk.h"
void buffer_cache_init(void);
int buffer_cache_read(disk_sector_t , char *, size_t, int);
int buffer_cache_write(disk_sector_t , char *, size_t, int, int);
void buffer_cache_elem_free(disk_sector_t);
void buffer_cache_free(void);
void buffer_cache_flush(void);

extern bool buffer_cache_inited;

#endif
