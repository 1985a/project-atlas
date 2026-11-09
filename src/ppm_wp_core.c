#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <wp/wp.h>

#include "ppm_wp_core.h"

int ppm_wp_core_available(void)
{
    return 1;
}

int ppm_wp_core_open(PpmWpCore *ctx)
{
    memset(ctx,0,sizeof(*ctx));

    wp_init(WP_INIT_ALL);

    WpCore *core = wp_core_new(NULL,NULL,NULL);

    if(!core)
        return -1;

    if(!wp_core_connect(core))
    {
        g_object_unref(core);
        return -1;
    }

    ctx->core = core;

    return 0;
}

void ppm_wp_core_close(PpmWpCore *ctx)
{
    if(!ctx || !ctx->core)
        return;

    g_object_unref(ctx->core);

    ctx->core=NULL;
}

static void on_ready(
        GObject *obj,
        GAsyncResult *res,
        gpointer data)
{
    GMainLoop *loop=data;
    GError *err=NULL;

    if(!wp_object_activate_finish(
            WP_OBJECT(obj),
            res,
            &err))
    {
        g_printerr("Activation failed: %s\n",
            err ? err->message : "unknown");

        if(err)
            g_error_free(err);
    }

    g_main_loop_quit(loop);
}

void ppm_wp_core_print_info(void)
{
    PpmWpCore ctx;

    if(ppm_wp_core_open(&ctx))
    {
        puts("No pude conectar.");
        return;
    }

    GMainLoop *loop=g_main_loop_new(NULL,FALSE);

    wp_object_activate(
        WP_OBJECT(ctx.core),
        WP_CORE_FEATURE_CONNECTED,
        NULL,
        on_ready,
        loop);

    g_main_loop_run(loop);

    puts("WirePlumber conectado.");

    printf("Remote : %s\n",
        wp_core_get_remote_name(
            WP_CORE(ctx.core)));

    printf("Version: %s\n",
        wp_core_get_remote_version(
            WP_CORE(ctx.core)));

    g_main_loop_unref(loop);

    ppm_wp_core_close(&ctx);
}
