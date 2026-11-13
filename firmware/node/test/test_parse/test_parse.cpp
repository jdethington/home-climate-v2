#include <unity.h>
#include <cstring>
#include "parse.h"

void test_extract_found(void)
{
    char out[32] = {0};
    const char *json = "{\"rename\": {\"node_id\": \"indoor9\", \"node_name\": \"Test\"}}";
    TEST_ASSERT_TRUE(extractStringValue(json, "\"node_id\"", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("indoor9", out);
    TEST_ASSERT_TRUE(extractStringValue(json, "\"node_name\"", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("Test", out);
}

void test_extract_missing_key(void)
{
    char out[32] = {0};
    TEST_ASSERT_FALSE(extractStringValue("{\"a\": \"b\"}", "\"node_id\"", out, sizeof(out)));
}

void test_extract_empty_value(void)
{
    char out[32] = {0};
    TEST_ASSERT_FALSE(extractStringValue("{\"node_id\": \"\"}", "\"node_id\"", out, sizeof(out)));
}

void test_extract_too_long(void)
{
    char out[8] = {0};
    // Value longer than the buffer — refuse rather than truncate.
    TEST_ASSERT_FALSE(extractStringValue("{\"node_id\": \"indoor12345\"}", "\"node_id\"", out, sizeof(out)));
}

void test_extract_ota_url(void)
{
    char out[96] = {0};
    TEST_ASSERT_TRUE(extractStringValue("{\"ota\": true, \"url\": \"http://192.168.0.104:5000/firmware/node.bin\"}", "\"url\"", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("http://192.168.0.104:5000/firmware/node.bin", out);
}

void test_valid_node_id(void)
{
    TEST_ASSERT_TRUE(validNodeId("indoor1"));
    TEST_ASSERT_TRUE(validNodeId("outdoor12"));
    TEST_ASSERT_FALSE(validNodeId("indoor"));    // no number
    TEST_ASSERT_FALSE(validNodeId("indoor1a"));  // non-digit
    TEST_ASSERT_FALSE(validNodeId("node-E4EC")); // provisional, not a real id
    TEST_ASSERT_FALSE(validNodeId(""));          // empty
    TEST_ASSERT_FALSE(validNodeId("kitchen1"));  // bad prefix
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_extract_found);
    RUN_TEST(test_extract_missing_key);
    RUN_TEST(test_extract_empty_value);
    RUN_TEST(test_extract_too_long);
    RUN_TEST(test_extract_ota_url);
    RUN_TEST(test_valid_node_id);
    UNITY_END();
}
