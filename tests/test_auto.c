/* Reglas de perfiles automáticos y parsers de pactl / DSP. */
#include "test_fixture.h"
#include "test_common.h"

#include "atlas.h"
#include "atlas_audio.h"
#include "atlas_auto.h"
#include "atlas_dsp.h"

static AtlasAutoStream stream(const char *binary, const char *app, const char *title, const char *steam)
{
    AtlasAutoStream s;
    memset(&s, 0, sizeof(s));
    snprintf(s.binary, sizeof(s.binary), "%s", binary);
    snprintf(s.app, sizeof(s.app), "%s", app);
    snprintf(s.title, sizeof(s.title), "%s", title);
    snprintf(s.steam_appid, sizeof(s.steam_appid), "%s", steam);
    return s;
}

static void test_rules(void)
{
    AtlasAutoStream ra = stream("retroarch", "RetroArch", "", "");
    AtlasAutoStream yt = stream("brave", "Brave", "Lo-fi beats - YouTube - Brave", "");
    AtlasAutoStream ytm = stream("brave", "Brave", "Mix - YouTube Music - Brave", "");
    AtlasAutoStream apex = stream("r5apex.exe", "wine", "Apex Legends", "1172470");
    AtlasAutoStream other = stream("game.exe", "wine", "", "999999");

    TEST_ASSERT(atlas_auto_rule_matches("binary=retroarch", &ra));
    TEST_ASSERT(atlas_auto_rule_matches("binary=RETRO*", &ra));          /* glob sin mayúsculas */
    TEST_ASSERT(!atlas_auto_rule_matches("binary=retroarch", &yt));
    TEST_ASSERT(atlas_auto_rule_matches("app~retro", &ra));

    TEST_ASSERT(atlas_auto_rule_matches("title~YouTube", &yt));
    TEST_ASSERT(atlas_auto_rule_matches("binary=brave title~YouTube", &yt));
    TEST_ASSERT(!atlas_auto_rule_matches("binary=firefox title~YouTube", &yt));
    /* "_" equivale a espacio */
    TEST_ASSERT(atlas_auto_rule_matches("title~YouTube_Music", &ytm));
    TEST_ASSERT(!atlas_auto_rule_matches("title~YouTube_Music", &yt));

    TEST_ASSERT(atlas_auto_rule_matches("steam=1172470   # Apex", &apex));
    TEST_ASSERT(!atlas_auto_rule_matches("steam=730", &apex));
    TEST_ASSERT(atlas_auto_rule_matches("steam=*", &other));
    TEST_ASSERT(!atlas_auto_rule_matches("steam=*", &yt));                /* no es de Steam */
    TEST_ASSERT(atlas_auto_rule_matches("binary=*.exe", &other));

    /* MPRIS: la pestaña de YouTube suena en segundo plano (la ventana muestra otra) */
    {
        AtlasAutoStream bg = stream("brave", "Brave", "Gmail - Brave", "");
        snprintf(bg.mpris, sizeof(bg.mpris), "Lo-fi beats - YouTube");
        snprintf(bg.url, sizeof(bg.url), "https://www.youtube.com/watch?v=abc");
        TEST_ASSERT(atlas_auto_rule_matches("title~YouTube", &bg));          /* title~ mira MPRIS */
        TEST_ASSERT(atlas_auto_rule_matches("url~youtube.com", &bg));
        TEST_ASSERT(!atlas_auto_rule_matches("url~music.youtube.com", &bg));
        TEST_ASSERT(!atlas_auto_rule_matches("url~twitch.tv", &yt));          /* sin URL no coincide */
    }

    /* género automático de Steam */
    {
        AtlasAutoStream fh = stream("ForzaHorizon5.exe", "wine", "", "1551360");
        snprintf(fh.genre, sizeof(fh.genre), "racing");
        TEST_ASSERT(atlas_auto_rule_matches("genre=racing", &fh));
        TEST_ASSERT(atlas_auto_rule_matches("genre=RACING", &fh));
        TEST_ASSERT(!atlas_auto_rule_matches("genre=fps", &fh));
        TEST_ASSERT(!atlas_auto_rule_matches("genre=racing", &other));        /* sin clasificar */
    }

    TEST_ASSERT(!atlas_auto_rule_matches("desconocida=1", &ra));          /* nunca coincide */
    TEST_ASSERT(!atlas_auto_rule_matches("", &ra));                       /* vacía no coincide */
    TEST_ASSERT(!atlas_auto_rule_matches("# solo comentario", &ra));
    TEST_ASSERT(atlas_auto_rule_matches("any", &ra));
}

