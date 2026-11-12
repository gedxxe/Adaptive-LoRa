#include <unity.h>
#include <initializer_list>
#include "common/config/device_config.h"
using namespace adaptive_lora;
namespace {
DeviceConfig client() { DeviceConfig c; c.role = DeviceRole::CLIENT; return c; }
void test_unset_and_unknown_role() {
    DeviceConfig c;
    TEST_ASSERT_EQUAL_STRING("invalid_role", validateConfig(c));
    c.role = static_cast<DeviceRole>(255);
    TEST_ASSERT_EQUAL_STRING("invalid_role", validateConfig(c));
}
void test_valid_client_and_gateway() {
    auto c = client(); TEST_ASSERT_NULL(validateConfig(c));
    c.role = DeviceRole::GATEWAY; c.nodeId = 1; TEST_ASSERT_NULL(validateConfig(c));
}
void test_role_id_constraints() {
    auto c = client(); c.nodeId = 1; TEST_ASSERT_EQUAL_STRING("client_id_conflict", validateConfig(c));
    c.role = DeviceRole::GATEWAY; c.nodeId = 2; TEST_ASSERT_EQUAL_STRING("gateway_id_mismatch", validateConfig(c));
}
void test_network_full_range() {
    auto c = client(); c.networkId = 65535; TEST_ASSERT_NULL(validateConfig(c));
    c.networkId = 0; TEST_ASSERT_EQUAL_STRING("invalid_network_id", validateConfig(c));
}
void test_reserved_ids() {
    for (uint16_t id : {uint16_t(0),uint16_t(65535)}) {
        auto c = client(); c.nodeId = id; TEST_ASSERT_EQUAL_STRING("invalid_node_id", validateConfig(c));
        c = client(); c.gatewayId = id; TEST_ASSERT_EQUAL_STRING("invalid_gateway_id", validateConfig(c));
    }
}
void test_schema_and_interval() {
    auto c = client(); c.schema = 2; TEST_ASSERT_EQUAL_STRING("unsupported_schema", validateConfig(c));
    c = client(); c.helloIntervalMs = 999; TEST_ASSERT_NOT_NULL(validateConfig(c));
    c.helloIntervalMs = 3600001; TEST_ASSERT_NOT_NULL(validateConfig(c));
    c.helloIntervalMs = 1000; TEST_ASSERT_NULL(validateConfig(c));
    c.helloIntervalMs = 3600000; TEST_ASSERT_NULL(validateConfig(c));
}
void test_blob_roundtrip() {
    uint8_t bytes[kConfigBlobSize];
    auto c = client(); DeviceConfig out;
    TEST_ASSERT_TRUE(encodeConfig(c, bytes, sizeof(bytes)));
    TEST_ASSERT_NULL(decodeConfig(bytes, sizeof(bytes), out));
    TEST_ASSERT_EQUAL_UINT16(1, out.schema);
    TEST_ASSERT_EQUAL_UINT8(uint8_t(DeviceRole::CLIENT), uint8_t(out.role));
    TEST_ASSERT_EQUAL_UINT16(2, out.nodeId);
    TEST_ASSERT_EQUAL_UINT16(1, out.gatewayId);
    TEST_ASSERT_EQUAL_UINT16(1, out.networkId);
    TEST_ASSERT_EQUAL_UINT32(5000, out.helloIntervalMs);
}
void test_blob_corruption_each_byte() {
    uint8_t bytes[kConfigBlobSize]; auto c = client();
    for (size_t i = 0; i < sizeof(bytes); ++i) {
        TEST_ASSERT_TRUE(encodeConfig(c, bytes, sizeof(bytes)));
        bytes[i] ^= 0x80;
        DeviceConfig out; TEST_ASSERT_NOT_NULL(decodeConfig(bytes, sizeof(bytes), out));
        TEST_ASSERT_EQUAL_UINT8(uint8_t(DeviceRole::UNSET), uint8_t(out.role));
    }
}
void test_blob_partial_missing_oversize_and_invalid_save() {
    uint8_t bytes[25]{}; DeviceConfig out;
    TEST_ASSERT_NOT_NULL(decodeConfig(nullptr, 24, out));
    for (size_t size = 0; size < 24; ++size) TEST_ASSERT_NOT_NULL(decodeConfig(bytes, size, out));
    TEST_ASSERT_NOT_NULL(decodeConfig(bytes, 25, out));
    TEST_ASSERT_FALSE(encodeConfig(out, bytes, sizeof(bytes)));
    auto c = client(); TEST_ASSERT_FALSE(encodeConfig(c, bytes, 23));
    TEST_ASSERT_FALSE(encodeConfig(c, nullptr, 24));
}
}
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_unset_and_unknown_role); RUN_TEST(test_valid_client_and_gateway);
    RUN_TEST(test_role_id_constraints); RUN_TEST(test_network_full_range);
    RUN_TEST(test_reserved_ids); RUN_TEST(test_schema_and_interval);
    RUN_TEST(test_blob_roundtrip); RUN_TEST(test_blob_corruption_each_byte);
    RUN_TEST(test_blob_partial_missing_oversize_and_invalid_save);
    return UNITY_END();
}
