/* Base de volumen: parser de archivos alsactl y clasificación de controles. */
#include "test_common.h"

#include "atlas.h"
#include "atlas_base.h"

/* Extracto con el formato real de ~/.config/asound.state del usuario */
static const char *STATE =
    "state.Creative {\n"
    "\tcontrol.1 {\n"
    "\t\tiface MIXER\n"
    "\t\tname 'Front Playback Volume'\n"
    "\t\tvalue.0 95\n"
    "\t\tvalue.1 95\n"
    "\t\tcomment {\n"
    "\t\t\taccess 'read write'\n"
    "\t\t\ttype INTEGER\n"
    "\t\t\tcount 2\n"
    "\t\t\trange '0 - 99'\n"
    "\t\t\tdbvalue.0 500\n"
    "\t\t\tdbvalue.1 500\n"
    "\t\t}\n"
    "\t}\n"
    "\tcontrol.2 {\n"
    "\t\tiface MIXER\n"
    "\t\tname 'Front Playback Switch'\n"
    "\t\tvalue.0 true\n"
    "\t\tvalue.1 true\n"
    "\t\tcomment {\n\t\t\taccess 'read write'\n\t\t\ttype BOOLEAN\n\t\t\tcount 2\n\t\t}\n"
    "\t}\n"
    "\tcontrol.3 {\n"
    "\t\tiface MIXER\n"
    "\t\tname 'FX: Equalizer Preset Switch'\n"
    "\t\tvalue Dance\n"
    "\t\tcomment {\n\t\t\taccess 'read write'\n\t\t\ttype ENUMERATED\n\t\t\tcount 1\n"
    "\t\t\titem.0 Flat\n\t\t\titem.4 Dance\n\t\t}\n"
    "\t}\n"
    "\tcontrol.4 {\n"
    "\t\tiface MIXER\n"
    "\t\tname 'Surround Channel Config'\n"
    "\t\tvalue '5.1'\n"
    "\t\tcomment { access 'read write' type ENUMERATED count 1 }\n"
    "\t}\n"
    "\tcontrol.5 {\n"
    "\t\tiface CARD\n"
    "\t\tname 'Mic Jack'\n"
    "\t\tvalue true\n"
    "\t}\n"
    "\tcontrol.6 {\n"
    "\t\tiface MIXER\n"
    "\t\tname 'Wedge Angle Capture Volume'\n"
    "\t\tvalue 10\n"
    "\t}\n"
    "\tcontrol.7 {\n"
    "\t\tiface MIXER\n"
    "\t\tname 'Mic Boost Capture Switch'\n"
    "\t\tvalue '0 dB'\n"
    "\t}\n"
    "\tcontrol.8 {\n"
    "\t\tiface MIXER\n"
    "\t\tname 'Master Playback Volume'\n"
    "\t\tvalue 90\n"
    "\t}\n"
    "}\n"
    "state.Generic {\n"
    "\tcontrol.1 {\n"
    "\t\tiface MIXER\n"
    "\t\tname 'Front Playback Volume'\n"
    "\t\tvalue.0 64\n"
    "\t\tvalue.1 64\n"
    "\t}\n"
    "}\n";

static const AtlasStateCtl *find(const AtlasStateCard *s, const char *name)
{
    int i;
    for (i = 0; i < s->n; i++)
        if (!strcmp(s->c[i].name, name))
            return &s->c[i];
    return NULL;
}

static void test_parse(void)
{
    static AtlasStateCard s;
    const AtlasStateCtl *c;

    TEST_ASSERT_EQ(atlas_state_parse(STATE, "Creative", &s), 0);
    TEST_ASSERT_EQ_INT(8, s.n);

    c = find(&s, "Front Playback Volume");
    TEST_ASSERT(c != NULL);
    TEST_ASSERT_EQ_STR("95,95", c ? c->value : NULL);       /* sin los dbvalue del comment */
    c = find(&s, "Front Playback Switch");
    TEST_ASSERT_EQ_STR("on,on", c ? c->value : NULL);       /* true -> on */
    c = find(&s, "FX: Equalizer Preset Switch");
    TEST_ASSERT_EQ_STR("Dance", c ? c->value : NULL);       /* no toma item.4 del comment */
    c = find(&s, "Surround Channel Config");
    TEST_ASSERT_EQ_STR("5.1", c ? c->value : NULL);         /* comment en una línea, comillas fuera */
    c = find(&s, "Mic Boost Capture Switch");
    TEST_ASSERT_EQ_STR("0 dB", c ? c->value : NULL);

    /* otra tarjeta del mismo archivo, sin mezclar */
    TEST_ASSERT_EQ(atlas_state_parse(STATE, "Generic", &s), 0);
    TEST_ASSERT_EQ_INT(1, s.n);
    TEST_ASSERT_EQ_STR("64,64", s.c[0].value);

    TEST_ASSERT_EQ(atlas_state_parse(STATE, "NoExiste", &s), -1);
    TEST_ASSERT_EQ(atlas_state_parse(STATE, "Creativ", &s), -1);   /* prefijo no basta */
}