static const char *SINKS =
    "[{\"index\":55,\"state\":\"RUNNING\",\"name\":\"alsa_output.pci-0000_27_00.0.analog-surround-51\","
    "\"description\":\"CA0132 Analog Surround 5.1\",\"mute\":false,"
    "\"volume\":{\"front-left\":{\"value\":34658},\"front-right\":{\"value\":34658},"
    "\"lfe\":{\"value\":39794}},"
    "\"properties\":{\"device.api\":\"alsa\",\"alsa.card\":\"0\",\"alsa.device\":\"0\"}},"
    "{\"index\":60,\"state\":\"IDLE\",\"name\":\"bluez_output.AA_BB.1\",\"description\":\"Auriculares\","
    "\"mute\":true,\"volume\":{\"front-left\":{\"value\":65536}},"
    "\"properties\":{\"device.api\":\"bluez5\"}}]";

static const char *CARDS =
    "[{\"name\":\"alsa_card.pci-0000_27_00.0\",\"active_profile\":\"output:analog-surround-51+input:analog-stereo\","
    "\"properties\":{\"alsa.card\":\"0\",\"device.bus\":\"pci\",\"device.vendor.id\":\"0x1102\","
    "\"device.product.name\":\"CA0132 Sound Core3D\"},"
    "\"profiles\":{\"off\":{\"description\":\"Off\",\"sinks\":0,\"sources\":0,\"available\":true},"
    "\"output:analog-surround-51+input:analog-stereo\":{\"description\":\"5.1\",\"sinks\":1,\"sources\":1,"
    "\"available\":true}}}]";

static void test_pactl_parsers(void)
{
    AtlasAudio a;
    const AtlasNode *s;

    memset(&a, 0, sizeof(a));
    TEST_ASSERT_EQ(atlas_audio_parse_info(&a,
        "{\"server_name\":\"PulseAudio (on PipeWire 1.6.10)\","
        "\"default_sink_name\":\"alsa_output.pci-0000_27_00.0.analog-surround-51\","
        "\"default_source_name\":\"alsa_input.pci-0000_27_00.0.analog-stereo\"}"), 0);
    TEST_ASSERT_EQ(atlas_audio_parse_nodes(a.sinks, ATLAS_MAX_NODES, &a.nsinks, SINKS), 0);
    TEST_ASSERT_EQ(atlas_audio_parse_cards(&a, CARDS), 0);

    TEST_ASSERT_EQ_INT(2, a.nsinks);
    s = atlas_audio_default_sink(&a);
    TEST_ASSERT(s != NULL);
    TEST_ASSERT_EQ_INT(0, s->alsa_card);
    TEST_ASSERT_EQ_INT(0, s->alsa_device);
    TEST_ASSERT_EQ_STR("alsa_card.pci-0000_27_00.0", s->card_name);
    TEST_ASSERT_EQ_INT(3, s->volume.channels);
    TEST_ASSERT_EQ_INT(39794, s->volume.value[2]);     /* volumen por canal conservado */
    TEST_ASSERT_EQ_INT(-1, a.sinks[1].alsa_card);
    TEST_ASSERT_EQ_INT(1, a.sinks[1].mute);

    TEST_ASSERT_EQ_INT(1, a.ncards);
    TEST_ASSERT_EQ_INT(2, a.cards[0].nprofiles);
    TEST_ASSERT(atlas_audio_card_by_alsa(&a, 0) == &a.cards[0]);
    TEST_ASSERT(atlas_audio_card_by_alsa(&a, 4) == NULL);

    TEST_ASSERT_EQ(atlas_audio_parse_nodes(a.sinks, ATLAS_MAX_NODES, &a.nsinks, "no json"), -1);

    /* Regresión: 5.1 real de la Sound Blaster Z. El orden de canales debe
     * ser el del JSON (FL FR RL RR FC LFE), no el de la tabla hash. */
    {
        static const char *S51 =
            "[{\"name\":\"s51\",\"description\":\"5.1\",\"state\":\"IDLE\",\"mute\":false,"
            "\"volume\":{\"front-left\":{\"value\":1},\"front-right\":{\"value\":2},"
            "\"rear-left\":{\"value\":3},\"rear-right\":{\"value\":4},"
            "\"front-center\":{\"value\":5},\"lfe\":{\"value\":6}},\"properties\":{}}]";
        int k;
        TEST_ASSERT_EQ(atlas_audio_parse_nodes(a.sinks, ATLAS_MAX_NODES, &a.nsinks, S51), 0);
        TEST_ASSERT_EQ_INT(6, a.sinks[0].volume.channels);
        for (k = 0; k < 6; k++)
            TEST_ASSERT_EQ_INT(k + 1, a.sinks[0].volume.value[k]);
    }
}

