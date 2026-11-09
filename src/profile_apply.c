#include <stdio.h>

#include "pipewire_cmd.h"
#include "ppm_default.h"
#include "ppm_alsa_preset.h"
#include "ppm_volume.h"
#include "atlas_dsp.h"
#include "profile_apply.h"
#include "status.h"

static void
print_step(const char *name, const char *value)
{
    if (!name)
        return;

    if (!value)
        value = "";

    printf("[*] %-28s %s\n", name, value);
}

static int
apply_pipewire(const AudioProfile *profile)
{
    int ret;

    if (profile->rate > 0)
        printf("[*] %-28s %d Hz\n",
            "Frecuencia", profile->rate);
    else
        print_step("Frecuencia", "la del sistema");

    if (profile->quantum > 0)
        printf("[*] %-28s %d\n",
            "Quantum", profile->quantum);
    else
        print_step("Quantum", "la del sistema");

    if (profile->rate <= 0 && profile->quantum <= 0) {
        print_step("Ajustes de PipeWire", "sin forzar");
        return 0;
    }

    ret = pipewire_apply_rate_quantum(
        profile->rate,
        profile->quantum);

    if (ret != 0) {
        fprintf(stderr,
            "No se pudo fijar rate/quantum en PipeWire.\n");
        return -1;
    }

    print_step("Ajustes de PipeWire", "aplicado");

    return 0;
}

static int
apply_default_sink(const AudioProfile *profile)
{
    int ret;

    if (profile->default_sink[0] == '\0') {
        print_step("Salida por defecto", "no configurado");
        return 0;
    }

    print_step("Salida por defecto", profile->default_sink);

    ret = ppm_apply_default_sink(profile->default_sink);
    if (ret != 0) {
        fprintf(stderr,
            "No se pudo fijar la salida por defecto: %s\n",
            profile->default_sink);
        return -1;
    }

    print_step("Salida por defecto", "aplicado");

    return 0;
}

static int
apply_default_source(const AudioProfile *profile)
{
    int ret;

    if (profile->default_source[0] == '\0') {
        print_step("Entrada por defecto", "no configurado");
        return 0;
    }

    print_step("Entrada por defecto", profile->default_source);

    ret = ppm_apply_default_source(profile->default_source);
    if (ret != 0) {
        fprintf(stderr,
            "No se pudo fijar la entrada por defecto: %s\n",
            profile->default_source);
        return -1;
    }

    print_step("Entrada por defecto", "aplicado");

    return 0;
}

static int
apply_alsa_preset(const AudioProfile *profile)
{
    int ret;

    if (profile->alsa_preset[0] == '\0') {
        print_step("Preset ALSA", "no configurado");
        return 0;
    }

    print_step("Preset ALSA", profile->alsa_preset);

    ret = ppm_alsa_apply_preset(profile->alsa_preset);
    if (ret != 0) {
        fprintf(stderr,
            "No se pudo aplicar el preset ALSA: %s\n",
            profile->alsa_preset);
        return -1;
    }

    print_step("Preset ALSA", "aplicado");

    return 0;
}

static int
apply_dsp(const AudioProfile *profile)
{
    if (profile->dsp_preset[0] == '\0') {
        print_step("DSP", "no configurado");
        return 0;
    }

    if (ppm_dsp_apply(profile->dsp_preset) != 0) {
        fprintf(stderr,
            "No se pudo aplicar el DSP: %s\n",
            profile->dsp_preset);
        return -1;
    }

    return 0;
}