static void test_classify(void)
{
    static AtlasStateCard s;

    atlas_state_parse(STATE, "Creative", &s);
    TEST_ASSERT_EQ_INT(ATLAS_CTL_VOLUME, atlas_ctl_classify(find(&s, "Front Playback Volume")));
    TEST_ASSERT_EQ_INT(ATLAS_CTL_VOLUME, atlas_ctl_classify(find(&s, "Front Playback Switch")));
    TEST_ASSERT_EQ_INT(ATLAS_CTL_VOLUME, atlas_ctl_classify(find(&s, "Master Playback Volume")));
    TEST_ASSERT_EQ_INT(ATLAS_CTL_VOLUME, atlas_ctl_classify(find(&s, "Mic Boost Capture Switch")));
    TEST_ASSERT_EQ_INT(ATLAS_CTL_EFFECT, atlas_ctl_classify(find(&s, "FX: Equalizer Preset Switch")));
    TEST_ASSERT_EQ_INT(ATLAS_CTL_SPEAKER, atlas_ctl_classify(find(&s, "Surround Channel Config")));
    TEST_ASSERT_EQ_INT(ATLAS_CTL_IGNORE, atlas_ctl_classify(find(&s, "Mic Jack")));
    /* CA0132-0001: valor fuera de rango guardado, nunca se reaplica */
    TEST_ASSERT_EQ_INT(ATLAS_CTL_IGNORE, atlas_ctl_classify(find(&s, "Wedge Angle Capture Volume")));
}

static void test_value_matches(void)
{
    TEST_ASSERT_EQ_INT(1, atlas_value_matches("95,95", "95,95"));
    TEST_ASSERT_EQ_INT(1, atlas_value_matches("90", "90,90"));      /* un valor = todos los canales */
    TEST_ASSERT_EQ_INT(0, atlas_value_matches("90", "90,80"));
    TEST_ASSERT_EQ_INT(1, atlas_value_matches("on,on", "on,on"));
    TEST_ASSERT_EQ_INT(1, atlas_value_matches("true", "on"));
    TEST_ASSERT_EQ_INT(0, atlas_value_matches("on", "off"));
    TEST_ASSERT_EQ_INT(1, atlas_value_matches("Dance", "Dance"));
    TEST_ASSERT_EQ_INT(0, atlas_value_matches("Dance", "Flat"));
    TEST_ASSERT_EQ_INT(0, atlas_value_matches("66", "50"));
    TEST_ASSERT_EQ_INT(0, atlas_value_matches("95,95,95", "95,95"));
}

static void test_diff(void)
{
    static AtlasStateCard live;
    static char names[4][128] = { "Master Playback Volume", "FX: Smart Volume Playback Volume",
                                  "No Existe Volume", "FX: Smart Volume Playback Volume" };
    static char values[4][128] = { "90", "50", "1", "66" };
    int out[4], nd;

    memset(&live, 0, sizeof(live));
    live.n = 2;
    strcpy(live.c[0].iface, "MIXER");
    strcpy(live.c[0].name, "Master Playback Volume");
    strcpy(live.c[0].value, "90");
    strcpy(live.c[1].iface, "MIXER");
    strcpy(live.c[1].name, "FX: Smart Volume Playback Volume");
    strcpy(live.c[1].value, "50");
    /* la capa posterior (66, tus efectos) manda sobre la anterior (50) */
    nd = atlas_base_diff(names, values, 4, &live, out);
    TEST_ASSERT_EQ_INT(1, nd);
    TEST_ASSERT_EQ_INT(3, out[0]);
    /* solo lectura: se ignora */
    live.c[1].iface[0] = '\0';
    TEST_ASSERT_EQ_INT(0, atlas_base_diff(names, values, 4, &live, out));
}

int main(void)
{
    test_parse();
    test_classify();
    test_value_matches();
    test_diff();
    return test_summary();
}
