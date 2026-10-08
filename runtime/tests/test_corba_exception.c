#include "hace/corba_exception.h"
#include "test_framework.h"

static int test_system_exception_roundtrip(void)
{
    uint8_t storage[32];
    hace_cdr_buffer_t buf;
    hace_corba_system_exception_t exc;
    hace_corba_system_exception_t decoded;

    exc.exception = HACE_CORBA_EXC_OBJECT_NOT_EXIST;
    exc.minor_code = 42U;
    exc.completion_status = HACE_CORBA_COMPLETED_NO;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_corba_encode_system_exception(&buf, &exc) == HACE_CORBA_OK);

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_corba_decode_system_exception(&buf, &decoded) ==
                      HACE_CORBA_OK);
    HACE_ASSERT_TRUE(decoded.exception == HACE_CORBA_EXC_OBJECT_NOT_EXIST);
    HACE_ASSERT_EQ_INT(42, decoded.minor_code);
    HACE_ASSERT_TRUE(decoded.completion_status == HACE_CORBA_COMPLETED_NO);
    return 1;
}

static int test_system_exception_decode_rejects_bad_codes(void)
{
    uint8_t storage[32];
    hace_cdr_buffer_t buf;
    hace_corba_system_exception_t decoded;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    /* an exception code past HACE_CORBA_EXC_COMM_FAILURE */
    HACE_ASSERT_TRUE(hace_cdr_encode_u32(&buf, 999U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_u32(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_u32(&buf, 0U) == HACE_CDR_OK);

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_corba_decode_system_exception(&buf, &decoded) ==
                      HACE_CORBA_ERR_INVALID_ARGUMENT);
    return 1;
}

static int test_exception_from_uiop_status_mapping(void)
{
    hace_corba_system_exception_t exc;

    exc = hace_corba_exception_from_uiop_status(HACE_UIOP_OK);
    HACE_ASSERT_TRUE(exc.exception == HACE_CORBA_EXC_NONE);
    HACE_ASSERT_TRUE(exc.completion_status == HACE_CORBA_COMPLETED_YES);

    exc = hace_corba_exception_from_uiop_status(HACE_UIOP_ERR_TIMEOUT);
    HACE_ASSERT_TRUE(exc.exception == HACE_CORBA_EXC_TIMEOUT);
    HACE_ASSERT_TRUE(exc.completion_status == HACE_CORBA_COMPLETED_MAYBE);

    exc = hace_corba_exception_from_uiop_status(HACE_UIOP_ERR_HAL);
    HACE_ASSERT_TRUE(exc.exception == HACE_CORBA_EXC_COMM_FAILURE);
    HACE_ASSERT_TRUE(exc.completion_status == HACE_CORBA_COMPLETED_MAYBE);

    exc = hace_corba_exception_from_uiop_status(HACE_UIOP_ERR_TOO_LARGE);
    HACE_ASSERT_TRUE(exc.exception == HACE_CORBA_EXC_BAD_PARAM);
    HACE_ASSERT_TRUE(exc.completion_status == HACE_CORBA_COMPLETED_NO);

    exc = hace_corba_exception_from_uiop_status(HACE_UIOP_ERR_INVALID_ARGUMENT);
    HACE_ASSERT_TRUE(exc.exception == HACE_CORBA_EXC_BAD_PARAM);
    HACE_ASSERT_TRUE(exc.completion_status == HACE_CORBA_COMPLETED_NO);
    return 1;
}

static int test_null_arguments_are_rejected(void)
{
    uint8_t storage[32];
    hace_cdr_buffer_t buf;
    hace_corba_system_exception_t exc;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_corba_encode_system_exception(NULL, &exc) ==
                      HACE_CORBA_ERR_INVALID_ARGUMENT);
    HACE_ASSERT_TRUE(hace_corba_encode_system_exception(&buf, NULL) ==
                      HACE_CORBA_ERR_INVALID_ARGUMENT);
    HACE_ASSERT_TRUE(hace_corba_decode_system_exception(&buf, NULL) ==
                      HACE_CORBA_ERR_INVALID_ARGUMENT);
    return 1;
}

int main(void)
{
    HACE_RUN_TEST(test_system_exception_roundtrip);
    HACE_RUN_TEST(test_system_exception_decode_rejects_bad_codes);
    HACE_RUN_TEST(test_exception_from_uiop_status_mapping);
    HACE_RUN_TEST(test_null_arguments_are_rejected);
    HACE_TEST_REPORT();
}
