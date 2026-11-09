#include <string.h>

#include "profile_apply.h"
#include "test_common.h"

static int pipewire_calls;
static int pipewire_result;

static int default_sink_calls;
static int default_sink_result;
static char default_sink_name[128];

static int default_source_calls;
static int default_source_result;
static char default_source_name[128];

static int alsa_preset_calls;
static int alsa_preset_result;
static char alsa_preset_name[128];

static int volume_calls;
static int volume_result;
static char volume_target[128];
static char volume_value[32];

static int mute_calls;
static int mute_result;
static char mute_target[128];
static char mute_state[32];

static int state_save_calls;

int
pipewire_apply_rate_quantum(int rate, int quantum)
{
    (void)rate;
    (void)quantum;

    pipewire_calls++;
    return pipewire_result;
}

int
ppm_apply_default_sink(const char *name)
{
    size_t length;

    default_sink_calls++;

    if (!name)
        return -1;

    length = strlen(name);
    if (length >= sizeof(default_sink_name))
        length = sizeof(default_sink_name) - 1;

    memcpy(default_sink_name, name, length);
    default_sink_name[length] = '\0';

    return default_sink_result;
}


int
ppm_apply_default_source(const char *name)
{
    size_t length;

    default_source_calls++;

    if (!name)
        return -1;

    length = strlen(name);
    if (length >= sizeof(default_source_name))
        length = sizeof(default_source_name) - 1;

    memcpy(default_source_name, name, length);
    default_source_name[length] = '\0';

    return default_source_result;
}


int
ppm_alsa_apply_preset(const char *preset)
{
    size_t length;

    alsa_preset_calls++;

    if (!preset)
        return -1;

    length = strlen(preset);
    if (length >= sizeof(alsa_preset_name))
        length = sizeof(alsa_preset_name) - 1;

    memcpy(alsa_preset_name, preset, length);
    alsa_preset_name[length] = '\0';

    return alsa_preset_result;
}

int
ppm_set_volume(const char *target, const char *volume)
{
    size_t length;

    volume_calls++;

    if (!target || !volume)
        return -1;

    length = strlen(target);
    if (length >= sizeof(volume_target))
        length = sizeof(volume_target) - 1;

    memcpy(volume_target, target, length);
    volume_target[length] = '\0';

    length = strlen(volume);
    if (length >= sizeof(volume_value))
        length = sizeof(volume_value) - 1;

    memcpy(volume_value, volume, length);
    volume_value[length] = '\0';

    return volume_result;
}

int
ppm_set_mute(const char *target, const char *state)
{
    size_t length;

    mute_calls++;

    if (!target || !state)
        return -1;

    length = strlen(target);
    if (length >= sizeof(mute_target))
        length = sizeof(mute_target) - 1;

    memcpy(mute_target, target, length);
    mute_target[length] = '\0';

    length = strlen(state);
    if (length >= sizeof(mute_state))
        length = sizeof(mute_state) - 1;

    memcpy(mute_state, state, length);
    mute_state[length] = '\0';

    return mute_result;
}

void
state_save_profile(const char *id)
{
    (void)id;
    state_save_calls++;
}

static int dsp_calls;
static int dsp_result;
static char dsp_name[64];

int
ppm_dsp_apply(const char *preset)
{
    dsp_calls++;
    if (!preset)
        return -1;
    snprintf(dsp_name, sizeof(dsp_name), "%s", preset);
    return dsp_result;
}

static void
reset_mocks(void)
{
    pipewire_calls = 0;
    pipewire_result = 0;

    default_sink_calls = 0;
    default_sink_result = 0;
    default_sink_name[0] = '\0';

    default_source_calls = 0;
    default_source_result = 0;
    default_source_name[0] = '\0';

    alsa_preset_calls = 0;
    alsa_preset_result = 0;
    alsa_preset_name[0] = '\0';

    volume_calls = 0;
    volume_result = 0;
    volume_target[0] = '\0';
    volume_value[0] = '\0';

    mute_calls = 0;
    mute_result = 0;
    mute_target[0] = '\0';
    mute_state[0] = '\0';

    state_save_calls = 0;
}

