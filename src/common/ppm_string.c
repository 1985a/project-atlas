#include "common/ppm_string.h"
#include "common/ppm_memory.h"

#include <string.h>

static PpmResult ppm_string_reserve_for(PpmString *s,size_t required)
{
    if (!s)
        return PPM_INVALID_ARGUMENT;

    if (required <= s->capacity)
        return PPM_OK;

    size_t cap = s->capacity ? s->capacity : 8;

    while (cap < required)
    {
        if (cap > SIZE_MAX / 2)
            return PPM_OUT_OF_MEMORY;

        cap *= 2;
    }

    char *tmp = ppm_realloc(s->data,cap);

    if (!tmp)
        return PPM_OUT_OF_MEMORY;

    s->data = tmp;
    s->capacity = cap;

    return PPM_OK;
}

PpmResult ppm_string_init(PpmString *s)
{
    if (!s)
        return PPM_INVALID_ARGUMENT;

    memset(s,0,sizeof(*s));

    return PPM_OK;
}

void ppm_string_destroy(PpmString *s)
{
    if (!s)
        return;

    ppm_free(s->data);

    memset(s,0,sizeof(*s));
}

PpmResult ppm_string_assign(PpmString *s,const char *text)
{
    if (!s)
        return PPM_INVALID_ARGUMENT;

    if (!text)
        text = "";

    size_t len = strlen(text);

    if (len == SIZE_MAX)
        return PPM_OUT_OF_MEMORY;

    PpmResult r = ppm_string_reserve_for(s,len + 1);

    if (r != PPM_OK)
        return r;

    memcpy(s->data,text,len + 1);
    s->size = len;

    return PPM_OK;
}

PpmResult ppm_string_append(PpmString *s,const char *text)
{
    if (!s)
        return PPM_INVALID_ARGUMENT;

    if (!text)
        text = "";

    size_t len = strlen(text);

    if (len == 0)
        return PPM_OK;

    if (s->size > SIZE_MAX - len - 1)
        return PPM_OUT_OF_MEMORY;

    size_t new_size = s->size + len;

    PpmResult r = ppm_string_reserve_for(s,new_size + 1);

    if (r != PPM_OK)
        return r;

    memcpy(s->data + s->size,text,len + 1);
    s->size = new_size;

    return PPM_OK;
}

const char *ppm_string_c_str(const PpmString *s)
{
    if (!s || !s->data)
        return "";

    return s->data;
}

size_t ppm_string_size(const PpmString *s)
{
    return s ? s->size : 0;
}

void ppm_string_clear(PpmString *s)
{
    if (!s)
        return;

    s->size = 0;

    if (s->data)
        s->data[0] = '\0';
}
