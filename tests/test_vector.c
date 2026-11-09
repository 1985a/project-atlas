#include "common/ppm_vector.h"
#include "test_common.h"

static void test_init(void)
{
    PpmVector v;

    TEST_ASSERT_EQ(ppm_vector_init(&v), PPM_OK);
    TEST_ASSERT_EQ(ppm_vector_size(&v), 0);
    TEST_ASSERT_EQ(ppm_vector_capacity(&v), 0);
    TEST_ASSERT(ppm_vector_empty(&v));

    ppm_vector_destroy(&v);
}

static void test_push_pop(void)
{
    PpmVector v;

    ppm_vector_init(&v);

    int a = 10;
    int b = 20;

    ppm_vector_push(&v, &a);
    ppm_vector_push(&v, &b);

    TEST_ASSERT_EQ(ppm_vector_size(&v), 2);

    TEST_ASSERT_EQ(ppm_vector_back(&v), &b);
    TEST_ASSERT_EQ(ppm_vector_front(&v), &a);

    TEST_ASSERT_EQ(ppm_vector_pop(&v), &b);
    TEST_ASSERT_EQ(ppm_vector_pop(&v), &a);
    TEST_ASSERT_NULL(ppm_vector_pop(&v));

    ppm_vector_destroy(&v);
}

static void test_reserve(void)
{
    PpmVector v;

    ppm_vector_init(&v);

    TEST_ASSERT_EQ(
        ppm_vector_reserve(&v,64),
        PPM_OK);

    TEST_ASSERT(ppm_vector_capacity(&v) >= 64);

    ppm_vector_destroy(&v);
}

int main(void)
{
    test_init();
    test_push_pop();
    test_reserve();

    return test_summary();
}
