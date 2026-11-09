#include <stdio.h>
#include <string.h>

#include <spa/utils/dict.h>
#include <pipewire/keys.h>

#include "ppm_registry.h"
#include "ppm_device.h"
#include "ppm_node.h"
#include "ppm_port.h"

static const char *lookup(const struct spa_dict *props,const char *key)
{
    const char *v=spa_dict_lookup(props,key);
    return v ? v : "";
}

void ppm_registry_parse(
        uint32_t id,
        uint32_t version,
        const char *type,
        const struct spa_dict *props)
{

    if(!strcmp(type,"PipeWire:Interface:Device"))
    {
        ppm_device_add(
            id,
            version,
            lookup(props,PW_KEY_DEVICE_API),
            lookup(props,PW_KEY_DEVICE_NAME),
            lookup(props,PW_KEY_DEVICE_DESCRIPTION),
            lookup(props,PW_KEY_OBJECT_PATH)
        );

        return;
    }

    if(!strcmp(type,"PipeWire:Interface:Node"))
    {
        ppm_node_add(
            id,
            version,
            type,
            lookup(props,PW_KEY_MEDIA_CLASS),
            lookup(props,PW_KEY_NODE_NAME),
            lookup(props,PW_KEY_NODE_DESCRIPTION),
            lookup(props,PW_KEY_DEVICE_ID),
            lookup(props,PW_KEY_CLIENT_ID),
            lookup(props,PW_KEY_FACTORY_NAME),
            lookup(props,PW_KEY_OBJECT_PATH)
        );

        return;
    }



    if(!strcmp(type,"PipeWire:Interface:Link"))
    {
        ppm_link_add(
            id,
            version,
            lookup(props,"link.output.port"),
            lookup(props,"link.input.port")
        );

        return;
    }

    if(!strcmp(type,"PipeWire:Interface:Port"))
    {
        ppm_port_add(
            id,
            version,
            lookup(props,"port.direction"),
            lookup(props,"node.id"),
            lookup(props,"port.name"),
            lookup(props,"port.alias")
        );

        return;
    }
}
