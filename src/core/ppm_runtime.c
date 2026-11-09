#include "core/ppm_runtime.h"

#include "common/ppm_memory.h"

struct PpmRuntime
{
    PpmRuntimeState state;

    /*
     * Non-owning backend pointer.
     *
     * The runtime controls backend open/scan/close lifecycle,
     * but it does not allocate or free the backend object itself.
     */
    PpmBackend *backend;
};

PpmResult ppm_runtime_create(PpmRuntime **runtime)
{
    if (!runtime)
        return PPM_INVALID_ARGUMENT;

    *runtime = NULL;

    PpmRuntime *rt = ppm_calloc(1,sizeof(*rt));

    if (!rt)
        return PPM_OUT_OF_MEMORY;

    rt->state = PPM_RUNTIME_STATE_INITIALIZED;
    rt->backend = NULL;

    *runtime = rt;

    return PPM_OK;
}

PpmResult ppm_runtime_set_backend(
    PpmRuntime *runtime,
    PpmBackend *backend)
{
    if (!runtime || !backend)
        return PPM_INVALID_ARGUMENT;

    runtime->backend = backend;

    return PPM_OK;
}

PpmResult ppm_runtime_scan(PpmRuntime *runtime)
{
    if (!runtime)
        return PPM_INVALID_ARGUMENT;

    if (!runtime->backend)
        return PPM_UNSUPPORTED;

    PpmResult r = ppm_backend_open(runtime->backend);

    if (r != PPM_OK)
        return r;

    r = ppm_backend_scan(runtime->backend);

    if (r != PPM_OK)
        return r;

    runtime->state = PPM_RUNTIME_STATE_SCANNED;

    return PPM_OK;
}

PpmRuntimeState ppm_runtime_state(const PpmRuntime *runtime)
{
    if (!runtime)
        return PPM_RUNTIME_STATE_EMPTY;

    return runtime->state;
}

void ppm_runtime_destroy(PpmRuntime *runtime)
{
    if (!runtime)
        return;

    if (runtime->backend)
        ppm_backend_close(runtime->backend);

    runtime->state = PPM_RUNTIME_STATE_SHUTDOWN;

    ppm_free(runtime);
}
