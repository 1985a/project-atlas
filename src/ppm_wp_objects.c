#include "ppm_wp_objects.h"
#include <stdio.h>

#include <glib.h>
#include <wp/wp.h>

#include "ppm_wp_core.h"

typedef struct
{
    GMainLoop *loop;
    int timed_out;
} ObjData;

static gboolean on_timeout(gpointer data)
{
    ObjData *d = data;
    d->timed_out = 1;
    g_main_loop_quit(d->loop);
    return G_SOURCE_REMOVE;
}

static void on_objects_ready(
        GObject *obj,
        GAsyncResult *res,
        gpointer data)
{
    ObjData *d = data;
    GError *err = NULL;

    if(!wp_object_activate_finish(WP_OBJECT(obj), res, &err))
    {
        g_printerr("ObjectManager activation failed: %s\n",
            err ? err->message : "unknown");

        if(err)
            g_error_free(err);
    }

    g_main_loop_quit(d->loop);
}

void ppm_wp_dump_objects(void)
{
    PpmWpCore ctx;

    if(ppm_wp_core_open(&ctx))
    {
        puts("No pude abrir WirePlumber.");
        return;
    }

    puts("");
    puts("====================================");
    puts("WirePlumber Object Manager");
    puts("====================================");

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    WpObjectManager *om = wp_object_manager_new();

    wp_object_manager_add_interest(
        om,
        WP_TYPE_GLOBAL_PROXY,
        NULL);

    wp_object_manager_request_object_features(
        om,
        WP_TYPE_GLOBAL_PROXY,
        WP_OBJECT_FEATURES_ALL);

    wp_core_install_object_manager(
        WP_CORE(ctx.core),
        om);

    ObjData d = { .loop = loop, .timed_out = 0 };

    wp_object_activate(
        WP_OBJECT(om),
        WP_OBJECT_FEATURES_ALL,
        NULL,
        on_objects_ready,
        &d);

    g_timeout_add_seconds(3, on_timeout, &d);

    g_main_loop_run(loop);

    if(d.timed_out)
        puts("TIMEOUT: ObjectManager no activó en 3 segundos.");
    else
        puts("ObjectManager activado correctamente.");

    g_object_unref(om);
    g_main_loop_unref(loop);

    ppm_wp_core_close(&ctx);
}