static int
apply_volume(const AudioProfile *profile)
{
    char value[16];
    int ret;

    if (!profile->restore_volume) {
        print_step("Volumen", "no pedido");
        return 0;
    }

    if (profile->sink_volume < 0 &&
        profile->source_volume < 0) {
        print_step("Volumen", "no configurado");
        return 0;
    }

    if (profile->sink_volume >= 0) {
        if (profile->sink_volume > 100) {
            fprintf(stderr,
                "Volumen de salida no válido: %d%%\n",
                profile->sink_volume);
            return -1;
        }

        if (profile->default_sink[0] == '\0') {
            fprintf(stderr,
                "Sink volume requires a default sink.\n");
            return -1;
        }

        snprintf(value,
            sizeof(value),
            "%d",
            profile->sink_volume);

        ret = ppm_set_volume(
            profile->default_sink,
            value);

        if (ret != 0) {
            fprintf(stderr,
                "No se pudo fijar el volumen de salida: %d%%\n",
                profile->sink_volume);
            return -1;
        }

        printf("[*] %-28s %d%%\n",
            "Sink volume",
            profile->sink_volume);
    }

    if (profile->source_volume >= 0) {
        if (profile->source_volume > 100) {
            fprintf(stderr,
                "Volumen de entrada no válido: %d%%\n",
                profile->source_volume);
            return -1;
        }

        if (profile->default_source[0] == '\0') {
            fprintf(stderr,
                "Source volume requires a default source.\n");
            return -1;
        }

        snprintf(value,
            sizeof(value),
            "%d",
            profile->source_volume);

        ret = ppm_set_volume(
            profile->default_source,
            value);

        if (ret != 0) {
            fprintf(stderr,
                "No se pudo fijar el volumen de entrada: %d%%\n",
                profile->source_volume);
            return -1;
        }

        printf("[*] %-28s %d%%\n",
            "Source volume",
            profile->source_volume);
    }

    return 0;
}

static int
apply_mute(const AudioProfile *profile)
{
    int ret;

    if (!profile->restore_volume) {
        print_step("Silencio", "no pedido");
        return 0;
    }

    if (profile->sink_muted < 0 &&
        profile->source_muted < 0) {
        print_step("Silencio", "no configurado");
        return 0;
    }

    if (profile->sink_muted >= 0) {
        if (profile->sink_muted > 1) {
            fprintf(stderr,
                "Valor de silencio de salida no válido: %d\n",
                profile->sink_muted);
            return -1;
        }

        if (profile->default_sink[0] == '\0') {
            fprintf(stderr,
                "Sink mute requires a default sink.\n");
            return -1;
        }

        ret = ppm_set_mute(
            profile->default_sink,
            profile->sink_muted ? "on" : "off");

        if (ret != 0) {
            fprintf(stderr,
                "No se pudo silenciar la salida: %s\n",
                profile->sink_muted ? "on" : "off");
            return -1;
        }

        print_step(
            "Sink mute",
            profile->sink_muted ? "on" : "off");
    }

    if (profile->source_muted >= 0) {
        if (profile->source_muted > 1) {
            fprintf(stderr,
                "Valor de silencio de entrada no válido: %d\n",
                profile->source_muted);
            return -1;
        }

        if (profile->default_source[0] == '\0') {
            fprintf(stderr,
                "Source mute requires a default source.\n");
            return -1;
        }

        ret = ppm_set_mute(
            profile->default_source,
            profile->source_muted ? "on" : "off");

        if (ret != 0) {
            fprintf(stderr,
                "No se pudo silenciar la entrada: %s\n",
                profile->source_muted ? "on" : "off");
            return -1;
        }

        print_step(
            "Source mute",
            profile->source_muted ? "on" : "off");
    }

    return 0;
}

int
profile_apply_validate(const AudioProfile *profile)
{
    if (!profile)
        return -1;

    if (!profile->enabled)
        return -1;

    if (profile->rate < 0)
        return -1;

    if (profile->quantum < 0)
        return -1;

    return 0;
}

int
profile_apply(const AudioProfile *profile)
{

    if (profile_apply_validate(profile) != 0)
        return -1;

    puts("=========================================");
    puts("Project Atlas");
    printf("Aplicando perfil: %s\n", profile->id);
    puts("=========================================");
    puts("");

    print_step("Cargando perfil", profile->id);

    if (profile->name[0] != '\0')
        print_step("Nombre", profile->name);

    if (apply_pipewire(profile) != 0)
        return -1;

    if (apply_default_sink(profile) != 0)
        return -1;

    if (apply_default_source(profile) != 0)
        return -1;

    if (apply_alsa_preset(profile) != 0)
        return -1;

    if (apply_dsp(profile) != 0)
        return -1;

    if (apply_volume(profile) != 0)
        return -1;

    if (apply_mute(profile) != 0)
        return -1;

    state_save_profile(profile->id);

    puts("");
    puts("Perfil aplicado.");

    return 0;
}
