#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atlas.h"
#include "atlas_auto.h"
#include "atlas_base.h"
#include "atlas_output.h"
#include "atlas_steam.h"
#include "atlas_dsp.h"
#include "atlas_health.h"
#include "atlas_hwdb.h"
#include "atlas_recover.h"
#include "atlas_watch.h"

#include "backup.h"
#include "latency.h"
#include "monitor.h"
#include "pipewire_cmd.h"
#include "profile.h"
#include "profile_apply.h"
#include "status.h"

#include "ppm_alsa_preset.h"
#include "ppm_context.h"
#include "ppm_control.h"
#include "ppm_default.h"
#include "ppm_device.h"
#include "ppm_graph.h"
#include "ppm_link.h"
#include "ppm_node.h"
#include "ppm_port.h"
#include "ppm_profile_state.h"
#include "ppm_volume.h"
#include "ppm_wp_core.h"
#include "ppm_wp_objects.h"

static void usage(void)
{
    puts("Project Atlas · audio-profile " ATLAS_VERSION);
    puts("");
    puts("Diagnóstico y recuperación");
    puts("  status [--probe]          estado: servicios, conexión, hardware");
    puts("  check                     sonda funcional (0 sano · 1 fallo · 2 sin verificar)");
    puts("  doctor                    diagnóstico completo con comandos de arreglo");
    puts("  recover [--dry-run] [--force] [--confirm] [--max-level 1-3]");
    puts("                            recuperación escalonada y verificada");
    puts("  test-tone [--sink S]      tono suave para confirmar que se oye");
    puts("  report                    informe de evidencia");
    puts("  history [-n N]            historial de recuperaciones/reanudaciones");
    puts("  watch [--simulate-resume] daemon: protección al suspender + perfiles automáticos");
    puts("");
    puts("Perfiles");
    puts("  list                      perfiles disponibles");
    puts("  show <perfil>             ver un perfil");
    puts("  apply <perfil>            aplicar (pausa el modo automático)");
    puts("  reset                     quitar rate/quantum forzados");
    puts("  auto [status|on|off|once|rules|assign <perfil> <cond...>]");
    puts("  steam-genre <appid> [--refresh]   clase del juego para el modo automático");
    puts("  output [status|speakers|headphones|toggle]   altavoces 5.1 / auriculares");
    puts("  save <nombre> | load <nombre>   instantáneas completas del usuario");
    puts("");
    puts("Base de volumen (se mantiene con todos los perfiles)");
    puts("  base import [archivo]     importar tu asound.state (def. ~/.config/asound.state)");
    puts("  base save                 guardar los volúmenes actuales como base");
    puts("  base show | base apply");
    puts("");
    puts("DSP de la tarjeta");
    puts("  dsp [get]                 controles actuales");
    puts("  dsp presets               presets de la tarjeta");
    puts("  dsp apply <preset>        aplicar preset");
    puts("  dsp set \"<control>\" <v>   cambiar un control");
    puts("  dsp save <nombre>         guardar efectos actuales como preset");
    puts("  alsa-save <nombre> [--card ID] [--force]");
    puts("");
    puts("Hardware");
    puts("  devices                   tarjetas + Hardware DB");
    puts("  hardware-info <id|n>      ficha de la Hardware DB");
    puts("  hardware-register <n|all> [--id X] [--new] [--link id] [--dry-run]");
    puts("  hardware-validate         validar JSON, enlaces y presets");
    puts("");
    puts("Grafo (bajo nivel)");
    puts("  sinks | sources | streams | ports | links | graph");
    puts("  move <stream> <sink> · default-sink <s> · default-source <s>");
    puts("  volume <target> <0-100> · mute <target> <on|off>");
    puts("  latency · monitor · backup · wp-info · wp-objects · ca0132-optimal");
    puts("");
    puts("Opciones globales: --json (salida para scripts/GUI), --quiet");
}

static int open_registry(PpmContext *ctx)
{
    if (ppm_context_open(ctx) != 0) {
        puts("No pude conectar con PipeWire.");
        return -1;
    }
    ppm_context_roundtrip(ctx);
    return 0;
}

typedef void (*DumpFn)(void);

static int with_registry(DumpFn fn)
{
    PpmContext ctx;
    if (open_registry(&ctx) != 0)
        return 1;
    fn();
    ppm_context_close(&ctx);
    return 0;
}

