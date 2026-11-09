/*
 * Hardware Matching Engine (RFC-0003): identidad estable, coincidencia
 * única / ninguna / ambigua / parcial y registro sin duplicados.
 */
#include <dirent.h>

#include "test_fixture.h"
#include "test_common.h"

#include "atlas.h"
#include "atlas_hwdb.h"

static void make_sysfs(void)
{
    char t[1024];

    /* card0: Sound Blaster Z (PCI) */
    fx_write("sys/devices/pci0000:00/0000:27:00.0/vendor", "0x1102\n");
    fx_write("sys/devices/pci0000:00/0000:27:00.0/device", "0x0012\n");
    fx_write("sys/devices/pci0000:00/0000:27:00.0/subsystem_vendor", "0x1102\n");
    fx_write("sys/devices/pci0000:00/0000:27:00.0/subsystem_device", "0x0010\n");
    fx_mkdirs("sys/bus/pci");
    fx_mkdirs("sys/bus/usb");
    fx_mkdirs("sys/drivers/snd_hda_intel");
    fx_mkdirs("sys/drivers/snd-usb-audio");
    fx_path(t, sizeof(t), "sys/bus/pci");
    fx_symlink(t, "sys/devices/pci0000:00/0000:27:00.0/subsystem");
    fx_path(t, sizeof(t), "sys/drivers/snd_hda_intel");
    fx_symlink(t, "sys/devices/pci0000:00/0000:27:00.0/driver");
    fx_mkdirs("sys/class/sound/card0");
    fx_path(t, sizeof(t), "sys/devices/pci0000:00/0000:27:00.0");
    fx_symlink(t, "sys/class/sound/card0/device");

    /* card3: webcam USB (el device de ALSA es la interfaz; IDs en el padre) */
    fx_write("sys/devices/usb1/1-2/idVendor", "046d\n");
    fx_write("sys/devices/usb1/1-2/idProduct", "08e5\n");
    fx_write("sys/devices/usb1/1-2/serial", "1EBF8ADF\n");
    fx_write("sys/devices/usb1/1-2/product", "HD Pro Webcam C920\n");
    fx_mkdirs("sys/devices/usb1/1-2/1-2:1.2");
    fx_path(t, sizeof(t), "sys/bus/usb");
    fx_symlink(t, "sys/devices/usb1/1-2/1-2:1.2/subsystem");
    fx_path(t, sizeof(t), "sys/drivers/snd-usb-audio");
    fx_symlink(t, "sys/devices/usb1/1-2/1-2:1.2/driver");
    fx_mkdirs("sys/class/sound/card3");
    fx_path(t, sizeof(t), "sys/devices/usb1/1-2/1-2:1.2");
    fx_symlink(t, "sys/class/sound/card3/device");

    fx_write("proc/asound/cards",
             " 0 [Creative       ]: HDA-Intel - HDA Creative\n"
             "                      HDA Creative at 0xfc204000 irq 30\n"
             " 3 [C920           ]: USB-Audio - HD Pro Webcam C920\n"
             "                      HD Pro Webcam C920 at usb-0000:2b:00.3-2.2, high speed\n");
    fx_write("proc/asound/card0/id", "Creative\n");
    fx_write("proc/asound/card0/codec#1", "Codec: Creative Sound Blaster Z\nAddress: 1\n");
    fx_write("proc/asound/card3/id", "C920\n");

    fx_path(t, sizeof(t), "sys");
    setenv("ATLAS_SYS_ROOT", t, 1);
    fx_path(t, sizeof(t), "proc");
    setenv("ATLAS_PROC_ROOT", t, 1);
}

static const char *CA0132_JSON =
    "{ \"id\": \"ca0132\", \"name\": \"Creative Sound Blaster Z\", \"chip\": \"CA0132\","
    "  \"aliases\": [\"sbz\"], \"quirks\": [\"close-pcm-before-sleep\"],"
    "  \"match\": [ { \"bus\": \"pci\", \"vendor\": \"0x1102\", \"device\": \"0x0012\","
    "                 \"subsystem_vendor\": \"0x1102\", \"subsystem_device\": \"0x0010\" } ] }";

static int count_dirs(const char *path)
{
    DIR *d = opendir(path);
    struct dirent *de;
    int n = 0;
    if (!d)
        return 0;
    while ((de = readdir(d)))
        if (de->d_name[0] != '.')
            n++;
    closedir(d);
    return n;
}

