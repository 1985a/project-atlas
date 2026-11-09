#ifndef PPM_MODELS_H
#define PPM_MODELS_H

#include <stdint.h>

#define PPM_NAME_MAX        128
#define PPM_DESC_MAX        256
#define PPM_ID_MAX          64
#define PPM_PATH_MAX        256
#define PPM_MAX_CAPS        64
#define PPM_MAX_CONTROLS    256
#define PPM_MAX_STREAMS     256

typedef struct
{
    char id[PPM_ID_MAX];
    char name[PPM_NAME_MAX];
    char description[PPM_DESC_MAX];

    int supported;
} PpmCapability;

typedef struct
{
    char name[PPM_NAME_MAX];
    char type[PPM_ID_MAX];

    int readable;
    int writable;

    long min;
    long max;

    long values[8];
    int value_count;

    long recommended[8];
    int recommended_count;
} PpmMixerControl;

typedef struct
{
    uint32_t id;

    char name[PPM_NAME_MAX];
    char description[PPM_DESC_MAX];
    char media_class[PPM_ID_MAX];

    char device_id[PPM_ID_MAX];
    char client_id[PPM_ID_MAX];
} PpmCoreNode;

typedef struct
{
    uint32_t id;

    char node_id[PPM_ID_MAX];
    char direction[PPM_ID_MAX];
    char name[PPM_NAME_MAX];
    char alias[PPM_NAME_MAX];
} PpmCorePort;

typedef struct
{
    uint32_t id;

    char output_port[PPM_ID_MAX];
    char input_port[PPM_ID_MAX];
} PpmCoreLink;

typedef struct
{
    uint32_t node_id;

    char node_name[PPM_NAME_MAX];
    char app_name[PPM_NAME_MAX];
    char media_class[PPM_ID_MAX];

    char target_sink[PPM_NAME_MAX];

    float volume;
    int muted;

    int channels;
    int rate;
} PpmStream;

typedef struct
{
    char id[PPM_ID_MAX];
    char name[PPM_NAME_MAX];
    char chip[PPM_NAME_MAX];
    char model[PPM_NAME_MAX];

    int alsa_card;

    PpmCapability capabilities[PPM_MAX_CAPS];
    int capability_count;

    PpmMixerControl controls[PPM_MAX_CONTROLS];
    int control_count;
} PpmHardware;

typedef struct
{
    char name[PPM_NAME_MAX];
    char description[PPM_DESC_MAX];

    char hardware_id[PPM_ID_MAX];

    PpmMixerControl controls[PPM_MAX_CONTROLS];
    int control_count;

    PpmStream streams[PPM_MAX_STREAMS];
    int stream_count;
} PpmProfile;

#endif