static AudioProfile
make_valid_profile(void)
{
    AudioProfile profile = {0};

    profile.enabled = 1;
    profile.rate = 48000;
    profile.quantum = 128;
    profile.sink_volume = -1;
    profile.sink_muted = -1;
    profile.source_volume = -1;
    profile.source_muted = -1;

    memcpy(profile.id, "gaming", sizeof("gaming"));
    memcpy(profile.name,
        "Gaming Low Latency",
        sizeof("Gaming Low Latency"));

    return profile;
}

static void
test_validate_ok(void)
{
    AudioProfile profile = make_valid_profile();

    TEST_ASSERT_EQ(profile_apply_validate(&profile), 0);
}

static void
test_validate_null(void)
{
    TEST_ASSERT_EQ(profile_apply_validate(NULL), -1);
}

static void
test_validate_disabled(void)
{
    AudioProfile profile = make_valid_profile();

    profile.enabled = 0;

    TEST_ASSERT_EQ(profile_apply_validate(&profile), -1);
}

static void
test_validate_negative_rate(void)
{
    AudioProfile profile = make_valid_profile();

    profile.rate = -1;

    TEST_ASSERT_EQ(profile_apply_validate(&profile), -1);
}

static void
test_validate_negative_quantum(void)
{
    AudioProfile profile = make_valid_profile();

    profile.quantum = -1;

    TEST_ASSERT_EQ(profile_apply_validate(&profile), -1);
}

static void
test_apply_without_default_sink(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    TEST_ASSERT_EQ(profile_apply(&profile), 0);
    TEST_ASSERT_EQ_INT(1, pipewire_calls);
    TEST_ASSERT_EQ_INT(0, default_sink_calls);
    TEST_ASSERT_EQ_INT(1, state_save_calls);
}

static void
test_apply_default_sink(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    memcpy(profile.default_sink,
        "Sound Blaster Z",
        sizeof("Sound Blaster Z"));

    TEST_ASSERT_EQ(profile_apply(&profile), 0);
    TEST_ASSERT_EQ_INT(1, pipewire_calls);
    TEST_ASSERT_EQ_INT(1, default_sink_calls);
    TEST_ASSERT_EQ_STR("Sound Blaster Z", default_sink_name);
    TEST_ASSERT_EQ_INT(1, state_save_calls);
}

static void
test_default_sink_failure(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    memcpy(profile.default_sink,
        "Missing Sink",
        sizeof("Missing Sink"));

    default_sink_result = -1;

    TEST_ASSERT_EQ(profile_apply(&profile), -1);
    TEST_ASSERT_EQ_INT(1, default_sink_calls);
    TEST_ASSERT_EQ_INT(0, state_save_calls);
}


static void
test_apply_default_source(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    memcpy(profile.default_source,
        "Sound Blaster Z Microphone",
        sizeof("Sound Blaster Z Microphone"));

    TEST_ASSERT_EQ(profile_apply(&profile), 0);
    TEST_ASSERT_EQ_INT(1, pipewire_calls);
    TEST_ASSERT_EQ_INT(0, default_sink_calls);
    TEST_ASSERT_EQ_INT(1, default_source_calls);
    TEST_ASSERT_EQ_STR(
        "Sound Blaster Z Microphone",
        default_source_name);
    TEST_ASSERT_EQ_INT(1, state_save_calls);
}

static void
test_default_source_failure(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    memcpy(profile.default_source,
        "Missing Source",
        sizeof("Missing Source"));

    default_source_result = -1;

    TEST_ASSERT_EQ(profile_apply(&profile), -1);
    TEST_ASSERT_EQ_INT(1, default_source_calls);
    TEST_ASSERT_EQ_INT(0, state_save_calls);
}

static void
test_apply_sink_and_source(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    memcpy(profile.default_sink,
        "Sound Blaster Z",
        sizeof("Sound Blaster Z"));

    memcpy(profile.default_source,
        "Sound Blaster Z Microphone",
        sizeof("Sound Blaster Z Microphone"));

    TEST_ASSERT_EQ(profile_apply(&profile), 0);
    TEST_ASSERT_EQ_INT(1, default_sink_calls);
    TEST_ASSERT_EQ_INT(1, default_source_calls);
    TEST_ASSERT_EQ_STR("Sound Blaster Z", default_sink_name);
    TEST_ASSERT_EQ_STR(
        "Sound Blaster Z Microphone",
        default_source_name);
    TEST_ASSERT_EQ_INT(1, state_save_calls);
}


