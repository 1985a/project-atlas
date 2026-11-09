#ifndef PPM_LOOKUP_H
#define PPM_LOOKUP_H

#include "ppm_node.h"

PpmNode *ppm_find_stream(const char *name);
PpmNode *ppm_find_sink(const char *name);
PpmNode *ppm_find_source(const char *name);

#endif
