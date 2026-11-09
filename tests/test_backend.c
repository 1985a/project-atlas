#include "core/ppm_backend.h"
#include "test_common.h"

typedef struct
{
    int opened;
    int scanned;
    int closed;
} MockBackendData;

static PpmResult mock_open(PpmBackend *backend)
{
    MockBackendData *d = ppm_backend_userdata(backend);
    d->opened++;
    return PPM_OK;
}

static PpmResult mock_scan(PpmBackend *backend)
{
    MockBackendData *d = ppm_backend_userdata(backend);
    d->scanned++;
    return PPM_OK;
}

static void mock_close(PpmBackend *backend)
{
    MockBackendData *d = ppm_backend_userdata(backend);
    d->closed++;
}

static void test_init(void)
{
    PpmBackend backend;
    MockBackendData data = {0};

    static const PpmBackendOps ops = {
        .name = "mock",
        .open = mock_open,
        .scan = mock_scan,
        .close = mock_close,
    };

    TEST_ASSERT_EQ(ppm_backend_init(&backend,&ops,&data), PPM_OK);
    TEST_ASSERT_EQ_STR("mock", ppm_backend_name(&backend));
    TEST_ASSERT_EQ(ppm_backend_userdata(&backend), &data);
}

static void test_null_arguments(void)
{
    PpmBackend backend;

    TEST_ASSERT_EQ(ppm_backend_init(NULL,NULL,NULL), PPM_INVALID_ARGUMENT);
    TEST_ASSERT_EQ(ppm_backend_init(&backend,NULL,NULL), PPM_INVALID_ARGUMENT);
    TEST_ASSERT_EQ_STR("", ppm_backend_name(NULL));
    TEST_ASSERT_NULL(ppm_backend_userdata(NULL));
    TEST_ASSERT_EQ(ppm_backend_open(NULL), PPM_INVALID_ARGUMENT);
    TEST_ASSERT_EQ(ppm_backend_scan(NULL), PPM_INVALID_ARGUMENT);
}

static void test_open_scan_close(void)
{
    PpmBackend backend;
    MockBackendData data = {0};

    static const PpmBackendOps ops = {
        .name = "mock",
        .open = mock_open,
        .scan = mock_scan,
        .close = mock_close,
    };

    TEST_ASSERT_EQ(ppm_backend_init(&backend,&ops,&data), PPM_OK);

    TEST_ASSERT_EQ(ppm_backend_open(&backend), PPM_OK);
    TEST_ASSERT_EQ_INT(1, data.opened);
    TEST_ASSERT_EQ_INT(1, backend.is_open);

    TEST_ASSERT_EQ(ppm_backend_scan(&backend), PPM_OK);
    TEST_ASSERT_EQ_INT(1, data.scanned);

    ppm_backend_close(&backend);

    TEST_ASSERT_EQ_INT(1, data.closed);
    TEST_ASSERT_EQ_INT(0, backend.is_open);
}

static void test_unsupported_ops(void)
{
    PpmBackend backend;

    static const PpmBackendOps ops = {
        .name = "empty",
        .open = NULL,
        .scan = NULL,
        .close = NULL,
    };

    TEST_ASSERT_EQ(ppm_backend_init(&backend,&ops,NULL), PPM_OK);
    TEST_ASSERT_EQ(ppm_backend_open(&backend), PPM_UNSUPPORTED);
    TEST_ASSERT_EQ(ppm_backend_scan(&backend), PPM_UNSUPPORTED);

    ppm_backend_close(&backend);
}

int main(void)
{
    test_init();
    test_null_arguments();
    test_open_scan_close();
    test_unsupported_ops();

    return test_summary();
}
