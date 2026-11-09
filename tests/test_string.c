#include "common/ppm_string.h"
#include "test_common.h"

static void test_init(void)
{
    PpmString s;

    TEST_ASSERT_EQ(ppm_string_init(&s), PPM_OK);
    TEST_ASSERT_EQ_INT(0, ppm_string_size(&s));
    TEST_ASSERT_EQ_STR("", ppm_string_c_str(&s));

    ppm_string_destroy(&s);
}

static void test_assign(void)
{
    PpmString s;

    ppm_string_init(&s);

    TEST_ASSERT_EQ(ppm_string_assign(&s, "PipeWire"), PPM_OK);
    TEST_ASSERT_EQ_INT(8, ppm_string_size(&s));
    TEST_ASSERT_EQ_STR("PipeWire", ppm_string_c_str(&s));

    ppm_string_destroy(&s);
}

static void test_assign_empty(void)
{
    PpmString s;

    ppm_string_init(&s);

    TEST_ASSERT_EQ(ppm_string_assign(&s, ""), PPM_OK);
    TEST_ASSERT_EQ_INT(0, ppm_string_size(&s));
    TEST_ASSERT_EQ_STR("", ppm_string_c_str(&s));

    ppm_string_destroy(&s);
}

static void test_assign_null(void)
{
    PpmString s;

    ppm_string_init(&s);

    TEST_ASSERT_EQ(ppm_string_assign(&s, NULL), PPM_OK);
    TEST_ASSERT_EQ_INT(0, ppm_string_size(&s));
    TEST_ASSERT_EQ_STR("", ppm_string_c_str(&s));

    ppm_string_destroy(&s);
}

static void test_reassign_shorter(void)
{
    PpmString s;

    ppm_string_init(&s);

    ppm_string_assign(&s, "abc");
    ppm_string_assign(&s, "x");

    TEST_ASSERT_EQ_INT(1, ppm_string_size(&s));
    TEST_ASSERT_EQ_STR("x", ppm_string_c_str(&s));

    ppm_string_destroy(&s);
}

static void test_reassign_longer(void)
{
    PpmString s;

    ppm_string_init(&s);

    ppm_string_assign(&s, "abc");
    ppm_string_assign(&s, "abcdefghijklmnopqrstuvwxyz");

    TEST_ASSERT_EQ_INT(26, ppm_string_size(&s));
    TEST_ASSERT_EQ_STR("abcdefghijklmnopqrstuvwxyz", ppm_string_c_str(&s));

    ppm_string_destroy(&s);
}

static void test_append(void)
{
    PpmString s;

    ppm_string_init(&s);

    ppm_string_append(&s, "Pipe");
    ppm_string_append(&s, "Wire");

    TEST_ASSERT_EQ_INT(8, ppm_string_size(&s));
    TEST_ASSERT_EQ_STR("PipeWire", ppm_string_c_str(&s));

    ppm_string_destroy(&s);
}

static void test_append_after_empty_assign(void)
{
    PpmString s;

    ppm_string_init(&s);

    ppm_string_assign(&s, "");
    ppm_string_append(&s, "abc");

    TEST_ASSERT_EQ_INT(3, ppm_string_size(&s));
    TEST_ASSERT_EQ_STR("abc", ppm_string_c_str(&s));

    ppm_string_destroy(&s);
}

static void test_append_many(void)
{
    PpmString s;

    ppm_string_init(&s);

    ppm_string_append(&s, "A");
    TEST_ASSERT_EQ_STR("A", ppm_string_c_str(&s));

    ppm_string_append(&s, "B");
    TEST_ASSERT_EQ_STR("AB", ppm_string_c_str(&s));

    ppm_string_append(&s, "C");
    TEST_ASSERT_EQ_STR("ABC", ppm_string_c_str(&s));

    ppm_string_append(&s, "D");
    TEST_ASSERT_EQ_STR("ABCD", ppm_string_c_str(&s));

    TEST_ASSERT_EQ_INT(4, ppm_string_size(&s));

    ppm_string_destroy(&s);
}

static void test_clear(void)
{
    PpmString s;

    ppm_string_init(&s);

    ppm_string_assign(&s, "PipeWire");
    ppm_string_clear(&s);

    TEST_ASSERT_EQ_INT(0, ppm_string_size(&s));
    TEST_ASSERT_EQ_STR("", ppm_string_c_str(&s));

    ppm_string_assign(&s, "Audio");

    TEST_ASSERT_EQ_INT(5, ppm_string_size(&s));
    TEST_ASSERT_EQ_STR("Audio", ppm_string_c_str(&s));

    ppm_string_destroy(&s);
}

static void test_clear_then_append(void)
{
    PpmString s;

    ppm_string_init(&s);

    ppm_string_assign(&s, "PipeWire");
    ppm_string_clear(&s);
    ppm_string_append(&s, "Audio");

    TEST_ASSERT_EQ_INT(5, ppm_string_size(&s));
    TEST_ASSERT_EQ_STR("Audio", ppm_string_c_str(&s));

    ppm_string_destroy(&s);
}

static void test_growth(void)
{
    PpmString s;

    ppm_string_init(&s);

    for (int i = 0; i < 1000; ++i)
        TEST_ASSERT_EQ(ppm_string_append(&s, "x"), PPM_OK);

    TEST_ASSERT_EQ_INT(1000, ppm_string_size(&s));
    TEST_ASSERT_EQ('x', ppm_string_c_str(&s)[0]);
    TEST_ASSERT_EQ('x', ppm_string_c_str(&s)[999]);
    TEST_ASSERT_EQ('\0', ppm_string_c_str(&s)[1000]);

    ppm_string_destroy(&s);
}

int main(void)
{
    test_init();
    test_assign();
    test_assign_empty();
    test_assign_null();
    test_reassign_shorter();
    test_reassign_longer();
    test_append();
    test_append_after_empty_assign();
    test_append_many();
    test_clear();
    test_clear_then_append();
    test_growth();

    return test_summary();
}
