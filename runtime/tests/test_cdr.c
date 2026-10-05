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

static int test_set_byte_order(void)
{
    uint8_t storage[8];
    hace_cdr_buffer_t buf;
    uint32_t out;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_buffer_set_byte_order(&buf, HACE_CDR_LITTLE_ENDIAN) ==
                      HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_u32(&buf, 0x01020304U) == HACE_CDR_OK);
    /* byte order was switched before this encode, so the bytes are
     * little-endian even though the buffer was initialized big-endian. */
    HACE_ASSERT_EQ_INT(0x04, storage[0]);
    HACE_ASSERT_EQ_INT(0x01, storage[3]);

    buf.position = 0U;
    HACE_ASSERT_TRUE(hace_cdr_decode_u32(&buf, &out) == HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(0x01020304, out);

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_byte_order(&buf, 99) ==
                      HACE_CDR_ERR_INVALID_ARGUMENT);
    return 1;
}

static int test_buffer_init_view(void)
{
    uint8_t storage[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    hace_cdr_buffer_t parent;
    hace_cdr_buffer_t view;
    uint8_t first;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&parent, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&parent, 2U) == HACE_CDR_OK);

    HACE_ASSERT_TRUE(hace_cdr_buffer_init_view(&view, &parent, 3U) == HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(5, hace_cdr_buffer_position(&parent)); /* consumed */
    HACE_ASSERT_EQ_INT(3, hace_cdr_buffer_capacity(&view));
    HACE_ASSERT_EQ_INT(0, hace_cdr_buffer_position(&view));

    HACE_ASSERT_TRUE(hace_cdr_decode_octet(&view, &first) == HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(3, first); /* storage[2] */

    /* view is bounded to 3 bytes even though parent has more left. */
    HACE_ASSERT_TRUE(hace_cdr_buffer_init_view(&view, &parent, 10U) ==
                      HACE_CDR_ERR_BUFFER_OVERFLOW);
    return 1;
}

static int test_string_roundtrip(void)
{
    uint8_t storage[32];
    hace_cdr_buffer_t buf;
    char out[16];
    size_t out_length = 0U;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_string(&buf, "hello", 16U) == HACE_CDR_OK);
    /* u32 length (6, including NUL) + 6 bytes = 10 */
    HACE_ASSERT_EQ_INT(10, buf.position);

    buf.position = 0U;
    HACE_ASSERT_TRUE(hace_cdr_decode_string(&buf, out, sizeof(out), &out_length) ==
                      HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(5, out_length);
    HACE_ASSERT_TRUE(strcmp(out, "hello") == 0);
    return 1;
}

static int test_string_too_long_is_rejected(void)
{
    uint8_t storage[32];
    hace_cdr_buffer_t buf;
    char out[4];

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    /* "hello" + NUL needs 6 bytes; max_length 5 doesn't leave room. */
    HACE_ASSERT_TRUE(hace_cdr_encode_string(&buf, "hello", 5U) ==
                      HACE_CDR_ERR_INVALID_ARGUMENT);
    HACE_ASSERT_EQ_INT(0, buf.position);

    HACE_ASSERT_TRUE(hace_cdr_encode_string(&buf, "hello", 6U) == HACE_CDR_OK);

    buf.position = 0U;
    HACE_ASSERT_TRUE(hace_cdr_decode_string(&buf, out, sizeof(out), NULL) ==
                      HACE_CDR_ERR_INVALID_ARGUMENT);
    return 1;
}

static int test_sequence_length(void)
{
    uint8_t storage[8];
    hace_cdr_buffer_t buf;
    size_t out_count = 0U;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_sequence_length(&buf, 3U) == HACE_CDR_OK);

    buf.position = 0U;
    HACE_ASSERT_TRUE(hace_cdr_decode_sequence_length(&buf, 10U, &out_count) ==
                      HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(3, out_count);

    buf.position = 0U;
    HACE_ASSERT_TRUE(hace_cdr_decode_sequence_length(&buf, 2U, &out_count) ==
                      HACE_CDR_ERR_INVALID_ARGUMENT);
    return 1;
}

static int test_octet_seq_roundtrip(void)
{
    uint8_t storage[16];
    hace_cdr_buffer_t buf;
    const uint8_t data[] = {0xAAU, 0xBBU, 0xCCU};
    uint8_t out[8];
    size_t out_count = 0U;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_octet_seq(&buf, data, sizeof(data)) ==
                      HACE_CDR_OK);

    buf.position = 0U;
    HACE_ASSERT_TRUE(hace_cdr_decode_octet_seq(&buf, out, sizeof(out), &out_count) ==
                      HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(3, out_count);
    HACE_ASSERT_TRUE(memcmp(out, data, sizeof(data)) == 0);

    buf.position = 0U;
    HACE_ASSERT_TRUE(hace_cdr_decode_octet_seq(&buf, out, 2U, &out_count) ==
                      HACE_CDR_ERR_INVALID_ARGUMENT);
    return 1;
}

static int test_raw_bytes_roundtrip(void)
{
    uint8_t storage[8];
    hace_cdr_buffer_t buf;
    const uint8_t data[] = {0x11U, 0x22U, 0x33U};
    uint8_t out[3];

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    /* No length prefix is written: a single octet followed directly by
     * the raw bytes should advance position by exactly 1 + 3 = 4. */
    HACE_ASSERT_TRUE(hace_cdr_encode_octet(&buf, 0xAAU) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_raw_bytes(&buf, data, sizeof(data)) ==
                      HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(4, buf.position);

    buf.position = 0U;
    {
        uint8_t marker;
        HACE_ASSERT_TRUE(hace_cdr_decode_octet(&buf, &marker) == HACE_CDR_OK);
        HACE_ASSERT_EQ_INT(0xAA, marker);
    }
    HACE_ASSERT_TRUE(hace_cdr_decode_raw_bytes(&buf, out, sizeof(out)) ==
                      HACE_CDR_OK);
    HACE_ASSERT_TRUE(memcmp(out, data, sizeof(data)) == 0);
    return 1;
}

static int test_raw_bytes_bounds_are_rejected(void)
{
    uint8_t storage[4];
    hace_cdr_buffer_t buf;
    uint8_t data[4] = {0};
    uint8_t out[4];

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_raw_bytes(&buf, data, 5U) ==
                      HACE_CDR_ERR_BUFFER_OVERFLOW);
    HACE_ASSERT_EQ_INT(0, buf.position);

    HACE_ASSERT_TRUE(hace_cdr_encode_raw_bytes(&buf, data, sizeof(data)) ==
                      HACE_CDR_OK);
    buf.position = 0U;
    HACE_ASSERT_TRUE(hace_cdr_decode_raw_bytes(&buf, out, 5U) ==
                      HACE_CDR_ERR_BUFFER_UNDERFLOW);
    return 1;
}

static int test_struct_pattern_example(void)
{
    /* Demonstrates that CDR structs need no dedicated API: a struct is
     * just its members encoded in declared order, each self-aligning. */
    uint8_t storage[16];
    hace_cdr_buffer_t buf;
    uint16_t id_out;
    uint32_t value_out;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_u16(&buf, 7U) == HACE_CDR_OK);  /* id */
    HACE_ASSERT_TRUE(hace_cdr_encode_u32(&buf, 99U) == HACE_CDR_OK); /* value */
    /* u16 (2) + 2 pad bytes (u32 needs 4-byte alignment) + u32 (4) = 8 */
    HACE_ASSERT_EQ_INT(8, buf.position);

    buf.position = 0U;
    HACE_ASSERT_TRUE(hace_cdr_decode_u16(&buf, &id_out) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_decode_u32(&buf, &value_out) == HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(7, id_out);
    HACE_ASSERT_EQ_INT(99, value_out);
    return 1;
}

static int test_union_pattern_example(void)
{
    /* Demonstrates that CDR unions need no dedicated API either: encode
     * the discriminant, then only the member it selects. */
    uint8_t storage[32];
    hace_cdr_buffer_t buf;
    uint32_t discriminant_out;
    int32_t int_value_out;
    enum { UNION_AS_INT = 0, UNION_AS_STRING = 1 };

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_u32(&buf, (uint32_t)UNION_AS_INT) ==
                      HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_i32(&buf, -5) == HACE_CDR_OK);

    buf.position = 0U;
    HACE_ASSERT_TRUE(hace_cdr_decode_u32(&buf, &discriminant_out) == HACE_CDR_OK);
    HACE_ASSERT_EQ_INT(UNION_AS_INT, discriminant_out);
    HACE_ASSERT_TRUE(hace_cdr_decode_i32(&buf, &int_value_out) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(int_value_out == -5);
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
    HACE_RUN_TEST(test_set_byte_order);
    HACE_RUN_TEST(test_buffer_init_view);
    HACE_RUN_TEST(test_string_roundtrip);
    HACE_RUN_TEST(test_string_too_long_is_rejected);
    HACE_RUN_TEST(test_sequence_length);
    HACE_RUN_TEST(test_octet_seq_roundtrip);
    HACE_RUN_TEST(test_raw_bytes_roundtrip);
    HACE_RUN_TEST(test_raw_bytes_bounds_are_rejected);
    HACE_RUN_TEST(test_struct_pattern_example);
    HACE_RUN_TEST(test_union_pattern_example);
    HACE_RUN_TEST(test_null_arguments_are_rejected);
    HACE_TEST_REPORT();
}
