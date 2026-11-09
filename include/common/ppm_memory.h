#ifndef PPM_MEMORY_H
#define PPM_MEMORY_H

#include "ppm_base.h"
#include "ppm_types.h"

typedef struct
{
    ppm_u64 allocations;
    ppm_u64 frees;
    ppm_u64 bytes_allocated;
    ppm_u64 bytes_freed;
} PpmMemoryStats;

void *ppm_malloc(size_t size);
void *ppm_calloc(size_t count,size_t size);
void *ppm_realloc(void *ptr,size_t size);
void  ppm_free(void *ptr);

void ppm_memory_reset_stats(void);
PpmMemoryStats ppm_memory_stats(void);

#endif