static void test_discover(void)
{
    AtlasHwDevice d[8];
    int n = atlas_hw_discover(d, 8);

    TEST_ASSERT_EQ_INT(2, n);
    TEST_ASSERT_EQ_INT(0, d[0].alsa_card);
    TEST_ASSERT_EQ_STR("pci", d[0].bus);
    TEST_ASSERT_EQ_STR("snd_hda_intel", d[0].driver);
    TEST_ASSERT_EQ_STR("0x1102", d[0].vendor);
    TEST_ASSERT_EQ_STR("0x0010", d[0].subsystem_device);
    TEST_ASSERT_EQ_STR("Creative Sound Blaster Z", d[0].codec);
    TEST_ASSERT_EQ_STR("pci:1102:0012:1102:0010", d[0].stable_key);
    TEST_ASSERT_EQ_STR("Creative", d[0].alsa_id);
    TEST_ASSERT_EQ_STR("HDA Creative at 0xfc204000 irq 30", d[0].longname);
    TEST_ASSERT_EQ_STR("", d[0].missing);

    TEST_ASSERT_EQ_INT(3, d[1].alsa_card);
    TEST_ASSERT_EQ_STR("usb", d[1].bus);
    TEST_ASSERT_EQ_STR("0x046d", d[1].vendor);   /* normalizado */
    TEST_ASSERT_EQ_STR("0x08e5", d[1].device);
    TEST_ASSERT_EQ_STR("usb:046d:08e5:1EBF8ADF", d[1].stable_key);
}

static void test_match_cases(void)
{
    AtlasHwDb db;
    AtlasHwDevice d[8];
    const AtlasHwEntry *e, *e2;
    char data[1024];

    atlas_hw_discover(d, 8);

    /* única */
    fx_write("db1/ca0132/card.json", CA0132_JSON);
    fx_path(data, sizeof(data), "db1");
    atlas_hwdb_load_dirs(&db, data, NULL);
    TEST_ASSERT_EQ_INT(1, db.n);
    TEST_ASSERT_EQ_INT(ATLAS_MATCH_UNIQUE, atlas_hwdb_match(&db, &d[0], &e, NULL));
    TEST_ASSERT_EQ_STR("ca0132", e->id);
    TEST_ASSERT(atlas_hw_has_quirk(e, "close-pcm-before-sleep"));
    TEST_ASSERT_EQ_INT(ATLAS_MATCH_NONE, atlas_hwdb_match(&db, &d[1], &e, NULL));
    TEST_ASSERT(atlas_hwdb_find(&db, "sbz") != NULL);
    TEST_ASSERT(atlas_hwdb_find(&db, "SBZ") != NULL);
    TEST_ASSERT(atlas_hwdb_find(&db, "nada") == NULL);

    /* ambigua: dos entradas con la misma regla */
    fx_write("db2/ca0132/card.json", CA0132_JSON);
    fx_write("db2/copia/card.json",
             "{ \"id\": \"copia\", \"name\": \"Copia\", \"match\": [ { \"bus\": \"pci\","
             " \"vendor\": \"0x1102\", \"device\": \"0x0012\", \"subsystem_vendor\": \"0x1102\","
             " \"subsystem_device\": \"0x0010\" } ] }");
    fx_path(data, sizeof(data), "db2");
    atlas_hwdb_load_dirs(&db, data, NULL);
    TEST_ASSERT_EQ_INT(ATLAS_MATCH_AMBIGUOUS, atlas_hwdb_match(&db, &d[0], &e, &e2));
    TEST_ASSERT(e != NULL && e2 != NULL && e != e2);

    /* más específica gana: una regla genérica (solo chip) y una exacta */
    fx_write("db3/ca0132/card.json", CA0132_JSON);
    fx_write("db3/familia/card.json",
             "{ \"id\": \"familia\", \"name\": \"CA0132 genérica\", \"match\": [ { \"bus\": \"pci\","
             " \"vendor\": \"0x1102\", \"device\": \"0x0012\" } ] }");
    fx_path(data, sizeof(data), "db3");
    atlas_hwdb_load_dirs(&db, data, NULL);
    TEST_ASSERT_EQ_INT(ATLAS_MATCH_UNIQUE, atlas_hwdb_match(&db, &d[0], &e, NULL));
    TEST_ASSERT_EQ_STR("ca0132", e->id);

    /* parcial: mismo chip, otro subsistema (p. ej. Recon3D) */
    fx_write("db4/recon3d/card.json",
             "{ \"id\": \"recon3d\", \"name\": \"Recon3D\", \"match\": [ { \"bus\": \"pci\","
             " \"vendor\": \"0x1102\", \"device\": \"0x0012\", \"subsystem_vendor\": \"0x1102\","
             " \"subsystem_device\": \"0x0013\" } ] }");
    fx_path(data, sizeof(data), "db4");
    atlas_hwdb_load_dirs(&db, data, NULL);
    TEST_ASSERT_EQ_INT(ATLAS_MATCH_PARTIAL, atlas_hwdb_match(&db, &d[0], &e, NULL));
    TEST_ASSERT_EQ_STR("recon3d", e->id);

    /* JSON roto: se informa y no se carga */
    fx_write("db5/rota/card.json", "{ esto no es json");
    fx_path(data, sizeof(data), "db5");
    atlas_hwdb_load_dirs(&db, data, NULL);
    TEST_ASSERT_EQ_INT(0, db.n);
    TEST_ASSERT_EQ_INT(1, db.nerrors);
}

