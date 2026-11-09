#include "core/ppm_runtime.h"
#include "test_common.h"

typedef struct
{
    int opened;
    int scanned;
    int closed;
} RuntimeMockData;

static PpmResult mock_open(PpmBackend *backend)
{
    RuntimeMockData *data = ppm_backend_userdata(backend);
    data->opened++;
    return PPM_OK;
}

static PpmResult mock_scan(PpmBackend *backend)
{
    RuntimeMockData *data = ppm_backend_userdata(backend);
    data->scanned++;
    return PPM_OK;
}

static void mock_close(PpmBackend *backend)
{
    RuntimeMockData *data = ppm_backend_userdata(backend);
    data->closed++;
}

static const PpmBackendOps mock_ops = {
    .name = "runtime-mock",
    .open = mock_open,
    .scan = mock_scan,
    .close = mock_close,
};

static void test_create_destroy(void)
{
    PpmRuntime *rt = NULL;

    TEST_ASSERT_EQ(ppm_runtime_create(&rt), PPM_OK);
    TEST_ASSERT_NOT_NULL(rt);
    TEST_ASSERT_EQ(PPM_RUNTIME_STATE_INITIALIZED, ppm_runtime_state(rt));

    ppm_runtime_destroy(rt);
}

static void test_null_arguments(void)
{
    PpmRuntime *rt = NULL;
    PpmBackend backend;

    TEST_ASSERT_EQ(ppm_runtime_create(NULL), PPM_INVALID_ARGUMENT);
    TEST_ASSERT_EQ(ppm_runtime_set_backend(NULL,&backend), PPM_INVALID_ARGUMENT);
    TEST_ASSERT_EQ(ppm_runtime_set_backend(rt,NULL), PPM_INVALID_ARGUMENT);
    TEST_ASSERT_EQ(ppm_runtime_scan(NULL), PPM_INVALID_ARGUMENT);
    TEST_ASSERT_EQ(PPM_RUNTIME_STATE_EMPTY, ppm_runtime_state(NULL));
}

static void test_scan_without_backend(void)
{
    PpmRuntime *rt = NULL;

    TEST_ASSERT_EQ(ppm_runtime_create(&rt), PPM_OK);
    TEST_ASSERT_EQ(ppm_runtime_scan(rt), PPM_UNSUPPORTED);
    TEST_ASSERT_EQ(PPM_RUNTIME_STATE_INITIALIZED, ppm_runtime_state(rt));

    ppm_runtime_destroy(rt);
}

static void test_scan_with_mock_backend(void)
{
    PpmRuntime *rt = NULL;
    PpmBackend backend;
    RuntimeMockData data = {0};

    TEST_ASSERT_EQ(ppm_runtime_create(&rt), PPM_OK);
    TEST_ASSERT_EQ(ppm_backend_init(&backend,&mock_ops,&data), PPM_OK);
    TEST_ASSERT_EQ(ppm_runtime_set_backend(rt,&backend), PPM_OK);

    TEST_ASSERT_EQ(ppm_runtime_scan(rt), PPM_OK);

    TEST_ASSERT_EQ_INT(1, data.opened);
    TEST_ASSERT_EQ_INT(1, data.scanned);
    TEST_ASSERT_EQ_INT(0, data.closed);
    TEST_ASSERT_EQ(PPM_RUNTIME_STATE_SCANNED, ppm_runtime_state(rt));

    ppm_runtime_destroy(rt);

    TEST_ASSERT_EQ_INT(1, data.closed);
}

static void test_multiple_create_destroy(void)
{
    for (int i = 0; i < 32; ++i)
    {
        PpmRuntime *rt = NULL;

        TEST_ASSERT_EQ(ppm_runtime_create(&rt), PPM_OK);
        TEST_ASSERT_NOT_NULL(rt);
        TEST_ASSERT_EQ(PPM_RUNTIME_STATE_INITIALIZED, ppm_runtime_state(rt));

        ppm_runtime_destroy(rt);
    }
}

int main(void)
{
    test_create_destroy();
    test_null_arguments();
    test_scan_without_backend();
    test_scan_with_mock_backend();
    test_multiple_create_destroy();

    return test_summary();
}
