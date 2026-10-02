#include "hace/giop.h"
#include "test_framework.h"

#include <string.h>

static int test_message_header_roundtrip_little_endian(void)
{
    uint8_t storage[64];
    hace_cdr_buffer_t buf;
    hace_giop_message_header_t header;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_begin_message(&buf, HACE_CDR_LITTLE_ENDIAN) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_EQ_INT(16, hace_cdr_buffer_position(&buf));
    HACE_ASSERT_TRUE(hace_giop_finish_message(&buf, HACE_GIOP_MSG_MESSAGE_ERROR,
                                               0) == HACE_GIOP_OK);
    HACE_ASSERT_EQ_INT(16, hace_cdr_buffer_position(&buf));

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_decode_message_header(&buf, &header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(header.byte_order == HACE_CDR_LITTLE_ENDIAN);
    HACE_ASSERT_EQ_INT(0, header.more_fragments);
    HACE_ASSERT_TRUE(header.message_type == HACE_GIOP_MSG_MESSAGE_ERROR);
    HACE_ASSERT_EQ_INT(0, header.message_size);
    HACE_ASSERT_EQ_INT(16, hace_cdr_buffer_position(&buf));
    return 1;
}

static int test_message_header_rejects_bad_magic(void)
{
    uint8_t storage[32];
    hace_cdr_buffer_t buf;
    hace_giop_message_header_t header;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_begin_message(&buf, HACE_CDR_BIG_ENDIAN) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_finish_message(&buf, HACE_GIOP_MSG_MESSAGE_ERROR,
                                               0) == HACE_GIOP_OK);
    storage[0] = (uint8_t)'X'; /* corrupt magic */

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_decode_message_header(&buf, &header) ==
                      HACE_GIOP_ERR_BAD_MAGIC);
    return 1;
}

static int test_message_header_rejects_unsupported_version(void)
{
    uint8_t storage[32];
    hace_cdr_buffer_t buf;
    hace_giop_message_header_t header;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_begin_message(&buf, HACE_CDR_BIG_ENDIAN) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_finish_message(&buf, HACE_GIOP_MSG_MESSAGE_ERROR,
                                               0) == HACE_GIOP_OK);
    storage[5] = 1U; /* claim version 1.1 instead of 1.2 */

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_decode_message_header(&buf, &header) ==
                      HACE_GIOP_ERR_UNSUPPORTED_VERSION);
    return 1;
}

static int build_request(hace_cdr_buffer_t *buf, uint32_t request_id,
                          int response_expected, const char *operation)
{
    hace_giop_request_header_t header;

    memset(&header, 0, sizeof(header));
    header.request_id = request_id;
    header.response_expected = response_expected;
    header.object_key[0] = 0xABU;
    header.object_key_length = 1U;
    (void)strncpy(header.operation, operation, sizeof(header.operation) - 1U);

    if (hace_giop_begin_message(buf, HACE_CDR_BIG_ENDIAN) != HACE_GIOP_OK) {
        return 0;
    }
    if (hace_giop_encode_request_header(buf, &header) != HACE_GIOP_OK) {
        return 0;
    }
    if (hace_giop_finish_message(buf, HACE_GIOP_MSG_REQUEST, 0) != HACE_GIOP_OK) {
        return 0;
    }
    return 1;
}

