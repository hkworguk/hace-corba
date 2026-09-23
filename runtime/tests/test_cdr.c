#include "hace/cdr.h"
#include "test_framework.h"

#include <string.h>

static int test_octet_roundtrip(void)
{
    uint8_t storage[8];
    hace_cdr_buffer_t buf;
    uint8_t out;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_octet(&buf, 0x42U) == HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(1, buf.position);

    buf.position = 0U;
    HACE_ASSERT_TRUE(hace_cdr_decode_octet(&buf, &out) == HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(0x42, out);
    return 1;
}

static int test_bool_roundtrip(void)
{
    uint8_t storage[8];
    hace_cdr_buffer_t buf;
    int out = 0;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_LITTLE_ENDIAN) ==
                      HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_bool(&buf, 1) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_bool(&buf, 0) == HACE_CDR_OK);

    buf.position = 0U;
    HACE_ASSERT_TRUE(hace_cdr_decode_bool(&buf, &out) == HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(1, out);
    HACE_ASSERT_TRUE(hace_cdr_decode_bool(&buf, &out) == HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(0, out);
    return 1;
}

static int test_u16_alignment(void)
{
    uint8_t storage[8];
    hace_cdr_buffer_t buf;
    uint16_t out;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    /* One octet, then a u16 must be padded to a 2-byte boundary: octet at
     * offset 0, one pad byte at offset 1, u16 at offset 2..3. */
    HACE_ASSERT_TRUE(hace_cdr_encode_octet(&buf, 0xAAU) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_u16(&buf, 0x1234U) == HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(4, buf.position);
    HACE_ASSERT_EQ_INT(0x12, storage[2]); /* big-endian: high byte first */
    HACE_ASSERT_EQ_INT(0x34, storage[3]);

    buf.position = 0U;
    {
        uint8_t skip;
        HACE_ASSERT_TRUE(hace_cdr_decode_octet(&buf, &skip) == HACE_CDR_OK);
    }
    HACE_ASSERT_TRUE(hace_cdr_decode_u16(&buf, &out) == HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(0x1234, out);
    return 1;
}

static int test_u32_byte_order(void)
{
    uint8_t storage_be[8];
    uint8_t storage_le[8];
    hace_cdr_buffer_t buf_be;
    hace_cdr_buffer_t buf_le;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf_be, storage_be,
                                           sizeof(storage_be),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf_le, storage_le,
                                           sizeof(storage_le),
                                           HACE_CDR_LITTLE_ENDIAN) ==
                      HACE_CDR_OK);

    HACE_ASSERT_TRUE(hace_cdr_encode_u32(&buf_be, 0x01020304U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_u32(&buf_le, 0x01020304U) == HACE_CDR_OK);

    HACE_ASSERT_EQ_INT(0x01, storage_be[0]);
    HACE_ASSERT_EQ_INT(0x02, storage_be[1]);
    HACE_ASSERT_EQ_INT(0x03, storage_be[2]);
    HACE_ASSERT_EQ_INT(0x04, storage_be[3]);

    HACE_ASSERT_EQ_INT(0x04, storage_le[0]);
    HACE_ASSERT_EQ_INT(0x03, storage_le[1]);
    HACE_ASSERT_EQ_INT(0x02, storage_le[2]);
    HACE_ASSERT_EQ_INT(0x01, storage_le[3]);

    {
        uint32_t out_be = 0U;
        uint32_t out_le = 0U;
        buf_be.position = 0U;
        buf_le.position = 0U;
        HACE_ASSERT_TRUE(hace_cdr_decode_u32(&buf_be, &out_be) ==
                          HACE_CDR_OK);
        HACE_ASSERT_TRUE(hace_cdr_decode_u32(&buf_le, &out_le) ==
                          HACE_CDR_OK);
        HACE_ASSERT_EQ_INT(0x01020304, out_be);
        HACE_ASSERT_EQ_INT(0x01020304, out_le);
    }
    return 1;
}

static int test_i64_roundtrip(void)
{
    uint8_t storage[16];
    hace_cdr_buffer_t buf;
    int64_t out = 0;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_i64(&buf, -123456789012345LL) ==
                      HACE_CDR_OK);
    /* i64 aligns to 8 bytes from a zero start, so no padding here. */
    HACE_ASSERT_EQ_INT(8, buf.position);

    buf.position = 0U;
    HACE_ASSERT_TRUE(hace_cdr_decode_i64(&buf, &out) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(out == -123456789012345LL);
    return 1;
}

static int test_f64_roundtrip(void)
{
    uint8_t storage[16];
    hace_cdr_buffer_t buf;
    double out = 0.0;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_LITTLE_ENDIAN) ==
                      HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_f64(&buf, 3.1415926535) == HACE_CDR_OK);

    buf.position = 0U;
    HACE_ASSERT_TRUE(hace_cdr_decode_f64(&buf, &out) == HACE_CDR_OK);
    HACE_ASSERT_TRUE((out > 3.1415926534) && (out < 3.1415926536));
    return 1;
}

