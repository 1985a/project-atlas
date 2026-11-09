#include <stdio.h>

#include <json-glib/json-glib.h>

#include "ppm_metadata.h"
#include "ppm_exec.h"

/*
 * Mueve un stream a otro nodo fijando target.object en los metadatos de
 * PipeWire. El nombre del nodo lo pone la aplicación que reproduce, así que
 * se escapa como JSON y se pasa como argumento, nunca a través de la shell.
 */
int ppm_metadata_set_target(unsigned int stream_node, const char *target_node_name)
{
    JsonBuilder *b = json_builder_new();
    JsonGenerator *g = json_generator_new();
    JsonNode *root;
    gchar *value;
    char id[16];
    int r;

    json_builder_begin_object(b);
    json_builder_set_member_name(b, "name");
    json_builder_add_string_value(b, target_node_name ? target_node_name : "");
    json_builder_end_object(b);
    root = json_builder_get_root(b);
    json_generator_set_root(g, root);
    value = json_generator_to_data(g, NULL);

    snprintf(id, sizeof(id), "%u", stream_node);
    r = ppm_exec("pw-metadata", "-n", "default", id, "target.object", value, NULL);

    g_free(value);
    json_node_unref(root);
    g_object_unref(g);
    g_object_unref(b);
    return r;
}
