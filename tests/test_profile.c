#include "profile.h"
#include "test_common.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(f);

    if (!f)
        return;

    fputs(text, f);
    fclose(f);
}

static void test_legacy_profile(void)
{
    AudioProfile p;
    const char *path = "/tmp/audio_profile_legacy_test.conf";

    write_file(path,
        "name=Gaming Low Latency\n"
        "rate=48000\n"
        "quantum=128\n"
        "description=Baja latencia para juegos\n");

    TEST_ASSERT_EQ(profile_load_from_file("gaming", path, &p), 0);

    TEST_ASSERT_EQ_STR("gaming", p.id);
    TEST_ASSERT_EQ_STR("Gaming Low Latency", p.name);
    TEST_ASSERT_EQ_STR("Baja latencia para juegos", p.description);
    TEST_ASSERT_EQ_INT(1, p.enabled);
    TEST_ASSERT_EQ_INT(48000, p.rate);
    TEST_ASSERT_EQ_INT(128, p.quantum);
    TEST_ASSERT_EQ_STR("", p.default_sink);
    TEST_ASSERT_EQ_STR("", p.default_source);
    TEST_ASSERT_EQ_STR("", p.alsa_preset);
    TEST_ASSERT_EQ_INT(0, p.restore_volume);
    TEST_ASSERT_EQ_INT(0, p.restore_defaults);
    TEST_ASSERT_EQ_INT(0, p.restore_links);

    unlink(path);
}

static void test_extended_profile(void)
{
    AudioProfile p;
    const char *path = "/tmp/audio_profile_extended_test.conf";

    write_file(path,
        "name=Gaming\n"
        "description=Low latency gaming profile\n"
        "enabled=yes\n"
        "rate=48000\n"
        "quantum=128\n"
        "default_sink=Sound Blaster Z Speakers\n"
        "default_source=Sound Blaster Z Microphone\n"
        "alsa_preset=ca0132-gaming.state\n"
        "restore_volume=yes\n"
        "restore_defaults=yes\n"
        "restore_links=yes\n"
        "wireplumber_profile=pro-audio\n"
        "easyeffects_preset=gaming.json\n"
        "priority=100\n");

    TEST_ASSERT_EQ(profile_load_from_file("gaming", path, &p), 0);

    TEST_ASSERT_EQ_STR("gaming", p.id);
    TEST_ASSERT_EQ_STR("Gaming", p.name);
    TEST_ASSERT_EQ_STR("Low latency gaming profile", p.description);
    TEST_ASSERT_EQ_INT(1, p.enabled);
    TEST_ASSERT_EQ_INT(48000, p.rate);
    TEST_ASSERT_EQ_INT(128, p.quantum);
    TEST_ASSERT_EQ_STR("Sound Blaster Z Speakers", p.default_sink);
    TEST_ASSERT_EQ_STR("Sound Blaster Z Microphone", p.default_source);
    TEST_ASSERT_EQ_STR("ca0132-gaming.state", p.alsa_preset);
    TEST_ASSERT_EQ_INT(1, p.restore_volume);
    TEST_ASSERT_EQ_INT(1, p.restore_defaults);
    TEST_ASSERT_EQ_INT(1, p.restore_links);
    TEST_ASSERT_EQ_STR("pro-audio", p.wireplumber_profile);
    TEST_ASSERT_EQ_STR("gaming.json", p.easyeffects_preset);
    TEST_ASSERT_EQ_INT(100, p.priority);

    unlink(path);
}

static void test_disabled_profile(void)
{
    AudioProfile p;
    const char *path = "/tmp/audio_profile_disabled_test.conf";

    write_file(path,
        "name=Disabled\n"
        "enabled=no\n"
        "rate=44100\n"
        "quantum=512\n");

    TEST_ASSERT_EQ(profile_load_from_file("disabled", path, &p), 0);

    TEST_ASSERT_EQ_INT(0, p.enabled);
    TEST_ASSERT_EQ_INT(44100, p.rate);
    TEST_ASSERT_EQ_INT(512, p.quantum);

    unlink(path);
}

static void test_restore_flags_and_priority(void)
{
    AudioProfile p;
    const char *path = "/tmp/audio_profile_restore_flags_test.conf";

    write_file(path,
        "name=Flags\n"
        "restore_volume=yes\n"
        "restore_defaults=yes\n"
        "restore_links=yes\n"
        "priority=100\n");

    TEST_ASSERT_EQ(profile_load_from_file("flags", path, &p), 0);

    TEST_ASSERT_EQ_INT(1, p.restore_volume);
    TEST_ASSERT_EQ_INT(1, p.restore_defaults);
    TEST_ASSERT_EQ_INT(1, p.restore_links);
    TEST_ASSERT_EQ_INT(100, p.priority);

    unlink(path);
}


static void
test_volume_fields(void)
{
    AudioProfile p;
    const char *path = "/tmp/audio_profile_volume_test.conf";

    write_file(path,
        "name=Volume Test\n"
        "sink_volume=75\n"
        "sink_muted=no\n"
        "source_volume=90\n"
        "source_muted=yes\n");

    TEST_ASSERT_EQ(
        profile_load_from_file("volume", path, &p),
        0);

    TEST_ASSERT_EQ_INT(75, p.sink_volume);
    TEST_ASSERT_EQ_INT(0, p.sink_muted);
    TEST_ASSERT_EQ_INT(90, p.source_volume);
    TEST_ASSERT_EQ_INT(1, p.source_muted);

    unlink(path);
}

static void
test_volume_fields_unset(void)
{
    AudioProfile p;
    const char *path =
        "/tmp/audio_profile_volume_unset_test.conf";

    write_file(path, "name=Unset Volume\n");

    TEST_ASSERT_EQ(
        profile_load_from_file("unset", path, &p),
        0);

    TEST_ASSERT_EQ_INT(-1, p.sink_volume);
    TEST_ASSERT_EQ_INT(-1, p.sink_muted);
    TEST_ASSERT_EQ_INT(-1, p.source_volume);
    TEST_ASSERT_EQ_INT(-1, p.source_muted);

    unlink(path);
}

static void test_missing_file(void)
{
    AudioProfile p;

    TEST_ASSERT_EQ(profile_load_from_file("missing", "/tmp/does-not-exist-profile.conf", &p), -1);
}

int main(void)
{
    test_legacy_profile();
    test_extended_profile();
    test_disabled_profile();
    test_restore_flags_and_priority();
    test_volume_fields();
    test_volume_fields_unset();
    test_missing_file();

    return test_summary();
}
