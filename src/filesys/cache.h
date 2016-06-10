#include <stdio.h>
#include <stdlib.h>
#include "devices/disk.h"

void buffer_cache_init(void);
int buffer_cache_read(disk_sector_t, void*, size_t, int);
int buffer_cache_write(disk_sector_t, void*, size_t, int);
void buffer_cache_elem_free(disk_sector_t);
void buffer_cache_free(void);

