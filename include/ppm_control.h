#ifndef PPM_CONTROL_H
#define PPM_CONTROL_H

#include "ppm_node.h"

PpmNode *ppm_find_sink(const char *name);

int ppm_move_prepare(
        const char *stream,
        const char *sink);

#endif
