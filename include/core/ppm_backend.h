#ifndef PPM_BACKEND_H
#define PPM_BACKEND_H

#include "common/ppm_result.h"

typedef struct PpmBackend PpmBackend;

typedef struct
{
    const char *name;

    PpmResult (*open)(PpmBackend *backend);
    PpmResult (*scan)(PpmBackend *backend);
    void      (*close)(PpmBackend *backend);

} PpmBackendOps;

struct PpmBackend
{
    const PpmBackendOps *ops;
    void *userdata;
    int is_open;
};

PpmResult ppm_backend_init(
    PpmBackend *backend,
    const PpmBackendOps *ops,
    void *userdata);

const char *ppm_backend_name(
    const PpmBackend *backend);

void *ppm_backend_userdata(
    PpmBackend *backend);

PpmResult ppm_backend_open(
    PpmBackend *backend);

PpmResult ppm_backend_scan(
    PpmBackend *backend);

void ppm_backend_close(
    PpmBackend *backend);

#endif