static void test_register_no_duplicates(void)
{
    AtlasHwDb db;
    AtlasHwDevice d[8];
    char data[1024], user[1024], id[64], msg[768];
    int r;

    atlas_hw_discover(d, 8);
    fx_write("db6/ca0132/card.json", CA0132_JSON);
    fx_mkdirs("user-hw");
    fx_path(data, sizeof(data), "db6");
    fx_path(user, sizeof(user), "user-hw");
    atlas_hwdb_load_dirs(&db, data, user);

    /* ya registrada: no crea nada */
    r = atlas_hwdb_register(&db, &d[0], user, NULL, 0, 0, id, sizeof(id), msg, sizeof(msg));
    TEST_ASSERT_EQ_INT(ATLAS_MATCH_UNIQUE, r);
    TEST_ASSERT_EQ_STR("ca0132", id);
    TEST_ASSERT_EQ_INT(0, count_dirs(user));

    /* webcam nueva: se registra una vez... */
    r = atlas_hwdb_register(&db, &d[1], user, "c920", 0, 0, id, sizeof(id), msg, sizeof(msg));
    TEST_ASSERT_EQ_INT(ATLAS_MATCH_NONE, r);
    TEST_ASSERT_EQ_STR("c920", id);
    TEST_ASSERT_EQ_INT(1, count_dirs(user));

    /* ...y al redetectarla se reconoce, sin duplicar */
    atlas_hwdb_load_dirs(&db, data, user);
    r = atlas_hwdb_register(&db, &d[1], user, NULL, 0, 0, id, sizeof(id), msg, sizeof(msg));
    TEST_ASSERT_EQ_INT(ATLAS_MATCH_UNIQUE, r);
    TEST_ASSERT_EQ_STR("c920", id);
    TEST_ASSERT_EQ_INT(1, count_dirs(user));

    /* id inseguro rechazado */
    {
        AtlasHwDevice x = d[1];
        snprintf(x.vendor, sizeof(x.vendor), "0x9999");
        r = atlas_hwdb_register(&db, &x, user, "../etc", 0, 0, id, sizeof(id), msg, sizeof(msg));
        TEST_ASSERT_EQ_INT(ATLAS_MATCH_ERROR, r);
        TEST_ASSERT_EQ_INT(1, count_dirs(user));
    }

    /* sin IDs estables no se registra y se explica cómo obtenerlos */
    {
        AtlasHwDevice x;
        memset(&x, 0, sizeof(x));
        x.alsa_card = 7;
        snprintf(x.missing, sizeof(x.missing), "vendor,device");
        r = atlas_hwdb_register(&db, &x, user, NULL, 0, 0, id, sizeof(id), msg, sizeof(msg));
        TEST_ASSERT_EQ_INT(ATLAS_MATCH_ERROR, r);
        TEST_ASSERT(strstr(msg, "udevadm info") != NULL);
    }
}

static void test_matcher_score(void)
{
    AtlasHwMatcher m;
    AtlasHwDevice d;

    memset(&m, 0, sizeof(m));
    memset(&d, 0, sizeof(d));
    snprintf(d.vendor, sizeof(d.vendor), "0x1102");
    snprintf(d.device, sizeof(d.device), "0x0012");
    snprintf(d.codec, sizeof(d.codec), "Creative Sound Blaster Z");

    TEST_ASSERT_EQ_INT(0, atlas_matcher_score(&m, &d));          /* vacío no coincide */
    snprintf(m.vendor, sizeof(m.vendor), "0X1102");               /* mayúsculas */
    TEST_ASSERT_EQ_INT(1, atlas_matcher_score(&m, &d));
    snprintf(m.codec, sizeof(m.codec), "sound blaster");
    TEST_ASSERT_EQ_INT(2, atlas_matcher_score(&m, &d));
    snprintf(m.device, sizeof(m.device), "0x0013");
    TEST_ASSERT_EQ_INT(0, atlas_matcher_score(&m, &d));          /* un campo distinto anula */
}

int main(void)
{
    /* sin pactl real durante las pruebas */
    setenv("PATH", "/nonexistent", 1);
    fx_setup();
    make_sysfs();
    test_discover();
    test_match_cases();
    test_register_no_duplicates();
    test_matcher_score();
    fx_teardown();
    return test_summary();
}
