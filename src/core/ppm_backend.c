#include "core/ppm_backend.h"

#include <string.h>

PpmResult ppm_backend_init(
    PpmBackend *backend,
    const PpmBackendOps *ops,
    void *userdata)
{
    if (!backend || !ops || !ops->name)
        return PPM_INVALID_ARGUMENT;

    memset(backend,0,sizeof(*backend));

    backend->ops = ops;
    backend->userdata = userdata;
    backend->is_open = 0;

    return PPM_OK;
}

const char *ppm_backend_name(
    const PpmBackend *backend)
{
    if (!backend || !backend->ops || !backend->ops->name)
        return "";

    return backend->ops->name;
}

void *ppm_backend_userdata(
    PpmBackend *backend)
{
    if (!backend)
        return NULL;

    return backend->userdata;
}

PpmResult ppm_backend_open(
    PpmBackend *backend)
{
    if (!backend || !backend->ops)
        return PPM_INVALID_ARGUMENT;

    if (!backend->ops->open)
        return PPM_UNSUPPORTED;

    PpmResult r = backend->ops->open(backend);

    if (r == PPM_OK)
        backend->is_open = 1;

    return r;
}

PpmResult ppm_backend_scan(
    PpmBackend *backend)
{
    if (!backend || !backend->ops)
        return PPM_INVALID_ARGUMENT;

    if (!backend->ops->scan)
        return PPM_UNSUPPORTED;

    return backend->ops->scan(backend);
}

void ppm_backend_close(
    PpmBackend *backend)
{
    if (!backend || !backend->ops)
        return;

    if (backend->is_open && backend->ops->close)
        backend->ops->close(backend);

    backend->is_open = 0;
}
