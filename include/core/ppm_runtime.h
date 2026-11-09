#ifndef PPM_RUNTIME_H
#define PPM_RUNTIME_H

#include "common/ppm_result.h"
#include "core/ppm_backend.h"

typedef struct PpmRuntime PpmRuntime;

typedef enum
{
    PPM_RUNTIME_STATE_EMPTY = 0,
    PPM_RUNTIME_STATE_INITIALIZED,
    PPM_RUNTIME_STATE_SCANNED,
    PPM_RUNTIME_STATE_SHUTDOWN
} PpmRuntimeState;

PpmResult ppm_runtime_create(PpmRuntime **runtime);

PpmResult ppm_runtime_set_backend(
    PpmRuntime *runtime,
    PpmBackend *backend);

PpmResult ppm_runtime_scan(PpmRuntime *runtime);

PpmRuntimeState ppm_runtime_state(const PpmRuntime *runtime);

void ppm_runtime_destroy(PpmRuntime *runtime);

#endif