static void
test_apply_without_alsa_preset(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    TEST_ASSERT_EQ(profile_apply(&profile), 0);
    TEST_ASSERT_EQ_INT(0, alsa_preset_calls);
    TEST_ASSERT_EQ_INT(1, state_save_calls);
}

static void
test_apply_alsa_preset(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    memcpy(profile.alsa_preset,
        "ca0132-optimal.state",
        sizeof("ca0132-optimal.state"));

    TEST_ASSERT_EQ(profile_apply(&profile), 0);
    TEST_ASSERT_EQ_INT(1, alsa_preset_calls);
    TEST_ASSERT_EQ_STR(
        "ca0132-optimal.state",
        alsa_preset_name);
    TEST_ASSERT_EQ_INT(1, state_save_calls);
}

static void
test_alsa_preset_failure(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    memcpy(profile.alsa_preset,
        "missing.state",
        sizeof("missing.state"));

    alsa_preset_result = -1;

    TEST_ASSERT_EQ(profile_apply(&profile), -1);
    TEST_ASSERT_EQ_INT(1, alsa_preset_calls);
    TEST_ASSERT_EQ_INT(0, state_save_calls);
}

static void
test_apply_sink_source_and_alsa(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    memcpy(profile.default_sink,
        "Sound Blaster Z",
        sizeof("Sound Blaster Z"));

    memcpy(profile.default_source,
        "Sound Blaster Z Microphone",
        sizeof("Sound Blaster Z Microphone"));

    memcpy(profile.alsa_preset,
        "ca0132-optimal.state",
        sizeof("ca0132-optimal.state"));

    TEST_ASSERT_EQ(profile_apply(&profile), 0);
    TEST_ASSERT_EQ_INT(1, default_sink_calls);
    TEST_ASSERT_EQ_INT(1, default_source_calls);
    TEST_ASSERT_EQ_INT(1, alsa_preset_calls);
    TEST_ASSERT_EQ_STR(
        "ca0132-optimal.state",
        alsa_preset_name);
    TEST_ASSERT_EQ_INT(1, state_save_calls);
}

static void
test_apply_sink_volume(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    profile.restore_volume = 1;
    profile.sink_volume = 75;

    memcpy(profile.default_sink,
        "Sound Blaster Z",
        sizeof("Sound Blaster Z"));

    TEST_ASSERT_EQ(profile_apply(&profile), 0);
    TEST_ASSERT_EQ_INT(1, volume_calls);
    TEST_ASSERT_EQ_STR("Sound Blaster Z", volume_target);
    TEST_ASSERT_EQ_STR("75", volume_value);
    TEST_ASSERT_EQ_INT(1, state_save_calls);
}

static void
test_apply_source_volume(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    profile.restore_volume = 1;
    profile.source_volume = 90;

    memcpy(profile.default_source,
        "Sound Blaster Z Microphone",
        sizeof("Sound Blaster Z Microphone"));

    TEST_ASSERT_EQ(profile_apply(&profile), 0);
    TEST_ASSERT_EQ_INT(1, volume_calls);
    TEST_ASSERT_EQ_STR(
        "Sound Blaster Z Microphone",
        volume_target);
    TEST_ASSERT_EQ_STR("90", volume_value);
    TEST_ASSERT_EQ_INT(1, state_save_calls);
}
static void
test_apply_sink_and_source_volume(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    profile.restore_volume = 1;

    profile.sink_volume = 75;
    profile.source_volume = 90;

    memcpy(profile.default_sink,
        "Sound Blaster Z",
        sizeof("Sound Blaster Z"));

    memcpy(profile.default_source,
        "Sound Blaster Z Microphone",
        sizeof("Sound Blaster Z Microphone"));

    TEST_ASSERT_EQ(profile_apply(&profile),0);

    TEST_ASSERT_EQ_INT(2,volume_calls);

    TEST_ASSERT_EQ_STR(
        "Sound Blaster Z Microphone",
        volume_target);

    TEST_ASSERT_EQ_STR("90",volume_value);

    TEST_ASSERT_EQ_INT(1,state_save_calls);
}