static int test_request_roundtrip(void)
{
    uint8_t storage[128];
    hace_cdr_buffer_t buf;
    hace_giop_message_header_t msg_header;
    hace_giop_request_header_t req_header;
    hace_cdr_buffer_t body;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(build_request(&buf, 42U, 1, "ping"));

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_decode_message_header(&buf, &msg_header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(msg_header.message_type == HACE_GIOP_MSG_REQUEST);

    HACE_ASSERT_TRUE(hace_giop_decode_request_header(
                          &buf, msg_header.message_size, &req_header, &body) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_EQ_INT(42, req_header.request_id);
    HACE_ASSERT_EQ_INT(1, req_header.response_expected);
    HACE_ASSERT_EQ_INT(1, req_header.object_key_length);
    HACE_ASSERT_EQ_INT(0xAB, req_header.object_key[0]);
    HACE_ASSERT_TRUE(strcmp(req_header.operation, "ping") == 0);
    HACE_ASSERT_EQ_INT(0, hace_cdr_buffer_capacity(&body)); /* no argument bytes */
    return 1;
}

static int test_request_rejects_unsupported_addressing_disposition(void)
{
    uint8_t storage[128];
    hace_cdr_buffer_t buf;
    hace_giop_message_header_t msg_header;
    hace_giop_request_header_t req_header;
    hace_cdr_buffer_t body;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(build_request(&buf, 1U, 1, "ping"));

    /* TargetAddress discriminant is the u16 right after request_id(4) +
     * response_flags(1) + reserved(3), at body offset 8, i.e. absolute
     * offset 16 + 8 = 24. Corrupt it to an unsupported value (1). */
    storage[24] = 0U;
    storage[25] = 1U;

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_decode_message_header(&buf, &msg_header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_decode_request_header(
                          &buf, msg_header.message_size, &req_header, &body) ==
                      HACE_GIOP_ERR_UNSUPPORTED_ADDRESSING_DISPOSITION);
    return 1;
}

static int test_request_rejects_unsupported_sync_scope(void)
{
    uint8_t storage[128];
    hace_cdr_buffer_t buf;
    hace_giop_message_header_t msg_header;
    hace_giop_request_header_t req_header;
    hace_cdr_buffer_t body;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(build_request(&buf, 1U, 1, "ping"));

    /* response_flags is the single octet right after request_id, at
     * absolute offset 16 + 4 = 20. 1 = SYNC_WITH_TRANSPORT, which this
     * profile doesn't support (see docs/corba-spec-gap-analysis.md Sec 7). */
    storage[20] = 1U;

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_decode_message_header(&buf, &msg_header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_decode_request_header(
                          &buf, msg_header.message_size, &req_header, &body) ==
                      HACE_GIOP_ERR_UNSUPPORTED_SYNC_SCOPE);
    return 1;
}

static int test_reply_roundtrip(void)
{
    uint8_t storage[64];
    hace_cdr_buffer_t buf;
    hace_giop_reply_header_t reply_header;
    hace_giop_message_header_t msg_header;
    hace_giop_reply_header_t decoded;
    hace_cdr_buffer_t body;

    memset(&reply_header, 0, sizeof(reply_header));
    reply_header.request_id = 42U;
    reply_header.reply_status = HACE_GIOP_REPLY_SYSTEM_EXCEPTION;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_begin_message(&buf, HACE_CDR_BIG_ENDIAN) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_encode_reply_header(&buf, &reply_header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_finish_message(&buf, HACE_GIOP_MSG_REPLY, 0) ==
                      HACE_GIOP_OK);

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_decode_message_header(&buf, &msg_header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(msg_header.message_type == HACE_GIOP_MSG_REPLY);
    HACE_ASSERT_TRUE(hace_giop_decode_reply_header(&buf, msg_header.message_size,
                                                    &decoded, &body) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_EQ_INT(42, decoded.request_id);
    HACE_ASSERT_TRUE(decoded.reply_status == HACE_GIOP_REPLY_SYSTEM_EXCEPTION);
    return 1;
}

static int test_reply_rejects_nonempty_service_context(void)
{
    uint8_t storage[64];
    hace_cdr_buffer_t buf;
    hace_giop_reply_header_t reply_header;
    hace_giop_message_header_t msg_header;
    hace_giop_reply_header_t decoded;
    hace_cdr_buffer_t body;

    memset(&reply_header, 0, sizeof(reply_header));
    reply_header.request_id = 1U;
    reply_header.reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_begin_message(&buf, HACE_CDR_BIG_ENDIAN) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_encode_reply_header(&buf, &reply_header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_finish_message(&buf, HACE_GIOP_MSG_REPLY, 0) ==
                      HACE_GIOP_OK);

    /* service_context length is the first field of the body, at
     * absolute offset 16. Claim 1 entry instead of 0. */
    storage[19] = 1U;

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_decode_message_header(&buf, &msg_header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_decode_reply_header(&buf, msg_header.message_size,
                                                    &decoded, &body) ==
                      HACE_GIOP_ERR_NONEMPTY_SERVICE_CONTEXT);
    return 1;
}

static int test_cancel_request_roundtrip(void)
{
    uint8_t storage[32];
    hace_cdr_buffer_t buf;
    hace_giop_cancel_request_header_t header;
    hace_giop_message_header_t msg_header;
    hace_giop_cancel_request_header_t decoded;

    header.request_id = 7U;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_LITTLE_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_begin_message(&buf, HACE_CDR_LITTLE_ENDIAN) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_encode_cancel_request_header(&buf, &header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_finish_message(
                          &buf, HACE_GIOP_MSG_CANCEL_REQUEST, 0) == HACE_GIOP_OK);

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_decode_message_header(&buf, &msg_header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(msg_header.message_type == HACE_GIOP_MSG_CANCEL_REQUEST);
    HACE_ASSERT_TRUE(hace_giop_decode_cancel_request_header(&buf, &decoded) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_EQ_INT(7, decoded.request_id);
    return 1;
}

static int test_locate_request_roundtrip(void)
{
    uint8_t storage[32];
    hace_cdr_buffer_t buf;
    hace_giop_locate_request_header_t header;
    hace_giop_message_header_t msg_header;
    hace_giop_locate_request_header_t decoded;

    memset(&header, 0, sizeof(header));
    header.request_id = 9U;
    header.object_key[0] = 0xCDU;
    header.object_key_length = 1U;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_begin_message(&buf, HACE_CDR_BIG_ENDIAN) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_encode_locate_request_header(&buf, &header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_finish_message(
                          &buf, HACE_GIOP_MSG_LOCATE_REQUEST, 0) == HACE_GIOP_OK);

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_decode_message_header(&buf, &msg_header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(msg_header.message_type == HACE_GIOP_MSG_LOCATE_REQUEST);
    HACE_ASSERT_TRUE(hace_giop_decode_locate_request_header(&buf, &decoded) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_EQ_INT(9, decoded.request_id);
    HACE_ASSERT_EQ_INT(1, decoded.object_key_length);
    HACE_ASSERT_EQ_INT(0xCD, decoded.object_key[0]);
    return 1;
}

static int test_locate_reply_roundtrip(void)
{
    uint8_t storage[32];
    hace_cdr_buffer_t buf;
    hace_giop_locate_reply_header_t header;
    hace_giop_message_header_t msg_header;
    hace_giop_locate_reply_header_t decoded;

    header.request_id = 9U;
    header.locate_status = HACE_GIOP_LOCATE_OBJECT_HERE;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_begin_message(&buf, HACE_CDR_BIG_ENDIAN) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_encode_locate_reply_header(&buf, &header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_finish_message(
                          &buf, HACE_GIOP_MSG_LOCATE_REPLY, 0) == HACE_GIOP_OK);

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_decode_message_header(&buf, &msg_header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(msg_header.message_type == HACE_GIOP_MSG_LOCATE_REPLY);
    HACE_ASSERT_TRUE(hace_giop_decode_locate_reply_header(&buf, &decoded) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_EQ_INT(9, decoded.request_id);
    HACE_ASSERT_TRUE(decoded.locate_status == HACE_GIOP_LOCATE_OBJECT_HERE);
    return 1;
}

static int test_message_error_has_empty_body(void)
{
    uint8_t storage[32];
    hace_cdr_buffer_t buf;
    hace_giop_message_header_t msg_header;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_begin_message(&buf, HACE_CDR_BIG_ENDIAN) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_finish_message(&buf, HACE_GIOP_MSG_MESSAGE_ERROR,
                                               0) == HACE_GIOP_OK);
    HACE_ASSERT_EQ_INT(16, hace_cdr_buffer_position(&buf));

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_decode_message_header(&buf, &msg_header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(msg_header.message_type == HACE_GIOP_MSG_MESSAGE_ERROR);
    HACE_ASSERT_EQ_INT(0, msg_header.message_size);
    return 1;
}

static int test_fragment_roundtrip_and_more_fragments_flag(void)
{
    uint8_t storage[32];
    hace_cdr_buffer_t buf;
    hace_giop_fragment_header_t header;
    hace_giop_message_header_t msg_header;
    hace_giop_fragment_header_t decoded;
    hace_cdr_buffer_t body;

    header.request_id = 5U;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_begin_message(&buf, HACE_CDR_BIG_ENDIAN) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_encode_fragment_header(&buf, &header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_finish_message(&buf, HACE_GIOP_MSG_FRAGMENT, 1) ==
                      HACE_GIOP_OK);

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_decode_message_header(&buf, &msg_header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(msg_header.message_type == HACE_GIOP_MSG_FRAGMENT);
    HACE_ASSERT_EQ_INT(1, msg_header.more_fragments);
    HACE_ASSERT_TRUE(hace_giop_decode_fragment_header(
                          &buf, msg_header.message_size, &decoded, &body) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_EQ_INT(5, decoded.request_id);
    HACE_ASSERT_EQ_INT(0, hace_cdr_buffer_capacity(&body));
    return 1;
}

/*
 * Conformance vector: a hand-traced Request message (request_id=1,
 * twoway, object_key={0xAB}, operation="ping"), encoded big-endian.
 * Byte layout (see docs/architecture/adr-0002-giop-framing.md for the
 * field order this traces):
 *
 *   0-3   "GIOP"
 *   4-5   version 1.2
 *   6     flags: big-endian, no fragment follows        = 0x00
 *   7     message_type: Request                         = 0x00
 *   8-11  message_size                                  = 36
 *   12-15 padding
 *   16-19 request_id                                     = 1
 *   20    response_flags (WITH_TARGET)                   = 3
 *   21-23 reserved
 *   24-25 TargetAddress discriminant (KeyAddr)            = 0
 *   26-27 padding (align object_key's length to 4 bytes)
 *   28-31 object_key length                               = 1
 *   32    object_key data                                 = 0xAB
 *   33-35 padding (align operation's length to 4 bytes)
 *   36-39 operation length (incl NUL)                      = 5
 *   40-44 "ping\0"
 *   45-47 padding (align service_context length to 4 bytes)
 *   48-51 service_context length                           = 0
 */
static int test_request_conformance_vector(void)
{
    static const uint8_t expected[] = {
        0x47U, 0x49U, 0x4FU, 0x50U, /* "GIOP" */
        0x01U, 0x02U,               /* version 1.2 */
        0x00U,                      /* flags */
        0x00U,                      /* message_type: Request */
        0x00U, 0x00U, 0x00U, 0x24U, /* message_size = 36 */
        0x00U, 0x00U, 0x00U, 0x00U, /* padding */
        0x00U, 0x00U, 0x00U, 0x01U, /* request_id = 1 */
        0x03U,                      /* response_flags = WITH_TARGET */
        0x00U, 0x00U, 0x00U,        /* reserved */
        0x00U, 0x00U,               /* TargetAddress discriminant = 0 */
        0x00U, 0x00U,               /* padding */
        0x00U, 0x00U, 0x00U, 0x01U, /* object_key length = 1 */
        0xABU,                      /* object_key data */
        0x00U, 0x00U, 0x00U,        /* padding */
        0x00U, 0x00U, 0x00U, 0x05U, /* operation length = 5 */
        (uint8_t)'p', (uint8_t)'i', (uint8_t)'n', (uint8_t)'g', 0x00U,
        0x00U, 0x00U, 0x00U,        /* padding */
        0x00U, 0x00U, 0x00U, 0x00U  /* service_context length = 0 */
    };
    uint8_t storage[128];
    hace_cdr_buffer_t buf;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(build_request(&buf, 1U, 1, "ping"));

    HACE_ASSERT_EQ_INT(sizeof(expected), hace_cdr_buffer_position(&buf));
    HACE_ASSERT_TRUE(memcmp(storage, expected, sizeof(expected)) == 0);
    return 1;
}

int main(void)
{
    HACE_RUN_TEST(test_message_header_roundtrip_little_endian);
    HACE_RUN_TEST(test_message_header_rejects_bad_magic);
    HACE_RUN_TEST(test_message_header_rejects_unsupported_version);
    HACE_RUN_TEST(test_request_roundtrip);
    HACE_RUN_TEST(test_request_rejects_unsupported_addressing_disposition);
    HACE_RUN_TEST(test_request_rejects_unsupported_sync_scope);
    HACE_RUN_TEST(test_reply_roundtrip);
    HACE_RUN_TEST(test_reply_rejects_nonempty_service_context);
    HACE_RUN_TEST(test_cancel_request_roundtrip);
    HACE_RUN_TEST(test_locate_request_roundtrip);
    HACE_RUN_TEST(test_locate_reply_roundtrip);
    HACE_RUN_TEST(test_message_error_has_empty_body);
    HACE_RUN_TEST(test_fragment_roundtrip_and_more_fragments_flag);
    HACE_RUN_TEST(test_request_conformance_vector);
    HACE_TEST_REPORT();
}
