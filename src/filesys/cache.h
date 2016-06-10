#include <stdio.h>
#include <stdlib.h>


void buffer_cache_init();
int buffer_cache_read(disk_sector_t sector, char* buffer, size_t size, int off);
int buffer_cache_write(disk_sector_t sector, char* buffer, size_t size, int off);

void buffer_cache_elem_free(disk_sector_t sector);
void buffer_cache_free();