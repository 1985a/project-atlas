#include <stdio.h>
#include <string.h>

#include <pipewire/pipewire.h>

#include "ppm_context.h"
#include "ppm_registry.h"

static void
registry_global(
    void *data,
    uint32_t id,
    uint32_t permissions,
    const char *type,
    uint32_t version,
    const struct spa_dict *props)
{
    (void)data;
    (void)permissions;

    ppm_registry_parse(
        id,
        version,
        type,
        props);
}

static const struct pw_registry_events registry_events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = registry_global,
};

static void
core_done(
    void *data,
    uint32_t id,
    int seq)
{
    PpmContext *ctx = data;

    if (id != PW_ID_CORE)
        return;

    if (seq != ctx->pending_seq)
        return;

    pw_main_loop_quit(ctx->loop);
}

static const struct pw_core_events core_events = {
    PW_VERSION_CORE_EVENTS,
    .done = core_done,
};

int
ppm_context_open(PpmContext *ctx)
{
    memset(ctx,0,sizeof(*ctx));

    pw_init(NULL,NULL);

    ctx->loop = pw_main_loop_new(NULL);
    if (!ctx->loop)
        return -1;

    ctx->context =
        pw_context_new(
            pw_main_loop_get_loop(ctx->loop),
            NULL,
            0);

    if (!ctx->context)
        return -1;

    ctx->core =
        pw_context_connect(
            ctx->context,
            NULL,
            0);

    if (!ctx->core)
        return -1;

    pw_core_add_listener(
        ctx->core,
        &ctx->core_listener,
        &core_events,
        ctx);

    ctx->registry =
        pw_core_get_registry(
            ctx->core,
            PW_VERSION_REGISTRY,
            0);

    if (!ctx->registry)
        return -1;

    pw_registry_add_listener(
        ctx->registry,
        &ctx->registry_listener,
        &registry_events,
        ctx);

    return 0;
}

int
ppm_context_roundtrip(PpmContext *ctx)
{
    ctx->pending_seq =
        pw_core_sync(
            ctx->core,
            PW_ID_CORE,
            0);

    return pw_main_loop_run(ctx->loop);
}

void
ppm_context_close(PpmContext *ctx)
{
    spa_hook_remove(&ctx->registry_listener);
    spa_hook_remove(&ctx->core_listener);

    if (ctx->registry)
        pw_proxy_destroy((struct pw_proxy *)ctx->registry);

    if (ctx->core)
        pw_core_disconnect(ctx->core);

    if (ctx->context)
        pw_context_destroy(ctx->context);

    if (ctx->loop)
        pw_main_loop_destroy(ctx->loop);

    pw_deinit();
}