static void
test_sink_volume_failure(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    profile.restore_volume = 1;

    profile.sink_volume = 80;

    memcpy(profile.default_sink,
           "Sound Blaster Z",
           sizeof("Sound Blaster Z"));

    volume_result = -1;

    TEST_ASSERT_EQ(profile_apply(&profile), -1);

    TEST_ASSERT_EQ_INT(1, volume_calls);

    TEST_ASSERT_EQ_INT(0, state_save_calls);
}


static void
test_source_volume_failure(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    profile.restore_volume = 1;
    profile.source_volume = 85;

    memcpy(profile.default_source,
        "Sound Blaster Z Microphone",
        sizeof("Sound Blaster Z Microphone"));

    volume_result = -1;

    TEST_ASSERT_EQ(profile_apply(&profile), -1);
    TEST_ASSERT_EQ_INT(1, volume_calls);
    TEST_ASSERT_EQ_STR(
        "Sound Blaster Z Microphone",
        volume_target);
    TEST_ASSERT_EQ_STR("85", volume_value);
    TEST_ASSERT_EQ_INT(0, state_save_calls);
}


static void
test_restore_volume_disabled(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    profile.restore_volume = 0;

    profile.sink_volume = 75;

    memcpy(profile.default_sink,
           "Sound Blaster Z",
           sizeof("Sound Blaster Z"));

    TEST_ASSERT_EQ(profile_apply(&profile),0);

    TEST_ASSERT_EQ_INT(0, volume_calls);

    TEST_ASSERT_EQ_INT(0, mute_calls);

    TEST_ASSERT_EQ_INT(1, state_save_calls);
}



static void
test_volume_not_configured(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    profile.restore_volume = 1;

    profile.sink_volume = -1;
    profile.source_volume = -1;

    TEST_ASSERT_EQ(profile_apply(&profile),0);

    TEST_ASSERT_EQ_INT(0,volume_calls);

    TEST_ASSERT_EQ_INT(1,state_save_calls);
}


static void
test_apply_sink_mute(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    profile.restore_volume = 1;
    profile.sink_muted = 1;

    memcpy(profile.default_sink,
        "Sound Blaster Z",
        sizeof("Sound Blaster Z"));

    TEST_ASSERT_EQ(profile_apply(&profile), 0);
    TEST_ASSERT_EQ_INT(1, mute_calls);
    TEST_ASSERT_EQ_STR("Sound Blaster Z", mute_target);
    TEST_ASSERT_EQ_STR("on", mute_state);
    TEST_ASSERT_EQ_INT(1, state_save_calls);
}

static void
test_apply_source_mute(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    profile.restore_volume = 1;
    profile.source_muted = 1;

    memcpy(profile.default_source,
        "Sound Blaster Z Microphone",
        sizeof("Sound Blaster Z Microphone"));

    TEST_ASSERT_EQ(profile_apply(&profile), 0);
    TEST_ASSERT_EQ_INT(1, mute_calls);
    TEST_ASSERT_EQ_STR(
        "Sound Blaster Z Microphone",
        mute_target);
    TEST_ASSERT_EQ_STR("on", mute_state);
    TEST_ASSERT_EQ_INT(1, state_save_calls);
}

static void
test_apply_sink_and_source_mute(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    profile.restore_volume = 1;
    profile.sink_muted = 1;
    profile.source_muted = 1;

    memcpy(profile.default_sink,
        "Sound Blaster Z",
        sizeof("Sound Blaster Z"));

    memcpy(profile.default_source,
        "Sound Blaster Z Microphone",
        sizeof("Sound Blaster Z Microphone"));

    TEST_ASSERT_EQ(profile_apply(&profile), 0);
    TEST_ASSERT_EQ_INT(2, mute_calls);
    TEST_ASSERT_EQ_STR(
        "Sound Blaster Z Microphone",
        mute_target);
    TEST_ASSERT_EQ_STR("on", mute_state);
    TEST_ASSERT_EQ_INT(1, state_save_calls);
}


