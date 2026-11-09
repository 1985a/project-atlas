#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests = 0;
static int g_failures = 0;

#define TEST_ASSERT(expr)                                      \
do {                                                           \
    g_tests++;                                                 \
    if (!(expr)) {                                             \
        g_failures++;                                          \
        fprintf(stderr,"[FAIL] %s:%d: %s\n",                   \
                __FILE__,__LINE__,#expr);                      \
    }                                                          \
} while (0)

#define TEST_ASSERT_EQ(a,b) TEST_ASSERT((a)==(b))
#define TEST_ASSERT_NE(a,b) TEST_ASSERT((a)!=(b))
#define TEST_ASSERT_NULL(p) TEST_ASSERT((p)==NULL)
#define TEST_ASSERT_NOT_NULL(p) TEST_ASSERT((p)!=NULL)

#define TEST_ASSERT_EQ_INT(expected,actual)                    \
do {                                                           \
    long long e_ = (long long)(expected);                       \
    long long a_ = (long long)(actual);                         \
    g_tests++;                                                 \
    if (e_ != a_) {                                            \
        g_failures++;                                          \
        fprintf(stderr,                                       \
                "[FAIL] %s:%d\nExpected: %lld\nActual  : %lld\n", \
                __FILE__,__LINE__,e_,a_);                      \
    }                                                          \
} while (0)

#define TEST_ASSERT_EQ_STR(expected,actual)                    \
do {                                                           \
    const char *e_ = (expected);                                \
    const char *a_ = (actual);                                  \
    g_tests++;                                                 \
    if (!e_ || !a_ || strcmp(e_,a_) != 0) {                     \
        g_failures++;                                          \
        fprintf(stderr,                                       \
                "[FAIL] %s:%d\nExpected: \"%s\"\nActual  : \"%s\"\n", \
                __FILE__,__LINE__,                            \
                e_ ? e_ : "(null)",                           \
                a_ ? a_ : "(null)");                          \
    }                                                          \
} while (0)

static inline int test_summary(void)
{
    printf("\nTests   : %d\n", g_tests);
    printf("Failures: %d\n", g_failures);

    if (g_failures == 0)
        printf("[PASS]\n");
    else
        printf("[FAIL]\n");

    return g_failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

#endif
