/* Clasificación de juegos de Steam (datos reales de SteamSpy y de la tienda). */
#include "test_common.h"

#include "atlas.h"
#include "atlas_steam.h"

static void expect(const char *json, const char *want)
{
    char cls[32], name[64];
    TEST_ASSERT_EQ(atlas_steam_classify_json(json, cls, sizeof(cls), name, sizeof(name)), 0);
    TEST_ASSERT_EQ_STR(want, cls);
}

static void test_steamspy(void)
{
    char cls[32];

    expect("{\"name\":\"Forza Horizon 5\",\"tags\":{\"Racing\":1046,\"Open World\":816,\"Driving\":766,"
           "\"Multiplayer\":696,\"Automobile Sim\":611,\"Adventure\":470,\"Exploration\":396}}", "racing");
    expect("{\"name\":\"CS\",\"tags\":{\"FPS\":91172,\"Shooter\":65634,\"Multiplayer\":62536,"
           "\"Competitive\":53536,\"Action\":47634,\"Tactical\":41468}}", "fps");
    expect("{\"name\":\"Witcher 3\",\"tags\":{\"Open World\":12801,\"RPG\":10801,\"Story Rich\":10011,"
           "\"Atmospheric\":6881,\"Fantasy\":6585,\"Adventure\":6454}}", "adventure");
    expect("{\"name\":\"PUBG\",\"tags\":{\"Survival\":14893,\"Shooter\":12788,\"Battle Royale\":10936,"
           "\"FPS\":8483,\"Third-Person Shooter\":7341}}", "fps");
    expect("{\"name\":\"RE4\",\"tags\":{\"Horror\":554,\"Action\":538,\"Survival Horror\":500,"
           "\"Third-Person Shooter\":483,\"Story Rich\":390}}", "adventure");
    /* RPG presente, pero pesa más lo relajado/granja */
    expect("{\"name\":\"Stardew\",\"tags\":{\"Farming Sim\":7935,\"Pixel Graphics\":7389,\"Life Sim\":7246,"
           "\"RPG\":6530,\"Relaxing\":6286,\"Simulation\":5307}}", "gaming");

    /* sin etiquetas (SteamSpy devuelve []): usa el género genérico */
    expect("{\"name\":\"X\",\"genre\":\"Action, Racing\",\"tags\":[]}", "racing");
    expect("{\"name\":\"OW2\",\"genre\":\"Action, Free To Play\",\"tags\":[]}", "gaming");

    TEST_ASSERT_EQ(atlas_steam_classify_json("{\"tags\":[]}", cls, sizeof(cls), NULL, 0), -1);
    TEST_ASSERT_EQ(atlas_steam_classify_json("no json", cls, sizeof(cls), NULL, 0), -1);
}

static void test_store_html(void)
{
    char cls[32];
    /* fragmento real de store.steampowered.com/app/2357570 (Overwatch 2) */
    const char *html =
        "<script>$J( function() { InitAppTagModal( 2357570,\n"
        "  [{\"tagid\":113,\"name\":\"Free to Play\",\"count\":751,\"browseable\":true},"
        "{\"tagid\":620519,\"name\":\"Hero Shooter\",\"count\":591,\"browseable\":true},"
        "{\"tagid\":1663,\"name\":\"FPS\",\"count\":586,\"browseable\":true},"
        "{\"tagid\":3859,\"name\":\"Multiplayer\",\"count\":581,\"browseable\":true},"
        "{\"tagid\":1718,\"name\":\"MOBA\",\"count\":516,\"browseable\":true}],\n"
        "  [{\"tagid\":1,\"name\":\"x\"}], 'x'); });</script>";

    TEST_ASSERT_EQ(atlas_steam_classify_store_html(html, cls, sizeof(cls)), 0);
    TEST_ASSERT_EQ_STR("fps", cls);
    TEST_ASSERT_EQ(atlas_steam_classify_store_html("<html>sin etiquetas</html>", cls, sizeof(cls)), -1);
    TEST_ASSERT_EQ(atlas_steam_classify_store_html("InitAppTagModal( 1, [ {", cls, sizeof(cls)), -1);
}

static void test_genre_input_validation(void)
{
    char cls[32];
    /* appid no numérico: nunca llega a la red ni a la caché */
    TEST_ASSERT_EQ(atlas_steam_genre("../../etc", cls, sizeof(cls)), -1);
    TEST_ASSERT_EQ(atlas_steam_genre("", cls, sizeof(cls)), -1);
    TEST_ASSERT_EQ(atlas_steam_genre("123;rm", cls, sizeof(cls)), -1);
}

/* En el daemon la consulta no puede bloquear: vuelve al instante sin clase. */
static void test_background_lookup_does_not_block(void)
{
    char cls[32] = "x";
    long long t0;

    atlas_steam_set_background_lookups("/bin/true");
    t0 = atlas_now_ms();
    TEST_ASSERT_EQ_INT(-1, atlas_steam_genre("1234567", cls, sizeof(cls)));
    TEST_ASSERT_EQ_INT(-1, atlas_steam_genre("1234567", cls, sizeof(cls)));   /* no se relanza */
    TEST_ASSERT(atlas_now_ms() - t0 < 500);
    TEST_ASSERT_EQ_STR("", cls);
    atlas_steam_set_background_lookups(NULL);
}

int main(void)
{
    test_background_lookup_does_not_block();
    test_steamspy();
    test_store_html();
    test_genre_input_validation();
    return test_summary();
}