static void
test_sink_mute_failure(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    profile.restore_volume = 1;
    profile.sink_muted = 1;

    memcpy(profile.default_sink,
           "Sound Blaster Z",
           sizeof("Sound Blaster Z"));

    mute_result = -1;

    TEST_ASSERT_EQ(profile_apply(&profile), -1);

    TEST_ASSERT_EQ_INT(1, mute_calls);

    TEST_ASSERT_EQ_INT(0, state_save_calls);
}



static void
test_source_mute_failure(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    profile.restore_volume = 1;
    profile.source_muted = 1;

    memcpy(profile.default_source,
           "Sound Blaster Z Microphone",
           sizeof("Sound Blaster Z Microphone"));

    mute_result = -1;

    TEST_ASSERT_EQ(profile_apply(&profile), -1);

    TEST_ASSERT_EQ_INT(1, mute_calls);

    TEST_ASSERT_EQ_STR(
        "Sound Blaster Z Microphone",
        mute_target);

    TEST_ASSERT_EQ_STR("on", mute_state);

    TEST_ASSERT_EQ_INT(0, state_save_calls);
}



static void
test_mute_not_configured(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();

    profile.restore_volume = 1;

    profile.sink_muted = -1;
    profile.source_muted = -1;

    TEST_ASSERT_EQ(profile_apply(&profile), 0);

    TEST_ASSERT_EQ_INT(0, mute_calls);

    TEST_ASSERT_EQ_INT(1, state_save_calls);
}


/* v0.2: paso DSP (presets por tarjeta de la Hardware DB) */

static void
test_dsp_not_configured(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();
    dsp_calls = 0;
    dsp_result = 0;

    TEST_ASSERT_EQ(profile_apply(&profile), 0);
    TEST_ASSERT_EQ_INT(0, dsp_calls);
}

static void
test_apply_dsp(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();
    dsp_calls = 0;
    dsp_result = 0;
    snprintf(profile.dsp_preset, sizeof(profile.dsp_preset), "fps");

    TEST_ASSERT_EQ(profile_apply(&profile), 0);
    TEST_ASSERT_EQ_INT(1, dsp_calls);
    TEST_ASSERT_EQ_STR("fps", dsp_name);
    TEST_ASSERT_EQ_INT(1, state_save_calls);
}

static void
test_dsp_failure_aborts(void)
{
    AudioProfile profile = make_valid_profile();

    reset_mocks();
    dsp_calls = 0;
    dsp_result = -1;
    snprintf(profile.dsp_preset, sizeof(profile.dsp_preset), "racing");

    TEST_ASSERT_EQ(profile_apply(&profile), -1);
    TEST_ASSERT_EQ_INT(1, dsp_calls);
    /* un perfil a medias no se marca como actual */
    TEST_ASSERT_EQ_INT(0, state_save_calls);
    dsp_result = 0;
}

int
main(void)
{
    test_dsp_not_configured();
    test_apply_dsp();
    test_dsp_failure_aborts();
    test_validate_ok();
    test_validate_null();
    test_validate_disabled();
    test_validate_negative_rate();
    test_validate_negative_quantum();
    test_apply_without_default_sink();
    test_apply_default_sink();
    test_default_sink_failure();
    test_apply_default_source();
    test_default_source_failure();
    test_apply_sink_and_source();
    test_apply_without_alsa_preset();
    test_apply_alsa_preset();
    test_alsa_preset_failure();
    test_apply_sink_source_and_alsa();

    test_apply_sink_volume();
    test_apply_source_volume();
    test_apply_sink_and_source_volume();
    test_sink_volume_failure();
    test_source_volume_failure();
    test_restore_volume_disabled();
    test_mute_not_configured();
    test_volume_not_configured();
    test_apply_sink_mute();
    test_apply_source_mute();
    test_apply_sink_and_source_mute();
    test_sink_mute_failure();
    test_source_mute_failure();
    return test_summary();
}
