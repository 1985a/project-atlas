#include "common/ppm_vector.h"
#include "common/ppm_memory.h"

#include <string.h>

PpmResult ppm_vector_init(PpmVector *v)
{
    if (!v)
        return PPM_INVALID_ARGUMENT;

    memset(v,0,sizeof(*v));
    return PPM_OK;
}

void ppm_vector_destroy(PpmVector *v)
{
    if (!v)
        return;

    ppm_free(v->data);
    memset(v,0,sizeof(*v));
}

PpmResult ppm_vector_reserve(PpmVector *v,size_t cap)
{
    if (!v)
        return PPM_INVALID_ARGUMENT;

    if (cap <= v->capacity)
        return PPM_OK;

    if (cap > SIZE_MAX / sizeof(void *))
        return PPM_OUT_OF_MEMORY;

    void **tmp = ppm_realloc(v->data,sizeof(void*) * cap);

    if (!tmp)
        return PPM_OUT_OF_MEMORY;

    v->data = tmp;
    v->capacity = cap;

    return PPM_OK;
}

PpmResult ppm_vector_push(PpmVector *v,void *item)
{
    if (!v)
        return PPM_INVALID_ARGUMENT;

    if (v->size == v->capacity)
    {
        size_t next;

        if (v->capacity == 0)
            next = 8;
        else
        {
            if (v->capacity > SIZE_MAX / 2)
                return PPM_OUT_OF_MEMORY;

            next = v->capacity * 2;
        }

        PpmResult r = ppm_vector_reserve(v,next);

        if (r != PPM_OK)
            return r;
    }

    v->data[v->size++] = item;
    return PPM_OK;
}

void *ppm_vector_pop(PpmVector *v)
{
    if (!v || !v->size)
        return NULL;

    return v->data[--v->size];
}

void ppm_vector_clear(PpmVector *v)
{
    if (v)
        v->size = 0;
}

size_t ppm_vector_size(const PpmVector *v)
{
    return v ? v->size : 0;
}

size_t ppm_vector_capacity(const PpmVector *v)
{
    return v ? v->capacity : 0;
}

bool ppm_vector_empty(const PpmVector *v)
{
    return !v || v->size == 0;
}

void *ppm_vector_at(const PpmVector *v,size_t idx)
{
    if (!v || idx >= v->size)
        return NULL;

    return v->data[idx];
}

void *ppm_vector_front(const PpmVector *v)
{
    return ppm_vector_at(v,0);
}

void *ppm_vector_back(const PpmVector *v)
{
    if (!v || !v->size)
        return NULL;

    return v->data[v->size - 1];
}
