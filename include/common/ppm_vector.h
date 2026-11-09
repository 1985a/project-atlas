#ifndef PPM_VECTOR_H
#define PPM_VECTOR_H

#include "ppm_base.h"
#include "ppm_result.h"

/*
 * PpmVector
 *
 * Non-owning dynamic array of pointers.
 *
 * The vector owns only the internal pointer array.
 * It does NOT own or free the stored elements.
 */

typedef struct
{
    void **data;
    size_t size;
    size_t capacity;
} PpmVector;

PpmResult ppm_vector_init(PpmVector *v);
void ppm_vector_destroy(PpmVector *v);

PpmResult ppm_vector_reserve(PpmVector *v,size_t capacity);
PpmResult ppm_vector_push(PpmVector *v,void *item);

void *ppm_vector_pop(PpmVector *v);
void ppm_vector_clear(PpmVector *v);

size_t ppm_vector_size(const PpmVector *v);
size_t ppm_vector_capacity(const PpmVector *v);
bool ppm_vector_empty(const PpmVector *v);

void *ppm_vector_at(const PpmVector *v,size_t index);
void *ppm_vector_front(const PpmVector *v);
void *ppm_vector_back(const PpmVector *v);

#endif