static int cmd_show(const char *id)
{
    AudioProfile p;
    if (!id) {
        fprintf(stderr, "Falta perfil.\n");
        return 1;
    }
    if (profile_load(id, &p) != 0) {
        fprintf(stderr, "No pude cargar perfil: %s\n", id);
        return 1;
    }
    profile_print(&p);
    return 0;
}

static int cmd_reset(void)
{
    if (pipewire_apply_rate_quantum(0, 0) != 0) {
        fprintf(stderr, "No pude restablecer rate/quantum.\n");
        return 1;
    }
    state_save_profile("reference");
    puts("Rate y quantum vuelven a ser automáticos (perfil reference).");
    return 0;
}

int main(int argc, char **argv)
{
    char *args[128];
    int n = 0, i;
    const char *cmd;

    /* opciones globales en cualquier posición */
    for (i = 0; i < argc && n < 127; i++) {
        if (i > 0 && !strcmp(argv[i], "--json"))
            atlas_json_mode = 1;
        else if (i > 0 && !strcmp(argv[i], "--quiet"))
            atlas_quiet = 1;
        else
            args[n++] = argv[i];
    }
    args[n] = NULL;
    argc = n;
    argv = args;

    if (argc < 2 || !strcmp(argv[1], "help") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
        usage();
        return 0;
    }
    cmd = argv[1];

    if (!strcmp(cmd, "version") || !strcmp(cmd, "--version")) {
        if (atlas_json_mode) {
            JsonBuilder *b = json_builder_new();
            json_builder_begin_object(b);
            atlas_json_kv_str(b, "version", ATLAS_VERSION);
            atlas_json_kv_str(b, "data_dir", atlas_data_dir());
            json_builder_end_object(b);
            atlas_json_print(b);
        } else {
            printf("audio-profile %s (datos: %s)\n", ATLAS_VERSION, atlas_data_dir());
        }
        return 0;
    }

    /* --- diagnóstico / recuperación --- */
    if (!strcmp(cmd, "status"))
        return atlas_cmd_status(argc > 2 && !strcmp(argv[2], "--probe"));
    if (!strcmp(cmd, "check"))
        return atlas_cmd_check();
    if (!strcmp(cmd, "doctor"))
        return atlas_cmd_doctor();
    if (!strcmp(cmd, "recover"))
        return atlas_cmd_recover(argc - 2, argv + 2);
    if (!strcmp(cmd, "test-tone"))
        return atlas_cmd_test_tone(argc - 2, argv + 2);
    if (!strcmp(cmd, "report"))
        return atlas_cmd_report();
    if (!strcmp(cmd, "history"))
        return atlas_cmd_history(argc - 2, argv + 2);
    if (!strcmp(cmd, "watch"))
        return atlas_cmd_watch(argc - 2, argv + 2);

    /* --- perfiles --- */
    if (!strcmp(cmd, "list") || !strcmp(cmd, "profiles")) {
        profile_list();
        return 0;
    }
    if (!strcmp(cmd, "show"))
        return cmd_show(argc > 2 ? argv[2] : NULL);
    if (!strcmp(cmd, "apply")) {
        int by_auto = argc > 3 && !strcmp(argv[3], "--auto");
        if (argc < 3) {
            fprintf(stderr, "Falta perfil.\n");
            return 1;
        }
        return atlas_apply_profile_id(argv[2], by_auto) == 0 ? 0 : 1;
    }
    if (!strcmp(cmd, "reset"))
        return cmd_reset();
    if (!strcmp(cmd, "auto"))
        return atlas_cmd_auto(argc - 2, argv + 2);
    if (!strcmp(cmd, "save") || !strcmp(cmd, "load")) {
        if (argc < 3) {
            fprintf(stderr, "Uso: audio-profile %s <nombre>\n", cmd);
            return 1;
        }
        return (!strcmp(cmd, "save") ? ppm_profile_save(argv[2]) : ppm_profile_load(argv[2])) == 0 ? 0 : 1;
    }

    /* --- DSP / ALSA --- */
    if (!strcmp(cmd, "dsp"))
        return atlas_cmd_dsp(argc - 2, argv + 2);
    if (!strcmp(cmd, "base"))
        return atlas_cmd_base(argc - 2, argv + 2);
    if (!strcmp(cmd, "output"))
        return atlas_cmd_output(argc - 2, argv + 2);
    if (!strcmp(cmd, "steam-genre"))
        return atlas_cmd_steam_genre(argc - 2, argv + 2);
    if (!strcmp(cmd, "alsa-save")) {
        const char *card = "Creative";
        int force = 0;
        if (argc < 3) {
            fprintf(stderr, "Uso: audio-profile alsa-save <nombre> [--card ID] [--force]\n");
            return 1;
        }
        for (i = 3; i < argc; i++) {
            if (!strcmp(argv[i], "--card") && i + 1 < argc)
                card = argv[++i];
            else if (!strcmp(argv[i], "--force"))
                force = 1;
        }
        return ppm_alsa_save_preset(argv[2], card, force) == 0 ? 0 : 1;
    }
    if (!strcmp(cmd, "ca0132-optimal"))
        return ppm_alsa_apply_ca0132_optimal() == 0 ? 0 : 1;

    /* --- hardware --- */
    if (!strcmp(cmd, "devices"))
        return atlas_cmd_devices();
    if (!strcmp(cmd, "hardware-info")) {
        if (argc < 3) {
            fprintf(stderr, "Uso: audio-profile hardware-info <id|tarjeta>\n");
            return 1;
        }
        return atlas_cmd_hardware_info(argv[2]);
    }
    if (!strcmp(cmd, "hardware-register"))
        return atlas_cmd_hardware_register(argc - 2, argv + 2);
    if (!strcmp(cmd, "hardware-validate"))
        return atlas_cmd_hardware_validate();
    if (!strcmp(cmd, "pw-devices"))
        return with_registry(ppm_device_dump);

    /* --- grafo / bajo nivel (v0.1) --- */
    if (!strcmp(cmd, "sinks"))
        return with_registry(ppm_node_dump_sinks);
    if (!strcmp(cmd, "sources"))
        return with_registry(ppm_node_dump_sources);
    if (!strcmp(cmd, "streams"))
        return with_registry(ppm_node_dump_streams);
    if (!strcmp(cmd, "ports"))
        return with_registry(ppm_port_dump);
    if (!strcmp(cmd, "links"))
        return with_registry(ppm_link_dump);
    if (!strcmp(cmd, "graph"))
        return with_registry(ppm_graph_dump);

    if (!strcmp(cmd, "move") || !strcmp(cmd, "volume") || !strcmp(cmd, "mute")) {
        PpmContext ctx;
        int r;
        if (argc < 4) {
            fprintf(stderr, "Uso: audio-profile %s <objetivo> <valor>\n", cmd);
            return 1;
        }
        if (open_registry(&ctx) != 0)
            return 1;
        if (!strcmp(cmd, "move")) {
            ppm_move_prepare(argv[2], argv[3]);
            r = 0;
        } else if (!strcmp(cmd, "volume")) {
            r = ppm_set_volume(argv[2], argv[3]);
        } else {
            r = ppm_set_mute(argv[2], argv[3]);
        }
        ppm_context_close(&ctx);
        return r == 0 ? 0 : 1;
    }
    if (!strcmp(cmd, "default-sink") || !strcmp(cmd, "default-source")) {
        PpmContext ctx;
        int r;
        if (argc < 3) {
            fprintf(stderr, "Uso: audio-profile %s <nombre>\n", cmd);
            return 1;
        }
        if (open_registry(&ctx) != 0)
            return 1;
        r = !strcmp(cmd, "default-sink") ? ppm_set_default_sink(argv[2]) : ppm_set_default_source(argv[2]);
        ppm_context_close(&ctx);
        return r == 0 ? 0 : 1;
    }

    if (!strcmp(cmd, "wp-info")) {
        ppm_wp_core_print_info();
        return 0;
    }
    if (!strcmp(cmd, "wp-objects")) {
        ppm_wp_dump_objects();
        return 0;
    }
    if (!strcmp(cmd, "latency")) {
        latency_show();
        return 0;
    }
    if (!strcmp(cmd, "monitor")) {
        monitor_run();
        return 0;
    }
    if (!strcmp(cmd, "backup"))
        return backup_create() == 0 ? 0 : 1;

    fprintf(stderr, "Comando desconocido: %s\n\n", cmd);
    usage();
    return 2;
}
