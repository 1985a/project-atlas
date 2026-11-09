#include "core/ppm_pipewire_backend.h"

#include "common/ppm_memory.h"
#include "ppm_context.h"

typedef struct
{
    PpmContext *context;
} PpmPipeWireBackendData;

static PpmResult pipewire_open(PpmBackend *backend)
{
    if (!backend)
        return PPM_INVALID_ARGUMENT;

    PpmPipeWireBackendData *data =
        ppm_backend_userdata(backend);

    if (!data)
        return PPM_INVALID_ARGUMENT;

    if (data->context)
        return PPM_OK;

    data->context =
        ppm_calloc(1,sizeof(*data->context));

    if (!data->context)
        return PPM_OUT_OF_MEMORY;

    if (ppm_context_open(data->context) != 0)
    {
        ppm_free(data->context);
        data->context = NULL;
        return PPM_BACKEND_ERROR;
    }

    return PPM_OK;
}

static PpmResult pipewire_scan(PpmBackend *backend)
{
    if (!backend)
        return PPM_INVALID_ARGUMENT;

    PpmPipeWireBackendData *data =
        ppm_backend_userdata(backend);

    if (!data || !data->context)
        return PPM_BACKEND_ERROR;

    if (ppm_context_roundtrip(data->context) != 0)
        return PPM_BACKEND_ERROR;

    return PPM_OK;
}

static void pipewire_close(PpmBackend *backend)
{
    if (!backend)
        return;

    PpmPipeWireBackendData *data =
        ppm_backend_userdata(backend);

    if (!data)
        return;

    if (data->context)
    {
        ppm_context_close(data->context);
        ppm_free(data->context);
        data->context = NULL;
    }
}

static const PpmBackendOps pipewire_ops = {
    .name = "pipewire",
    .open = pipewire_open,
    .scan = pipewire_scan,
    .close = pipewire_close,
};

PpmResult ppm_pipewire_backend_init(PpmBackend *backend)
{
    static PpmPipeWireBackendData data;

    data.context = NULL;

    return ppm_backend_init(
        backend,
        &pipewire_ops,
        &data);
}
