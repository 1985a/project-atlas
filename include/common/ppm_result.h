#ifndef PPM_RESULT_H
#define PPM_RESULT_H

#include "ppm_base.h"

typedef enum
{
    PPM_OK = 0,

    PPM_ERROR,

    PPM_INVALID_ARGUMENT,

    PPM_NOT_FOUND,

    PPM_ALREADY_EXISTS,

    PPM_UNSUPPORTED,

    PPM_OUT_OF_MEMORY,

    PPM_BACKEND_ERROR,

    PPM_IO_ERROR

} PpmResult;

#endif