static void test_snapshot_roundtrip(void)
{
    AtlasAudio a, b;
    char path[1024];

    memset(&a, 0, sizeof(a));
    atlas_audio_parse_nodes(a.sinks, ATLAS_MAX_NODES, &a.nsinks, SINKS);
    atlas_audio_parse_cards(&a, CARDS);
    a.server_ok = 1;
    snprintf(a.default_sink, sizeof(a.default_sink), "%s", a.sinks[0].name);

    fx_setup();
    fx_path(path, sizeof(path), "snap.json");
    TEST_ASSERT_EQ(atlas_audio_save_snapshot(&a, path), 0);
    TEST_ASSERT_EQ(atlas_audio_load_snapshot(&b, path), 0);
    TEST_ASSERT_EQ_STR(a.default_sink, b.default_sink);
    TEST_ASSERT_EQ_INT(2, b.nsinks);
    TEST_ASSERT_EQ_INT(3, b.sinks[0].volume.channels);
    TEST_ASSERT_EQ_INT(39794, b.sinks[0].volume.value[2]);
    TEST_ASSERT_EQ_STR("output:analog-surround-51+input:analog-stereo", b.cards[0].active_profile);
    fx_teardown();
}

static void test_dsp_parsers(void)
{
    static AtlasDspState s;
    char names[8][128], values[8][128];
    const char *contents =
        "numid=40,iface=MIXER,name='Bass Redirection Crossover'\n"
        "  ; type=INTEGER,access=rw---R--,values=1,min=1,max=100,step=1\n"
        "  : values=50\n"
        "numid=26,iface=MIXER,name='FX: Equalizer Preset Switch'\n"
        "  ; type=ENUMERATED,access=rw------,values=1,items=3\n"
        "  ; Item #0 'Flat'\n  ; Item #1 'Acoustic'\n  ; Item #2 'Classical'\n"
        "  : values=0\n"
        "numid=61,iface=CARD,name='Front Headphone Jack'\n"
        "  ; type=BOOLEAN,access=r-------,values=1\n  : values=on\n";
    const char *preset =
        "# comentario\n"
        "Bass Redirection Crossover=12\n"
        "  FX: Equalizer Preset Switch = Flat  \n"
        "linea sin igual\n"
        "Malo=`rm -rf`\n";
    int n;

    TEST_ASSERT_EQ_INT(2, atlas_dsp_parse_contents(contents, &s));   /* solo iface=MIXER */
    TEST_ASSERT_EQ_STR("Bass Redirection Crossover", s.c[0].name);
    TEST_ASSERT_EQ_INT(1, s.c[0].min);
    TEST_ASSERT_EQ_INT(100, s.c[0].max);
    TEST_ASSERT_EQ_STR("50", s.c[0].values);
    TEST_ASSERT_EQ_INT(3, s.c[1].nitems);
    TEST_ASSERT_EQ_STR("Classical", s.c[1].items[2]);

    n = atlas_dsp_parse_preset(preset, names, values, 8);
    TEST_ASSERT_EQ_INT(2, n);
    TEST_ASSERT_EQ_STR("FX: Equalizer Preset Switch", names[1]);
    TEST_ASSERT_EQ_STR("Flat", values[1]);
}

static void test_manual_release(void)
{
    AtlasAutoState s;

    memset(&s, 0, sizeof(s));
    s.enabled = 1;
    TEST_ASSERT_EQ_STR("auto", atlas_auto_mode(&s));
    TEST_ASSERT_EQ_INT(0, atlas_auto_should_release(&s, "video"));   /* sin pausa: nada que soltar */

    /* viendo YouTube (el automático elegía video) eliges music a mano */
    s.manual_lock = 1;
    strcpy(s.current, "music");
    strcpy(s.lock_winner, "video");
    TEST_ASSERT_EQ_STR("auto-paused", atlas_auto_mode(&s));
    TEST_ASSERT_EQ_INT(0, atlas_auto_should_release(&s, "video"));   /* sigue el mismo vídeo */
    TEST_ASSERT_EQ_INT(0, atlas_auto_should_release(&s, ""));        /* deja de sonar */
    TEST_ASSERT_EQ_INT(1, atlas_auto_should_release(&s, "gaming"));  /* abres un juego */

    /* elegido sin nada sonando: lo primero que suene lo suelta */
    s.lock_winner[0] = '\0';
    TEST_ASSERT_EQ_INT(1, atlas_auto_should_release(&s, "video"));

    /* modo manual: nunca se suelta solo */
    s.enabled = 0;
    TEST_ASSERT_EQ_STR("manual", atlas_auto_mode(&s));
    TEST_ASSERT_EQ_INT(0, atlas_auto_should_release(&s, "gaming"));
}

int main(void)
{
    test_manual_release();
    test_rules();
    test_pactl_parsers();
    test_snapshot_roundtrip();
    test_dsp_parsers();
    return test_summary();
}
