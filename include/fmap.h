#ifndef FMAP_H
#define FMAP_H

#include <stddef.h>

void *map_file(char *path, size_t *length, int pm);
void unmap_file(void *addr, size_t length);

#endif