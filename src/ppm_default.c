#include <stdio.h>

#include "ppm_context.h"
#include "ppm_default.h"
#include "ppm_exec.h"
#include "ppm_lookup.h"

int
ppm_set_default_sink(const char *name)
{
    PpmNode *node;

    if (!name || name[0] == '\0')
        return -1;

    node = ppm_find_sink(name);
    if (!node) {
        puts("Sink no encontrado.");
        return -1;
    }

    printf("Salida por defecto:\n");
    printf("  %s\n", node->description[0] ?
        node->description : node->name);
    printf("  node=%u\n\n", node->id);

    {
        char id[16];
        snprintf(id, sizeof(id), "%u", node->id);
        return ppm_exec("wpctl", "set-default", id, NULL);
    }
}

int
ppm_set_default_source(const char *name)
{
    PpmNode *node;

    if (!name || name[0] == '\0')
        return -1;

    node = ppm_find_source(name);
    if (!node) {
        puts("Source no encontrado.");
        return -1;
    }

    printf("Entrada por defecto:\n");
    printf("  %s\n", node->description[0] ?
        node->description : node->name);
    printf("  node=%u\n\n", node->id);

    {
        char id[16];
        snprintf(id, sizeof(id), "%u", node->id);
        return ppm_exec("wpctl", "set-default", id, NULL);
    }
}

int
ppm_apply_default_sink(const char *name)
{
    PpmContext context;
    int ret;

    if (!name || name[0] == '\0')
        return -1;

    if (ppm_context_open(&context) != 0) {
        fprintf(stderr,
            "No pude conectar con PipeWire para cambiar el sink.\n");
        return -1;
    }

    ret = ppm_context_roundtrip(&context);
    if (ret != 0) {
        fprintf(stderr,
            "No pude actualizar el registro de PipeWire.\n");
        ppm_context_close(&context);
        return -1;
    }

    ret = ppm_set_default_sink(name);
    ppm_context_close(&context);

    return ret;
}
int
ppm_apply_default_source(const char *name)
{
    PpmContext context;
    int ret;

    if (!name || name[0] == '\0')
        return -1;

    if (ppm_context_open(&context) != 0) {
        fprintf(stderr,
            "No pude conectar con PipeWire para cambiar el source.\n");
        return -1;
    }

    ret = ppm_context_roundtrip(&context);
    if (ret != 0) {
        fprintf(stderr,
            "No pude actualizar el registro de PipeWire.\n");
        ppm_context_close(&context);
        return -1;
    }

    ret = ppm_set_default_source(name);
    ppm_context_close(&context);

    return ret;
}
