#include "common/ppm_memory.h"

#include <stdlib.h>

static PpmMemoryStats g_stats;

void *ppm_malloc(size_t size)
{
    void *p = malloc(size);

    if (p) {
        g_stats.allocations++;
        g_stats.bytes_allocated += size;
    }

    return p;
}

void *ppm_calloc(size_t count,size_t size)
{
    void *p = calloc(count,size);

    if (p) {
        g_stats.allocations++;
        g_stats.bytes_allocated += count * size;
    }

    return p;
}

void *ppm_realloc(void *ptr,size_t size)
{
    void *p = realloc(ptr,size);

    if (p)
        g_stats.bytes_allocated += size;

    return p;
}

void ppm_free(void *ptr)
{
    if (ptr)
        g_stats.frees++;

    free(ptr);
}

void ppm_memory_reset_stats(void)
{
    g_stats = (PpmMemoryStats){0};
}

PpmMemoryStats ppm_memory_stats(void)
{
    return g_stats;
}
