/* atlas.conf: un proceso largo (el daemon watch) debe ver los cambios que
 * hacen la GUI, la CLI o una edición a mano, sin reiniciarse. */
#include "test_common.h"

#include <stdio.h>
#include <unistd.h>

#include "atlas.h"

static void write_conf(const char *body)
{
    char path[4096], tmp[4096];
    FILE *f;

    atlas_mkdirs(atlas_config_dir());
    snprintf(path, sizeof(path), "%s/atlas.conf", atlas_config_dir());
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    f = fopen(tmp, "w");          /* igual que la GUI: escribir y renombrar */
    fputs(body, f);
    fclose(f);
    rename(tmp, path);
}

static void test_reload_on_change(void)
{
    char path[4096];

    write_conf("output.follow_jack=yes\n");
    TEST_ASSERT_EQ_INT(1, atlas_conf_bool("output.follow_jack"));

    write_conf("output.follow_jack=no\n");
    TEST_ASSERT_EQ_INT(0, atlas_conf_bool("output.follow_jack"));

    /* misma longitud y mismo segundo: debe notarse igual */
    write_conf("output.follow_jack=on\n");
    TEST_ASSERT_EQ_INT(1, atlas_conf_bool("output.follow_jack"));

    /* borrar el archivo vuelve a los valores de fábrica */
    snprintf(path, sizeof(path), "%s/atlas.conf", atlas_config_dir());
    unlink(path);
    TEST_ASSERT_EQ_INT(1, atlas_conf_bool("output.follow_jack"));
    write_conf("output.follow_jack=no\n");
    TEST_ASSERT_EQ_INT(0, atlas_conf_bool("output.follow_jack"));
}

int main(void)
{
    test_reload_on_change();
    return test_summary();
}
