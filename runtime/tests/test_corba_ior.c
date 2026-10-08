#include "hace/corba_ior.h"
#include "test_framework.h"

#include <string.h>

static int test_ior_cdr_roundtrip(void)
{
    uint8_t storage[64];
    hace_cdr_buffer_t buf;
    hace_corba_ior_t ior;
    hace_corba_ior_t decoded;

    ior.host_ipv4 = ((uint32_t)127U << 24) | 1U;
    ior.port = 58701U;
    ior.object_key[0] = 0xDEU;
    ior.object_key[1] = 0xADU;
    ior.object_key_length = 2U;

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_corba_ior_encode(&buf, &ior) == HACE_CORBA_OK);

    HACE_ASSERT_TRUE(hace_cdr_buffer_set_position(&buf, 0U) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_corba_ior_decode(&buf, &decoded) == HACE_CORBA_OK);
    HACE_ASSERT_EQ_INT(ior.host_ipv4, decoded.host_ipv4);
    HACE_ASSERT_EQ_INT(ior.port, decoded.port);
    HACE_ASSERT_EQ_INT(2, decoded.object_key_length);
    HACE_ASSERT_TRUE(memcmp(decoded.object_key, ior.object_key, 2U) == 0);
    return 1;
}

static int test_object_to_string_and_back(void)
{
    hace_corba_ior_t ior;
    hace_corba_ior_t decoded;
    char str[HACE_CORBA_MAX_IOR_STRING];
    size_t i;

    ior.host_ipv4 = ((uint32_t)192U << 24) | ((uint32_t)168U << 16) | 1U;
    ior.port = 12345U;
    ior.object_key_length = 4U;
    for (i = 0U; i < ior.object_key_length; i++) {
        ior.object_key[i] = (uint8_t)(0x10U + i);
    }

    HACE_ASSERT_TRUE(hace_corba_object_to_string(&ior, str, sizeof(str)) ==
                      HACE_CORBA_OK);
    HACE_ASSERT_TRUE(strcmp(str, "uiop:192.168.0.1:12345/10111213") == 0);

    HACE_ASSERT_TRUE(hace_corba_string_to_object(str, &decoded) == HACE_CORBA_OK);
    HACE_ASSERT_EQ_INT(ior.host_ipv4, decoded.host_ipv4);
    HACE_ASSERT_EQ_INT(ior.port, decoded.port);
    HACE_ASSERT_EQ_INT(ior.object_key_length, decoded.object_key_length);
    HACE_ASSERT_TRUE(memcmp(decoded.object_key, ior.object_key,
                             ior.object_key_length) == 0);
    return 1;
}

static int test_object_to_string_zero_length_key(void)
{
    hace_corba_ior_t ior;
    hace_corba_ior_t decoded;
    char str[HACE_CORBA_MAX_IOR_STRING];

    ior.host_ipv4 = 0U;
    ior.port = 0U;
    ior.object_key_length = 0U;

    HACE_ASSERT_TRUE(hace_corba_object_to_string(&ior, str, sizeof(str)) ==
                      HACE_CORBA_OK);
    HACE_ASSERT_TRUE(strcmp(str, "uiop:0.0.0.0:0/") == 0);

    HACE_ASSERT_TRUE(hace_corba_string_to_object(str, &decoded) == HACE_CORBA_OK);
    HACE_ASSERT_EQ_INT(0, decoded.object_key_length);
    return 1;
}

static int test_string_to_object_rejects_malformed_input(void)
{
    hace_corba_ior_t ior;

    HACE_ASSERT_TRUE(hace_corba_string_to_object("nonsense", &ior) ==
                      HACE_CORBA_ERR_INVALID_ARGUMENT);
    HACE_ASSERT_TRUE(hace_corba_string_to_object("uiop:999.1.1.1:80/ab", &ior) ==
                      HACE_CORBA_ERR_INVALID_ARGUMENT);
    HACE_ASSERT_TRUE(hace_corba_string_to_object("uiop:1.2.3.4:999999/ab", &ior) ==
                      HACE_CORBA_ERR_INVALID_ARGUMENT);
    HACE_ASSERT_TRUE(hace_corba_string_to_object("uiop:1.2.3.4:80/abc", &ior) ==
                      HACE_CORBA_ERR_INVALID_ARGUMENT); /* odd-length hex */
    HACE_ASSERT_TRUE(hace_corba_string_to_object("uiop:1.2.3.4:80/zz", &ior) ==
                      HACE_CORBA_ERR_INVALID_ARGUMENT); /* non-hex */
    HACE_ASSERT_TRUE(hace_corba_string_to_object("uiop:1.2.3.4-80/ab", &ior) ==
                      HACE_CORBA_ERR_INVALID_ARGUMENT); /* missing ':' */
    HACE_ASSERT_TRUE(hace_corba_string_to_object(NULL, &ior) ==
                      HACE_CORBA_ERR_INVALID_ARGUMENT);
    return 1;
}

static int test_object_to_string_rejects_small_buffer(void)
{
    hace_corba_ior_t ior;
    char tiny[4];

    ior.host_ipv4 = ((uint32_t)127U << 24) | 1U;
    ior.port = 1U;
    ior.object_key_length = 1U;
    ior.object_key[0] = 0xAAU;

    HACE_ASSERT_TRUE(hace_corba_object_to_string(&ior, tiny, sizeof(tiny)) ==
                      HACE_CORBA_ERR_INVALID_ARGUMENT);
    return 1;
}

int main(void)
{
    HACE_RUN_TEST(test_ior_cdr_roundtrip);
    HACE_RUN_TEST(test_object_to_string_and_back);
    HACE_RUN_TEST(test_object_to_string_zero_length_key);
    HACE_RUN_TEST(test_string_to_object_rejects_malformed_input);
    HACE_RUN_TEST(test_object_to_string_rejects_small_buffer);
    HACE_TEST_REPORT();
}
