#include <stdio.h>
#include <string.h>

#include "ppm_device.h"

PpmDevice ppm_devices[PPM_MAX_DEVICES];
int ppm_device_count = 0;

void ppm_device_add(
    uint32_t id,
    uint32_t version,
    const char *api,
    const char *name,
    const char *description,
    const char *object_path)
{
    if (ppm_device_count >= PPM_MAX_DEVICES)
        return;

    PpmDevice *d = &ppm_devices[ppm_device_count++];

    memset(d, 0, sizeof(*d));

    d->id = id;
    d->version = version;

    if (api) snprintf(d->api,sizeof(d->api),"%s",api);
    if (name) snprintf(d->name,sizeof(d->name),"%s",name);
    if (description) snprintf(d->description,sizeof(d->description),"%s",description);
    if (object_path) snprintf(d->object_path,sizeof(d->object_path),"%s",object_path);
}

void ppm_device_dump(void)
{
    puts("");
    puts("Devices");
    puts("==============================================");

    for (int i=0;i<ppm_device_count;i++) {

        PpmDevice *d=&ppm_devices[i];

        printf("%3u %-12s %-35s\n",
               d->id,
               d->api,
               d->description);
    }
}
