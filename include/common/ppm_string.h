#ifndef PPM_STRING_H
#define PPM_STRING_H

#include "ppm_base.h"
#include "ppm_result.h"

typedef struct
{
    char   *data;
    size_t  size;
    size_t  capacity;
} PpmString;

PpmResult ppm_string_init(PpmString *s);

void ppm_string_destroy(PpmString *s);

PpmResult ppm_string_assign(
    PpmString *s,
    const char *text);

PpmResult ppm_string_append(
    PpmString *s,
    const char *text);

const char *ppm_string_c_str(
    const PpmString *s);

size_t ppm_string_size(
    const PpmString *s);

void ppm_string_clear(
    PpmString *s);

#endif
