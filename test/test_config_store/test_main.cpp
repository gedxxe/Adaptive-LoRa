#include <unity.h>
#include <cstring>
#include <initializer_list>
#include <limits>
#include "app/nvs_config_store.h"
#include "common/protocol/frame_codec.h"
#include "../support/fake_config_storage.h"

using namespace adaptive_lora;
using namespace adaptive_lora::test;
namespace {
constexpr uint8_t goldenClient[24] = {
    0x4d, 0x46, 0x44, 0x43, 0x01, 0x00, 0x02, 0x00,
    0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x88, 0x13, 0x00, 0x00, 0xe7, 0x73, 0xe5, 0x84
};
constexpr uint8_t expectedNamespace[] = {0x6d, 0x69, 0x64, 0x65, 0x61, 0x2d, 0x70, 0x32, 0x70, 0};
DeviceConfig client() { DeviceConfig c; c.role = DeviceRole::CLIENT; return c; }
DeviceConfig gateway() { auto c = client(); c.role = DeviceRole::GATEWAY; c.nodeId = c.gatewayId; return c; }
DeviceConfig sentinel() { auto c = gateway(); c.networkId = 91; c.nodeId = c.gatewayId = 73; c.helloIntervalMs = 9000; return c; }
void sameConfig(const DeviceConfig& a, const DeviceConfig& b) {
    TEST_ASSERT_EQUAL_UINT16(a.schema, b.schema);
    TEST_ASSERT_EQUAL_UINT8(uint8_t(a.role), uint8_t(b.role));
    TEST_ASSERT_EQUAL_UINT16(a.networkId, b.networkId);
    TEST_ASSERT_EQUAL_UINT16(a.nodeId, b.nodeId);
    TEST_ASSERT_EQUAL_UINT16(a.gatewayId, b.gatewayId);
    TEST_ASSERT_EQUAL_UINT32(a.helloIntervalMs, b.helloIntervalMs);
}
// Fixture construction only: semantic-invalid stored blobs need a valid hash.
void reseal(uint8_t* bytes) {
    uint32_t hash = 2166136261u;
    for (unsigned i = 0; i < 20; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    put32(bytes + 20, hash);
}
unsigned count(const FakeConfigStorage& b, StorageCall wanted) {
    unsigned n = 0;
    for (size_t i = 0; i < b.traceLength; ++i) if (b.trace[i].call == wanted) ++n;
    return n;
}
void cleanup(const FakeConfigStorage& b) {
    TEST_ASSERT_FALSE(b.violation);
    TEST_ASSERT_FALSE(b.active);
    TEST_ASSERT_EQUAL_UINT(b.successfulBegins, b.ends);
    for (size_t i = 0; i < b.traceLength; ++i) {
        const auto& t = b.trace[i];
        if (t.call == StorageCall::BEGIN_READ || t.call == StorageCall::BEGIN_WRITE)
            TEST_ASSERT_EQUAL_MEMORY(expectedNamespace, t.identifier, sizeof(expectedNamespace));
        else if (t.call != StorageCall::END) TEST_ASSERT_EQUAL_STRING("device", t.identifier);
        if (t.call == StorageCall::READ || t.call == StorageCall::WRITE)
            TEST_ASSERT_EQUAL_UINT(24, t.length);
    }
}
void order(const FakeConfigStorage& b, std::initializer_list<StorageCall> expected) {
    TEST_ASSERT_EQUAL_UINT(expected.size(), b.traceLength);
    size_t i = 0;
    for (auto call : expected) TEST_ASSERT_EQUAL_INT(int(call), int(b.trace[i++].call));
    cleanup(b);
}
struct Fixture {
    FakeConfigStorage b;
    NvsConfigStore store{b};
    Fixture() { std::memcpy(b.bytes, goldenClient, sizeof(goldenClient)); b.length = sizeof(goldenClient); }
    void loadError(const char* expected) {
        const auto before = sentinel(); auto out = before;
        TEST_ASSERT_EQUAL_STRING(expected, store.load(out)); sameConfig(before, out); cleanup(b);
    }
    void saveError(const char* expected, const DeviceConfig& c = client()) {
        const auto before = c;
        TEST_ASSERT_EQUAL_STRING(expected, store.save(c)); sameConfig(before, c); cleanup(b);
    }
    StorageSession& readback() {
        auto& plan = b.sessions[1]; plan.replaceReadBytes = true;
        std::memcpy(plan.readBytes, goldenClient, sizeof(goldenClient));
        return plan;
    }
};

void test_load_valid_client_and_preserved_fields() {
    Fixture f; DeviceConfig out; TEST_ASSERT_NULL(f.store.load(out)); sameConfig(client(), out); cleanup(f.b);
}
void test_load_valid_gateway() {
    Fixture f; TEST_ASSERT_TRUE(encodeConfig(gateway(), f.b.bytes, sizeof(f.b.bytes)));
    DeviceConfig out; TEST_ASSERT_NULL(f.store.load(out)); sameConfig(gateway(), out); cleanup(f.b);
}
void test_load_begin_failure_has_no_end_or_reads() {
    Fixture f; f.b.sessions[0].beginOk = false; f.loadError("nvs_missing_or_unreadable");
    order(f.b, {StorageCall::BEGIN_READ});
}
void test_load_missing_key() {
    Fixture f; f.b.exists = false; f.loadError("nvs_missing_or_length");
    order(f.b, {StorageCall::BEGIN_READ, StorageCall::LENGTH, StorageCall::END});
}
void test_load_empty_blob() {
    Fixture f; f.b.length = 0; f.loadError("nvs_missing_or_length");
    TEST_ASSERT_EQUAL_UINT(0, count(f.b, StorageCall::READ));
}
void test_load_all_undersized_blobs_close_without_read() {
    for (size_t length = 1; length < 24; ++length) {
        Fixture f; f.b.length = length; f.loadError("nvs_missing_or_length");
        order(f.b, {StorageCall::BEGIN_READ, StorageCall::LENGTH, StorageCall::END});
    }
}
void test_load_oversized_lengths_never_read_buffer() {
    for (size_t length : {size_t(25), size_t(32), std::numeric_limits<size_t>::max()}) {
        Fixture f; f.b.length = length; f.loadError("nvs_missing_or_length");
        TEST_ASSERT_EQUAL_UINT(0, count(f.b, StorageCall::READ));
    }
}
void test_load_all_short_read_counts_leave_output_unchanged() {
    for (size_t n = 0; n < 24; ++n) {
        Fixture f; f.b.sessions[0].overrideReadCount = true; f.b.sessions[0].readCount = n;
        f.loadError("nvs_read_failed");
        order(f.b, {StorageCall::BEGIN_READ, StorageCall::LENGTH, StorageCall::READ, StorageCall::END});
    }
}
void test_load_overreported_read_count_is_rejected() {
    Fixture f; f.b.sessions[0].overrideReadCount = true; f.b.sessions[0].readCount = 25;
    f.loadError("nvs_read_failed");
}
void test_load_invalid_magic_with_valid_hash() {
    Fixture f; f.b.bytes[0] ^= 1; reseal(f.b.bytes); f.loadError("config_corrupt");
}
void test_load_invalid_checksum() {
    Fixture f; f.b.bytes[20] ^= 1; f.loadError("config_corrupt");
}
void test_load_valid_hash_unsupported_schema() {
    Fixture f; put16(f.b.bytes + 4, 2); reseal(f.b.bytes); f.loadError("unsupported_schema");
}
void test_load_valid_hash_invalid_role() {
    for (uint8_t role : {uint8_t(0), uint8_t(255)}) {
        Fixture f; f.b.bytes[6] = role; reseal(f.b.bytes); f.loadError("invalid_role");
    }
}
void test_load_valid_hash_invalid_network() {
    Fixture f; put16(f.b.bytes + 8, 0); reseal(f.b.bytes); f.loadError("invalid_network_id");
}
void test_load_valid_hash_invalid_node() {
    for (uint16_t id : {uint16_t(0), uint16_t(65535)}) {
        Fixture f; put16(f.b.bytes + 10, id); reseal(f.b.bytes); f.loadError("invalid_node_id");
    }
}
void test_load_valid_hash_invalid_gateway() {
    for (uint16_t id : {uint16_t(0), uint16_t(65535)}) {
        Fixture f; put16(f.b.bytes + 12, id); reseal(f.b.bytes); f.loadError("invalid_gateway_id");
    }
}
void test_load_gateway_id_mismatch() {
    Fixture f; f.b.bytes[6] = uint8_t(DeviceRole::GATEWAY); reseal(f.b.bytes); f.loadError("gateway_id_mismatch");
}
void test_load_client_id_conflict() {
    Fixture f; put16(f.b.bytes + 10, 1); reseal(f.b.bytes); f.loadError("client_id_conflict");
}
void test_load_valid_hash_invalid_intervals() {
    for (uint32_t interval : {uint32_t(999), uint32_t(3600001)}) {
        Fixture f; put32(f.b.bytes + 16, interval); reseal(f.b.bytes); f.loadError("invalid_hello_interval");
    }
}
void test_load_each_reserved_byte_with_valid_hash() {
    for (size_t offset : {size_t(7), size_t(14), size_t(15)}) {
        Fixture f; f.b.bytes[offset] = 1; reseal(f.b.bytes); f.loadError("config_corrupt");
    }
}
void test_load_corruption_at_every_blob_byte() {
    for (size_t i = 0; i < 24; ++i) {
        Fixture f; f.b.bytes[i] ^= 0x80; f.loadError("config_corrupt");
    }
}
void test_load_begin_failure_precedes_length_and_decode_errors() {
    Fixture f; f.b.sessions[0].beginOk = false; f.b.length = 0; f.b.bytes[0] ^= 1;
    f.loadError("nvs_missing_or_unreadable"); order(f.b, {StorageCall::BEGIN_READ});
}
void test_load_length_failure_precedes_corruption() {
    Fixture f; f.b.length = 23; f.b.bytes[0] ^= 1; f.loadError("nvs_missing_or_length");
    TEST_ASSERT_EQUAL_UINT(0, count(f.b, StorageCall::READ));
}
void test_load_short_read_precedes_decode_failure() {
    Fixture f; f.b.sessions[0].overrideReadCount = true; f.b.sessions[0].readCount = 23; f.b.bytes[0] ^= 1;
    f.loadError("nvs_read_failed");
}
void test_load_corruption_then_schema_then_role_error_priority() {
    Fixture f; put16(f.b.bytes + 4, 2); f.b.bytes[6] = 255;
    f.loadError("config_corrupt"); reseal(f.b.bytes); f.loadError("unsupported_schema");
    put16(f.b.bytes + 4, 1); reseal(f.b.bytes); f.loadError("invalid_role");
}
void test_load_readonly_exact_namespace_key_and_order() {
    Fixture f; DeviceConfig out; TEST_ASSERT_NULL(f.store.load(out));
    order(f.b, {StorageCall::BEGIN_READ, StorageCall::LENGTH, StorageCall::READ, StorageCall::END});
}
void test_save_client_exact_golden_blob_and_verification_order() {
    Fixture f; TEST_ASSERT_NULL(f.store.save(client()));
    TEST_ASSERT_EQUAL_UINT(24, f.b.lastWriteLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(goldenClient, f.b.lastWrite, 24);
    order(f.b, {StorageCall::BEGIN_WRITE, StorageCall::WRITE, StorageCall::END,
        StorageCall::BEGIN_READ, StorageCall::LENGTH, StorageCall::READ, StorageCall::END});
}
void test_save_gateway_roundtrip_and_exact_size() {
    Fixture f; TEST_ASSERT_NULL(f.store.save(gateway())); TEST_ASSERT_EQUAL_UINT(24, f.b.lastWriteLength);
    DeviceConfig decoded; TEST_ASSERT_NULL(decodeConfig(f.b.lastWrite, 24, decoded)); sameConfig(gateway(), decoded); cleanup(f.b);
}
void test_save_all_invalid_configs_rejected_before_storage() {
    struct Invalid { DeviceConfig config; const char* error; };
    Invalid cases[12]{};
    for (auto& item : cases) item.config = client();
    cases[0].config.schema = 2; cases[0].error = "unsupported_schema";
    cases[1].config.role = DeviceRole::UNSET; cases[1].error = "invalid_role";
    cases[2].config.role = static_cast<DeviceRole>(255); cases[2].error = "invalid_role";
    cases[3].config.networkId = 0; cases[3].error = "invalid_network_id";
    cases[4].config.nodeId = 0; cases[4].error = "invalid_node_id";
    cases[5].config.nodeId = 65535; cases[5].error = "invalid_node_id";
    cases[6].config.gatewayId = 0; cases[6].error = "invalid_gateway_id";
    cases[7].config.gatewayId = 65535; cases[7].error = "invalid_gateway_id";
    cases[8].config.role = DeviceRole::GATEWAY; cases[8].error = "gateway_id_mismatch";
    cases[9].config.nodeId = 1; cases[9].error = "client_id_conflict";
    cases[10].config.helloIntervalMs = 999; cases[10].error = "invalid_hello_interval";
    cases[11].config.helloIntervalMs = 3600001; cases[11].error = "invalid_hello_interval";
    for (const auto& item : cases) {
        Fixture f; f.saveError(item.error, item.config); order(f.b, {});
        TEST_ASSERT_EQUAL_UINT8_ARRAY(goldenClient, f.b.bytes, 24);
    }
}
void test_save_validation_error_precedes_open_error() {
    Fixture f; f.b.sessions[0].beginOk = false; auto c = client(); c.schema = 2; c.role = DeviceRole::UNSET;
    f.saveError("unsupported_schema", c); order(f.b, {});
}
void test_save_open_failure_has_no_end_or_readback() {
    Fixture f; f.b.sessions[0].beginOk = false; f.saveError("nvs_open_failed");
    order(f.b, {StorageCall::BEGIN_WRITE});
}
void test_save_zero_write_result() {
    Fixture f; f.b.overrideWriteCount = true; f.b.writeCount = 0; f.saveError("nvs_write_failed");
    order(f.b, {StorageCall::BEGIN_WRITE, StorageCall::WRITE, StorageCall::END});
}
void test_save_all_short_write_counts_close_without_readback() {
    for (size_t n = 1; n < 24; ++n) {
        Fixture f; f.b.overrideWriteCount = true; f.b.writeCount = n; f.saveError("nvs_write_failed");
        order(f.b, {StorageCall::BEGIN_WRITE, StorageCall::WRITE, StorageCall::END});
    }
}
void test_save_overreported_write_count_is_rejected() {
    Fixture f; f.b.overrideWriteCount = true; f.b.writeCount = 25; f.saveError("nvs_write_failed");
    TEST_ASSERT_EQUAL_UINT(0, count(f.b, StorageCall::READ));
}
void test_save_write_failure_precedes_scripted_readback_failure() {
    Fixture f; f.b.overrideWriteCount = true; f.b.sessions[1].beginOk = false; f.saveError("nvs_write_failed");
    TEST_ASSERT_EQUAL_UINT(1, f.b.begins); TEST_ASSERT_EQUAL_UINT(0, count(f.b, StorageCall::LENGTH));
}
void test_save_write_failure_can_follow_backing_mutation_without_success() {
    Fixture f; auto c = client(); c.nodeId = 19;
    f.b.overrideWriteCount = true; f.b.mutateOnWriteFailure = true; f.saveError("nvs_write_failed", c);
    DeviceConfig out; TEST_ASSERT_NULL(decodeConfig(f.b.bytes, 24, out)); sameConfig(c, out);
    TEST_ASSERT_EQUAL_UINT(1, f.b.begins); // Failure does not promise rollback.
}
void test_save_readback_open_failure_no_extra_end() {
    Fixture f; f.b.sessions[1].beginOk = false; f.saveError("nvs_missing_or_unreadable");
    order(f.b, {StorageCall::BEGIN_WRITE, StorageCall::WRITE, StorageCall::END, StorageCall::BEGIN_READ});
}
void test_save_readback_all_wrong_lengths() {
    for (size_t n = 0; n < 26; ++n) {
        if (n == 24) continue;
        Fixture f; f.b.sessions[1].overrideLength = true; f.b.sessions[1].reportedLength = n;
        f.saveError("nvs_missing_or_length");
        order(f.b, {StorageCall::BEGIN_WRITE, StorageCall::WRITE, StorageCall::END,
            StorageCall::BEGIN_READ, StorageCall::LENGTH, StorageCall::END});
    }
}
void test_save_readback_all_short_read_counts() {
    for (size_t n = 0; n < 24; ++n) {
        Fixture f; f.b.sessions[1].overrideReadCount = true; f.b.sessions[1].readCount = n;
        f.saveError("nvs_read_failed"); TEST_ASSERT_EQUAL_UINT(2, f.b.ends);
    }
}
void test_save_readback_overreported_count() {
    Fixture f; f.b.sessions[1].overrideReadCount = true; f.b.sessions[1].readCount = 25; f.saveError("nvs_read_failed");
}
void test_save_readback_invalid_magic_with_valid_hash() {
    Fixture f; auto& plan = f.readback(); plan.readBytes[0] ^= 1; reseal(plan.readBytes); f.saveError("config_corrupt");
}
void test_save_readback_checksum_corruption() {
    Fixture f; f.readback().readBytes[20] ^= 1; f.saveError("config_corrupt");
}
void test_save_readback_unsupported_schema_propagates() {
    Fixture f; auto& plan = f.readback(); put16(plan.readBytes + 4, 2); reseal(plan.readBytes); f.saveError("unsupported_schema");
}
void test_save_readback_invalid_role_propagates() {
    Fixture f; auto& plan = f.readback(); plan.readBytes[6] = 255; reseal(plan.readBytes); f.saveError("invalid_role");
}
void test_save_readback_invalid_device_ids_propagate() {
    const size_t offsets[] = {8, 10, 12};
    const char* errors[] = {"invalid_network_id", "invalid_node_id", "invalid_gateway_id"};
    for (size_t i = 0; i < 3; ++i) {
        Fixture f; auto& plan = f.readback(); put16(plan.readBytes + offsets[i], 0); reseal(plan.readBytes); f.saveError(errors[i]);
    }
}
void test_save_readback_reserved_bytes_propagate() {
    for (size_t offset : {size_t(7), size_t(14), size_t(15)}) {
        Fixture f; auto& plan = f.readback(); plan.readBytes[offset] = 1; reseal(plan.readBytes); f.saveError("config_corrupt");
    }
}
void test_save_readback_invalid_interval_propagates() {
    Fixture f; auto& plan = f.readback(); put32(plan.readBytes + 16, 999); reseal(plan.readBytes); f.saveError("invalid_hello_interval");
}
void test_save_different_valid_readback_reports_verify_failed() {
    for (unsigned field = 0; field < 5; ++field) {
        Fixture f; auto different = client();
        if (field == 0) different.networkId = 91;
        if (field == 1) different.nodeId = 19;
        if (field == 2) different.gatewayId = 7;
        if (field == 3) different.helloIntervalMs = 1000;
        if (field == 4) different = gateway();
        auto& plan = f.readback(); TEST_ASSERT_TRUE(encodeConfig(different, plan.readBytes, sizeof(plan.readBytes)));
        f.saveError("nvs_verify_failed"); TEST_ASSERT_EQUAL_UINT(2, f.b.ends);
    }
}
void test_save_does_not_modify_input_configuration() {
    Fixture f; const auto c = sentinel(); const auto before = c;
    TEST_ASSERT_NULL(f.store.save(c)); sameConfig(before, c); cleanup(f.b);
}
void test_failed_readback_reports_failure_even_if_new_bytes_are_present() {
    Fixture f; auto c = client(); c.nodeId = 19; f.b.sessions[1].beginOk = false;
    f.saveError("nvs_missing_or_unreadable", c);
    DeviceConfig out; TEST_ASSERT_NULL(f.store.load(out)); sameConfig(c, out); cleanup(f.b);
}
void test_save_client_then_load() {
    Fixture f; f.b.exists = false; TEST_ASSERT_NULL(f.store.save(client()));
    DeviceConfig out; TEST_ASSERT_NULL(f.store.load(out)); sameConfig(client(), out); cleanup(f.b);
}
void test_save_gateway_then_load() {
    Fixture f; TEST_ASSERT_NULL(f.store.save(gateway()));
    DeviceConfig out; TEST_ASSERT_NULL(f.store.load(out)); sameConfig(gateway(), out); cleanup(f.b);
}
void test_save_updated_ids_and_hello_interval() {
    Fixture f; auto c = client(); c.networkId = 65535; c.nodeId = 65534; c.gatewayId = 27; c.helloIntervalMs = 3600000;
    TEST_ASSERT_NULL(f.store.save(c)); DeviceConfig out; TEST_ASSERT_NULL(f.store.load(out)); sameConfig(c, out); cleanup(f.b);
}
void test_invalid_save_preserves_existing_blob_and_key() {
    Fixture f; const auto c = gateway(); TEST_ASSERT_NULL(f.store.save(c));
    uint8_t before[24]; std::memcpy(before, f.b.bytes, sizeof(before));
    const size_t callsBefore = f.b.traceLength; auto invalid = client(); invalid.nodeId = invalid.gatewayId;
    f.saveError("client_id_conflict", invalid);
    TEST_ASSERT_EQUAL_UINT(callsBefore, f.b.traceLength); TEST_ASSERT_EQUAL_UINT8_ARRAY(before, f.b.bytes, 24);
    DeviceConfig out; TEST_ASSERT_NULL(f.store.load(out)); sameConfig(c, out); cleanup(f.b);
}
void test_failed_then_successful_sessions_do_not_leak_handles() {
    Fixture f; f.b.sessions[0].beginOk = false; f.loadError("nvs_missing_or_unreadable");
    TEST_ASSERT_NULL(f.store.save(gateway())); DeviceConfig out; TEST_ASSERT_NULL(f.store.load(out));
    sameConfig(gateway(), out); TEST_ASSERT_EQUAL_UINT(4, f.b.begins); TEST_ASSERT_EQUAL_UINT(3, f.b.ends); cleanup(f.b);
}
void test_repeated_role_updates_and_loads() {
    Fixture f;
    for (unsigned n = 0; n < 3; ++n) {
        auto c = n % 2 ? gateway() : client(); c.networkId = uint16_t(n + 1); c.helloIntervalMs = 1000 * (n + 1);
        TEST_ASSERT_NULL(f.store.save(c)); DeviceConfig out; TEST_ASSERT_NULL(f.store.load(out)); sameConfig(c, out); cleanup(f.b);
    }
    TEST_ASSERT_EQUAL_UINT(9, f.b.begins); TEST_ASSERT_EQUAL_UINT(9, f.b.ends);
}
}  // namespace
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_load_valid_client_and_preserved_fields);
    RUN_TEST(test_load_valid_gateway);
    RUN_TEST(test_load_begin_failure_has_no_end_or_reads);
    RUN_TEST(test_load_missing_key);
    RUN_TEST(test_load_empty_blob);
    RUN_TEST(test_load_all_undersized_blobs_close_without_read);
    RUN_TEST(test_load_oversized_lengths_never_read_buffer);
    RUN_TEST(test_load_all_short_read_counts_leave_output_unchanged);
    RUN_TEST(test_load_overreported_read_count_is_rejected);
    RUN_TEST(test_load_invalid_magic_with_valid_hash);
    RUN_TEST(test_load_invalid_checksum);
    RUN_TEST(test_load_valid_hash_unsupported_schema);
    RUN_TEST(test_load_valid_hash_invalid_role);
    RUN_TEST(test_load_valid_hash_invalid_network);
    RUN_TEST(test_load_valid_hash_invalid_node);
    RUN_TEST(test_load_valid_hash_invalid_gateway);
    RUN_TEST(test_load_gateway_id_mismatch);
    RUN_TEST(test_load_client_id_conflict);
    RUN_TEST(test_load_valid_hash_invalid_intervals);
    RUN_TEST(test_load_each_reserved_byte_with_valid_hash);
    RUN_TEST(test_load_corruption_at_every_blob_byte);
    RUN_TEST(test_load_begin_failure_precedes_length_and_decode_errors);
    RUN_TEST(test_load_length_failure_precedes_corruption);
    RUN_TEST(test_load_short_read_precedes_decode_failure);
    RUN_TEST(test_load_corruption_then_schema_then_role_error_priority);
    RUN_TEST(test_load_readonly_exact_namespace_key_and_order);
    RUN_TEST(test_save_client_exact_golden_blob_and_verification_order);
    RUN_TEST(test_save_gateway_roundtrip_and_exact_size);
    RUN_TEST(test_save_all_invalid_configs_rejected_before_storage);
    RUN_TEST(test_save_validation_error_precedes_open_error);
    RUN_TEST(test_save_open_failure_has_no_end_or_readback);
    RUN_TEST(test_save_zero_write_result);
    RUN_TEST(test_save_all_short_write_counts_close_without_readback);
    RUN_TEST(test_save_overreported_write_count_is_rejected);
    RUN_TEST(test_save_write_failure_precedes_scripted_readback_failure);
    RUN_TEST(test_save_write_failure_can_follow_backing_mutation_without_success);
    RUN_TEST(test_save_readback_open_failure_no_extra_end);
    RUN_TEST(test_save_readback_all_wrong_lengths);
    RUN_TEST(test_save_readback_all_short_read_counts);
    RUN_TEST(test_save_readback_overreported_count);
    RUN_TEST(test_save_readback_invalid_magic_with_valid_hash);
    RUN_TEST(test_save_readback_checksum_corruption);
    RUN_TEST(test_save_readback_unsupported_schema_propagates);
    RUN_TEST(test_save_readback_invalid_role_propagates);
    RUN_TEST(test_save_readback_invalid_device_ids_propagate);
    RUN_TEST(test_save_readback_reserved_bytes_propagate);
    RUN_TEST(test_save_readback_invalid_interval_propagates);
    RUN_TEST(test_save_different_valid_readback_reports_verify_failed);
    RUN_TEST(test_save_does_not_modify_input_configuration);
    RUN_TEST(test_failed_readback_reports_failure_even_if_new_bytes_are_present);
    RUN_TEST(test_save_client_then_load);
    RUN_TEST(test_save_gateway_then_load);
    RUN_TEST(test_save_updated_ids_and_hello_interval);
    RUN_TEST(test_invalid_save_preserves_existing_blob_and_key);
    RUN_TEST(test_failed_then_successful_sessions_do_not_leak_handles);
    RUN_TEST(test_repeated_role_updates_and_loads);
    return UNITY_END();
}
