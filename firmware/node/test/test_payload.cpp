#include <unity.h>
#include <cstdint>
#include <cstring>
#include "payload.h"

void test_payload_size_is_64(void)
{
    TEST_ASSERT_EQUAL_size_t(64, sizeof(SensorPayload));
}

void test_payload_fits_espnow_limit(void)
{
    // ESP-NOW packets cap at 250 bytes.
    TEST_ASSERT_TRUE(sizeof(SensorPayload) < 250);
}

void test_fw_version_fits_field(void)
{
    // fw_version[8] holds 7 chars + NUL.
    TEST_ASSERT_TRUE(strlen(FW_VERSION) < 8);
}

void test_payload_round_trip(void)
{
    SensorPayload pkt = {};
    strncpy(pkt.node_id, "indoor1", sizeof(pkt.node_id) - 1);
    strncpy(pkt.location, "Back porch", sizeof(pkt.location) - 1);
    strncpy(pkt.sensor_model, "DHT22", sizeof(pkt.sensor_model) - 1);
    strncpy(pkt.fw_version, FW_VERSION, sizeof(pkt.fw_version) - 1);
    pkt.temp_c = 27.3f;
    pkt.humidity = 36.0f;
    pkt.batt_v = 4.1f;
    pkt.boot_count = 42;

    // Pack to bytes and back — the wire format must survive intact.
    uint8_t buf[sizeof(SensorPayload)];
    memcpy(buf, &pkt, sizeof(pkt));
    SensorPayload out = {};
    memcpy(&out, buf, sizeof(out));

    TEST_ASSERT_EQUAL_STRING("indoor1", out.node_id);
    TEST_ASSERT_EQUAL_STRING("Back porch", out.location);
    TEST_ASSERT_EQUAL_STRING("DHT22", out.sensor_model);
    TEST_ASSERT_EQUAL_STRING(FW_VERSION, out.fw_version);
    TEST_ASSERT_EQUAL_FLOAT(27.3f, out.temp_c);
    TEST_ASSERT_EQUAL_FLOAT(36.0f, out.humidity);
    TEST_ASSERT_EQUAL_FLOAT(4.1f, out.batt_v);
    TEST_ASSERT_EQUAL_UINT32(42, out.boot_count);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_payload_size_is_64);
    RUN_TEST(test_payload_fits_espnow_limit);
    RUN_TEST(test_fw_version_fits_field);
    RUN_TEST(test_payload_round_trip);
    UNITY_END();
}