static int test_encode_overflow_is_rejected(void)
{
    uint8_t storage[2];
    hace_cdr_buffer_t buf;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_u32(&buf, 1U) ==
                      HACE_CDR_ERR_BUFFER_OVERFLOW);
    /* A rejected encode must not have partially advanced the cursor. */
    HACE_ASSERT_EQ_INT(0, buf.position);
    return 1;
}

static int test_decode_underflow_is_rejected(void)
{
    uint8_t storage[2] = {0, 0};
    hace_cdr_buffer_t buf;
    uint32_t out = 0U;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_decode_u32(&buf, &out) ==
                      HACE_CDR_ERR_BUFFER_UNDERFLOW);
    return 1;
}

static int test_accessors_and_set_position(void)
{
    uint8_t storage[8];
    hace_cdr_buffer_t buf;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(sizeof(storage), hace_cdr_buffer_capacity(&buf));
    HACE_ASSERT_EQ_INT(0, hace_cdr_buffer_position(&buf));

    HACE_ASSERT_TRUE(hace_cdr_encode_u32(&buf, 0xDEADBEEFU) == HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(4, hace_cdr_buffer_position(&buf));

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(0, hace_cdr_buffer_position(&buf));

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, sizeof(storage) + 1U) ==
                      HACE_CDR_ERR_INVALID_ARGUMENT);

    HACE_ASSERT_EQ_INT(sizeof(hace_cdr_buffer_t), hace_cdr_buffer_sizeof());
    return 1;
}

static int test_null_arguments_are_rejected(void)
{
    uint8_t storage[8];
    hace_cdr_buffer_t buf;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(NULL, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) ==
                      HACE_CDR_ERR_INVALID_ARGUMENT);
    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, NULL, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) ==
                      HACE_CDR_ERR_INVALID_ARGUMENT);
    HACE_ASSERT_TRUE(hace_cdr_encode_u16(NULL, 1U) ==
                      HACE_CDR_ERR_INVALID_ARGUMENT);
    HACE_ASSERT_TRUE(hace_cdr_decode_u16(NULL, NULL) ==
                      HACE_CDR_ERR_INVALID_ARGUMENT);
    return 1;
}

int main(void)
{
    HACE_RUN_TEST(test_octet_roundtrip);
    HACE_RUN_TEST(test_bool_roundtrip);
    HACE_RUN_TEST(test_u16_alignment);
    HACE_RUN_TEST(test_u32_byte_order);
    HACE_RUN_TEST(test_i64_roundtrip);
    HACE_RUN_TEST(test_f64_roundtrip);
    HACE_RUN_TEST(test_encode_overflow_is_rejected);
    HACE_RUN_TEST(test_decode_underflow_is_rejected);
    HACE_RUN_TEST(test_accessors_and_set_position);
    HACE_RUN_TEST(test_null_arguments_are_rejected);
    HACE_TEST_REPORT();
}
