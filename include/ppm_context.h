#ifndef PPM_CONTEXT_H
#define PPM_CONTEXT_H

#include <pipewire/pipewire.h>
#include <spa/utils/hook.h>

typedef struct PpmContext
{
    struct pw_main_loop *loop;
    struct pw_context   *context;
    struct pw_core      *core;
    struct pw_registry  *registry;

    struct spa_hook core_listener;
    struct spa_hook registry_listener;

    int pending_seq;

} PpmContext;

int  ppm_context_open(PpmContext *ctx);
int  ppm_context_roundtrip(PpmContext *ctx);
void ppm_context_close(PpmContext *ctx);

#endif
