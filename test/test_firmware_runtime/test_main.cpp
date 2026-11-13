#include <unity.h>
#include <Arduino.h>
#include <esp_system.h>
#include <esp_random.h>
#include <cstring>
#include <string>
#include <initializer_list>
#include "app/firmware_runtime.h"
#include "../support/fake_radio_hardware.h"
#include "../support/fake_config_storage.h"

using namespace adaptive_lora;
using namespace adaptive_lora::test;
namespace {
constexpr int16_t injected = -701;
DeviceConfig config(DeviceRole role) {
    DeviceConfig c; c.role = role; c.nodeId = role == DeviceRole::GATEWAY ? 1 : 2; c.helloIntervalMs = 1000; return c;
}
unsigned occurrences(const std::string& text, const char* value) {
    unsigned count = 0;
    const size_t length = std::strlen(value);
    for (size_t p = 0; (p = text.find(value, p)) != std::string::npos; p += length) ++count;
    return count;
}
void contains(const char* value) { TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find(value)); }
void absent(const char* value) { TEST_ASSERT_EQUAL(std::string::npos, Serial.output.find(value)); }
void reseal(uint8_t* bytes) {
    uint32_t hash = 2166136261u;
    for (unsigned i = 0; i < 20; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    put32(bytes + 20, hash);
}
struct Fixture {
    FakeConfigStorage storage;
    FakeRadioHardware hardware;
    runtime_optional::optional<FirmwareRuntime> runtime;
    uint64_t now = 0;
    uint64_t totals[static_cast<size_t>(HardwareCall::COUNT)]{};
    size_t initObservations = 0, rngBeforeSetup = 0;
    explicit Fixture(DeviceRole role = DeviceRole::UNSET) {
        Serial.reset(); host_clock::set(0); hostReset() = HostReset{}; hostRandom() = HostRandom{};
        random({100, 200, 300, 400});
        storage.exists = role != DeviceRole::UNSET;
        if (storage.exists) {
            TEST_ASSERT_TRUE(encodeConfig(config(role), storage.bytes, sizeof(storage.bytes)));
            storage.length = kConfigBlobSize;
        }
        hardware.onBegin = observeInit; hardware.beginContext = this;
        runtime.emplace(storage, hardware);
    }
    static void observeInit(void* context) {
        auto& f = *static_cast<Fixture*>(context);
        ++f.initObservations;
        TEST_ASSERT_EQUAL_UINT32(kSerialBaud, Serial.baud);
        TEST_ASSERT_FALSE(f.storage.active);
        TEST_ASSERT_EQUAL_UINT(f.storage.successfulBegins, f.storage.ends);
        TEST_ASSERT_TRUE(f.storage.traceLength >= 4);
        TEST_ASSERT_EQUAL_INT(int(StorageCall::END), int(f.storage.trace[f.storage.traceLength - 1].call));
        TEST_ASSERT_EQUAL_INT(int(StorageCall::READ), int(f.storage.trace[f.storage.traceLength - 2].call));
        TEST_ASSERT_EQUAL_UINT(f.rngBeforeSetup, hostRandom().calls);
    }
    void random(std::initializer_list<uint32_t> values) {
        hostRandom() = HostRandom{}; TEST_ASSERT_TRUE(values.size() <= 16);
        for (auto value : values) hostRandom().values[hostRandom().length++] = value;
    }
    void collect() {
        TEST_ASSERT_FALSE(hardware.violation);
        TEST_ASSERT_FALSE(hostRandom().exhausted);
        TEST_ASSERT_FALSE(storage.violation);
        TEST_ASSERT_FALSE(storage.active);
        TEST_ASSERT_EQUAL_UINT(storage.successfulBegins, storage.ends);
        for (size_t i = 0; i < static_cast<size_t>(HardwareCall::COUNT); ++i) totals[i] += hardware.calls[i];
        hardware.resetTrace();
        // Each injected IRQ is one completed low-level stimulus. Actual driver
        // finish calls would clear the physical flags; no driver state is modeled.
        hardware.signal(0, false);
    }
    uint64_t calls(HardwareCall call) const { return totals[static_cast<size_t>(call)]; }
    void start(bool drain = true) {
        host_clock::set(now); rngBeforeSetup = hostRandom().calls;
        runtime->setup(); collect(); if (drain) flush();
    }
    void tick(uint64_t time) { now = time; host_clock::set(time); runtime->loop(); collect(); }
    void tick() { tick(now); }
    void flush() {
        for (unsigned i = 0; i < 256; ++i) {
            const size_t before = Serial.output.size(); tick();
            if (Serial.output.size() == before) return;
        }
        TEST_FAIL_MESSAGE("Diagnostic drain did not quiesce within fixture budget");
    }
    void send(const std::string& bytes, bool drain = true) {
        TEST_ASSERT_TRUE(Serial.inject(bytes.data(), bytes.size()));
        while (Serial.available()) tick();
        if (drain) flush();
    }
    void clear() { Serial.output.clear(); }
    void rx(const Frame& frame, uint64_t time) {
        TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(encodeFrame(frame, hardware.rxBytes, sizeof(hardware.rxBytes), hardware.packetLength)));
        hardware.signal(radio_irq::rxDone); tick(time);
    }
    void txDone(uint64_t time) { hardware.signal(radio_irq::txDone); tick(time); }
    Frame transmitted() {
        TEST_ASSERT_TRUE(hardware.txLength >= kHeaderSize);
        Frame frame;
        TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(decodeFrame(hardware.txBytes, hardware.txLength,
            get16(hardware.txBytes + 6), get16(hardware.txBytes + 10), frame)));
        return frame;
    }
    void noRadio() {
        for (size_t i = 0; i < static_cast<size_t>(HardwareCall::COUNT); ++i) TEST_ASSERT_EQUAL_UINT64(0, totals[i]);
        TEST_ASSERT_EQUAL_UINT(0, hostRandom().calls);
    }
};
Frame hello(uint16_t sequence = 0, uint32_t boot = 730) {
    Frame f; f.networkId = 1; f.sourceId = 2; f.destinationId = 1; f.bootId = boot; f.sequence = sequence; return f;
}
Frame ack(const Frame& request) {
    Frame f; f.type = MessageType::ACK; f.networkId = request.networkId;
    f.sourceId = request.destinationId; f.destinationId = request.sourceId; f.bootId = 900;
    setAckPayload(f, request); return f;
}
void assertProvisioning(Fixture& f, const char* error) {
    f.start(); contains("event=BOOT"); contains("event=CONFIG_INVALID"); contains(error);
    contains("event=PROVISIONING"); contains("mode=PROVISIONING,radio=disabled");
    f.tick(1000000); f.send("status\n"); f.noRadio(); absent("event=RADIO_INIT"); absent("event=TX_START");
}

void test_gateway_boot_order_and_exclusive_activation() {
    Fixture f(DeviceRole::GATEWAY); f.start();
    TEST_ASSERT_EQUAL_UINT(1, f.initObservations); TEST_ASSERT_EQUAL_UINT(1, hostRandom().calls);
    TEST_ASSERT_EQUAL_UINT64(1, f.calls(HardwareCall::BEGIN)); TEST_ASSERT_EQUAL_UINT64(1, f.calls(HardwareCall::START_RX));
    TEST_ASSERT_EQUAL_UINT64(0, f.calls(HardwareCall::START_TX));
    contains("event=CONFIG_VALID"); contains("event=RADIO_INIT"); contains("mode=GATEWAY,radio=ready");
    contains("scope=loaded,role=GATEWAY,network_id=1,node_id=1,gateway_id=1,schema=1,validity=valid");
    absent("event=TX_START"); TEST_ASSERT_EQUAL_UINT32(115200, Serial.baud);
}
void test_client_boot_schedules_first_hello_after_one_second() {
    Fixture f(DeviceRole::CLIENT); f.start();
    TEST_ASSERT_EQUAL_UINT(1, hostRandom().calls); TEST_ASSERT_EQUAL_UINT64(0, f.calls(HardwareCall::START_TX));
    contains("mode=CLIENT,radio=ready");
    f.tick(999999); TEST_ASSERT_EQUAL_UINT64(0, f.calls(HardwareCall::START_TX));
    f.tick(1000000); TEST_ASSERT_EQUAL_UINT64(1, f.calls(HardwareCall::START_TX));
    const auto frame = f.transmitted(); TEST_ASSERT_EQUAL_INT(int(MessageType::HELLO), int(frame.type));
    TEST_ASSERT_EQUAL_UINT16(2, frame.sourceId); TEST_ASSERT_EQUAL_UINT32(100, frame.bootId);
}
void test_missing_config_provisioning_poll_has_no_hardware_access() {
    Fixture f; assertProvisioning(f, "nvs_missing_or_length");
    f.tick(uint64_t(UINT32_MAX) + 1000); f.noRadio();
}
void test_corrupt_config_boot_fails_safe() {
    Fixture f(DeviceRole::CLIENT); f.storage.bytes[20] ^= 1; assertProvisioning(f, "config_corrupt");
}
void test_unsupported_schema_boot_fails_safe() {
    Fixture f(DeviceRole::CLIENT); put16(f.storage.bytes + 4, 2); reseal(f.storage.bytes);
    assertProvisioning(f, "unsupported_schema");
}
void test_invalid_role_boot_fails_safe() {
    Fixture f(DeviceRole::CLIENT); f.storage.bytes[6] = 255; reseal(f.storage.bytes);
    assertProvisioning(f, "invalid_role");
}
void test_invalid_network_and_device_ids_boot_fail_safe() {
    for (size_t offset : {size_t(8), size_t(10), size_t(12)}) {
        Fixture f(DeviceRole::CLIENT); put16(f.storage.bytes + offset, 0); reseal(f.storage.bytes);
        assertProvisioning(f, offset == 8 ? "invalid_network_id" : offset == 10 ? "invalid_node_id" : "invalid_gateway_id");
    }
}
void test_nvs_open_failure_boot_fails_safe() {
    Fixture f; f.storage.sessions[0].beginOk = false; assertProvisioning(f, "nvs_missing_or_unreadable");
}
void test_short_read_boot_fails_safe() {
    Fixture f(DeviceRole::GATEWAY); f.storage.sessions[0].overrideReadCount = true; f.storage.sessions[0].readCount = 23;
    assertProvisioning(f, "nvs_read_failed");
}
void test_radio_init_failure_never_constructs_either_app() {
    for (auto role : {DeviceRole::CLIENT, DeviceRole::GATEWAY}) {
        Fixture f(role); f.hardware.beginCode = injected; f.start(); f.tick(2000000); f.send("status\n");
        TEST_ASSERT_EQUAL_UINT(1, f.initObservations); TEST_ASSERT_EQUAL_UINT(0, hostRandom().calls);
        TEST_ASSERT_EQUAL_UINT64(0, f.calls(HardwareCall::START_RX)); TEST_ASSERT_EQUAL_UINT64(0, f.calls(HardwareCall::START_TX));
        contains("code=-701,result=failed"); contains("mode=RADIO_FAULT,radio=disabled");
    }
}
void test_header_or_crc_init_failure_prevents_activation() {
    for (bool crc : {false, true}) {
        Fixture f(DeviceRole::GATEWAY);
        if (crc) f.hardware.crcCode = injected; else f.hardware.headerCode = injected;
        f.start(); TEST_ASSERT_EQUAL_UINT(0, hostRandom().calls);
        TEST_ASSERT_EQUAL_UINT64(0, f.calls(HardwareCall::ENABLE_IRQ)); TEST_ASSERT_EQUAL_UINT64(0, f.calls(HardwareCall::START_RX));
        contains("code=-701,result=failed");
    }
}
void test_provision_gateway_save_is_inactive_until_new_runtime_boot() {
    Fixture f; f.start(); f.clear();
    f.send("config set role gateway\nconfig set node-id 1\nconfig save\nconfig show\nstatus\n");
    contains("event=CONFIG_SAVED"); contains("scope=staged,role=GATEWAY"); contains("mode=PROVISIONING,radio=disabled"); f.noRadio();
    DeviceConfig saved; TEST_ASSERT_NULL(decodeConfig(f.storage.bytes, 24, saved)); TEST_ASSERT_EQUAL_INT(int(DeviceRole::GATEWAY), int(saved.role));
    f.runtime.emplace(f.storage, f.hardware); f.now = 0; f.start();
    TEST_ASSERT_EQUAL_UINT(1, f.initObservations); contains("mode=GATEWAY,radio=ready");
}
void test_provision_client_save_is_inactive_until_new_runtime_boot() {
    Fixture f; f.start(); f.clear(); f.send("config set role client\nconfig save\nstatus\n");
    contains("verified_reboot_required"); contains("mode=PROVISIONING,radio=disabled"); f.noRadio();
    f.runtime.emplace(f.storage, f.hardware); f.now = 0; f.start(); f.tick(1000000);
    TEST_ASSERT_EQUAL_INT(int(MessageType::HELLO), int(f.transmitted().type));
}
void test_client_staging_and_save_preserve_active_role_and_ids() {
    Fixture f(DeviceRole::CLIENT); f.start(); f.clear();
    f.send("config set role gateway\nconfig set node-id 1\nconfig set network-id 9\nconfig save\nconfig show\nstatus\n");
    contains("scope=staged,role=GATEWAY,network_id=9,node_id=1"); contains("scope=loaded,role=CLIENT,network_id=1,node_id=2");
    contains("mode=CLIENT,radio=ready"); f.tick(1000000); const auto packet = f.transmitted();
    TEST_ASSERT_EQUAL_INT(int(MessageType::HELLO), int(packet.type)); TEST_ASSERT_EQUAL_UINT16(1, packet.networkId); TEST_ASSERT_EQUAL_UINT16(2, packet.sourceId);
    TEST_ASSERT_EQUAL_UINT(1, f.initObservations); TEST_ASSERT_EQUAL_UINT(1, hostRandom().calls);
}
void test_gateway_staging_client_preserves_active_gateway() {
    Fixture f(DeviceRole::GATEWAY); f.start(); f.clear();
    f.send("config set role client\nconfig set node-id 2\nconfig save\nstatus\n");
    contains("mode=GATEWAY,radio=ready"); f.rx(hello(), 1);
    TEST_ASSERT_EQUAL_INT(int(MessageType::ACK), int(f.transmitted().type)); TEST_ASSERT_EQUAL_UINT(1, hostRandom().calls);
}
void test_invalid_config_save_rejected_without_storage_access() {
    Fixture f; f.start(); f.clear(); const auto before = f.storage.begins;
    f.send("config save\nconfig set role gateway\nconfig save\n");
    contains("result=invalid_role"); contains("result=gateway_id_mismatch"); absent("event=CONFIG_SAVED");
    TEST_ASSERT_EQUAL_UINT(before, f.storage.begins); f.noRadio();
}
void test_storage_write_failure_dispatches_exact_error() {
    Fixture f; f.start(); f.clear(); f.storage.overrideWriteCount = true;
    f.send("config set role client\nconfig save\n"); contains("event=CONFIG_ERROR"); contains("result=nvs_write_failed");
    absent("event=CONFIG_SAVED"); TEST_ASSERT_EQUAL_UINT(2, f.storage.begins); f.noRadio();
}
void test_storage_readback_failure_does_not_activate_saved_role() {
    Fixture f; f.start(); f.clear(); f.storage.sessions[2].beginOk = false;
    f.send("config set role client\nconfig save\nstatus\n"); contains("result=nvs_missing_or_unreadable");
    absent("event=CONFIG_SAVED"); contains("mode=PROVISIONING,radio=disabled"); f.noRadio();
}
void test_valid_different_readback_reports_verify_error() {
    Fixture f; f.start(); f.clear(); auto other = config(DeviceRole::CLIENT); other.nodeId = 19;
    auto& plan = f.storage.sessions[2]; plan.replaceReadBytes = true;
    TEST_ASSERT_TRUE(encodeConfig(other, plan.readBytes, sizeof(plan.readBytes)));
    f.send("config set role client\nconfig save\n"); contains("result=nvs_verify_failed"); absent("event=CONFIG_SAVED"); f.noRadio();
}
void test_serial_partial_command_spans_loops() {
    Fixture f; f.start(); f.clear(); f.send("sta"); absent("event=STATUS");
    f.send("tus\n"); TEST_ASSERT_EQUAL_UINT(1, occurrences(Serial.output, "event=STATUS")); f.noRadio();
}
void test_serial_input_budget_is_exactly_64_bytes() {
    Fixture f; f.start(); f.clear(); const auto bytes = std::string(63, ' ') + "status\n";
    TEST_ASSERT_TRUE(Serial.inject(bytes.data(), bytes.size())); const auto before = Serial.readCalls;
    f.tick(); TEST_ASSERT_EQUAL_UINT(64, Serial.readCalls - before); absent("event=STATUS");
    f.tick(); f.flush(); TEST_ASSERT_EQUAL_UINT(bytes.size(), Serial.readCalls - before); contains("event=STATUS");
}
void test_multiple_serial_commands_cr_lf_and_help_dispatch() {
    Fixture f; f.start(); f.clear(); f.send("help\r\nconfig show\rstatus\n");
    contains("command=help|status|config show|config save|reboot"); contains("command=config set hello-interval-ms");
    contains("scope=staged"); TEST_ASSERT_EQUAL_UINT(1, occurrences(Serial.output, "event=STATUS"));
}
void test_crlf_save_dispatches_only_once() {
    Fixture f(DeviceRole::CLIENT); f.start(); f.clear(); f.send("config save\r\n");
    TEST_ASSERT_EQUAL_UINT(1, occurrences(Serial.output, "event=CONFIG_SAVED")); TEST_ASSERT_EQUAL_UINT(3, f.storage.begins);
}
void test_invalid_command_and_number_do_not_mutate_or_reboot() {
    Fixture f; f.start(); f.clear(); f.send("reboot junk\nconfig set node-id -1\nconfig show\n");
    contains("result=invalid_command"); contains("result=invalid_number"); contains("role=UNSET,network_id=1,node_id=2,gateway_id=1");
    f.tick(200000); TEST_ASSERT_EQUAL_UINT(0, hostReset().calls); f.noRadio();
}
void test_control_byte_line_cannot_execute_embedded_reboot() {
    Fixture f; f.start(); f.clear(); std::string bytes = "reboot"; bytes.push_back('\0'); bytes += "\nstatus\n";
    f.send(bytes); contains("result=line_too_long"); contains("event=STATUS");
    f.tick(200000); TEST_ASSERT_EQUAL_UINT(0, hostReset().calls); absent("waiting_for_tx_completion");
}
void test_overflow_line_recovery_keeps_following_status() {
    Fixture f; f.start(); f.clear(); f.send(std::string(140, 'x') + "reboot\nstatus\n");
    contains("result=line_too_long"); contains("event=STATUS"); absent("waiting_for_tx_completion"); TEST_ASSERT_EQUAL_UINT(0, hostReset().calls);
}
void test_gateway_receives_hello_and_only_gateway_sends_ack() {
    Fixture f(DeviceRole::GATEWAY); f.start(); f.clear(); const auto request = hello(17, 44); f.rx(request, 1);
    const auto response = f.transmitted(); TEST_ASSERT_TRUE(ackMatches(response, request, 1));
    TEST_ASSERT_EQUAL_UINT32(100, response.bootId); TEST_ASSERT_EQUAL_UINT16(0, response.sequence);
    f.txDone(2); f.flush(); contains("event=HELLO_RX"); contains("event=ACK_TX"); absent("result=hello_sent");
}
void test_client_receives_matching_ack_and_only_client_accepts_it() {
    Fixture f(DeviceRole::CLIENT); f.start(); f.clear(); f.tick(1000000); const auto request = f.transmitted();
    f.txDone(1000010); f.rx(ack(request), 1000020); f.flush();
    contains("event=ACK_RX"); contains("result=accepted"); absent("event=ACK_TX"); absent("event=HELLO_RX");
    TEST_ASSERT_EQUAL_UINT(1, hostRandom().calls); TEST_ASSERT_EQUAL_UINT64(1, f.calls(HardwareCall::START_TX));
}
void test_gateway_receive_start_fault_is_reported_then_recovers() {
    Fixture f(DeviceRole::GATEWAY); f.hardware.startRxCode = injected; f.start();
    contains("mode=RADIO_FAULT,radio=fault"); contains("result=rx_start_failed"); TEST_ASSERT_EQUAL_UINT(1, hostRandom().calls);
    f.hardware.startRxCode = 0; f.tick(999999); TEST_ASSERT_EQUAL_UINT64(1, f.calls(HardwareCall::START_RX));
    f.tick(1000000); f.send("status\n"); contains("mode=GATEWAY,radio=ready");
    TEST_ASSERT_EQUAL_UINT64(2, f.calls(HardwareCall::START_RX)); TEST_ASSERT_EQUAL_UINT(1, f.initObservations);
}
void test_runtime_dispatches_receive_integrity_error() {
    Fixture f(DeviceRole::GATEWAY); f.start(); f.clear(); f.hardware.signal(radio_irq::crcError); f.tick(1); f.flush();
    contains("event=CRC_ERROR"); contains("code=-7"); TEST_ASSERT_EQUAL_UINT64(0, f.calls(HardwareCall::START_TX));
    TEST_ASSERT_EQUAL_UINT64(2, f.calls(HardwareCall::START_RX));
}
void test_idle_provisioning_reboot_stop_is_hardware_noop() {
    Fixture f; f.start(); f.clear(); f.send("reboot\n"); contains("result=radio_stopped");
    f.noRadio(); TEST_ASSERT_EQUAL_UINT(0, hostReset().calls);
    f.tick(100000); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls); f.noRadio();
}
void test_reboot_while_receiving_stops_once() {
    Fixture f(DeviceRole::GATEWAY); f.start(); f.send("reboot\n");
    TEST_ASSERT_EQUAL_UINT64(1, f.calls(HardwareCall::STANDBY));
    const auto rx = f.calls(HardwareCall::START_RX); f.tick(99999); TEST_ASSERT_EQUAL_UINT(0, hostReset().calls);
    f.tick(100000); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls); TEST_ASSERT_EQUAL_UINT64(rx, f.calls(HardwareCall::START_RX));
}
void test_reboot_waits_for_actual_tx_done_before_stop() {
    Fixture f(DeviceRole::CLIENT); f.start(); f.tick(1000000); f.send("reboot\n");
    TEST_ASSERT_EQUAL_UINT64(1, f.calls(HardwareCall::STANDBY)); // TX pre-standby only.
    f.tick(1500000); TEST_ASSERT_EQUAL_UINT(0, hostReset().calls);
    f.txDone(1500001); f.flush(); contains("result=hello_sent"); contains("result=radio_stopped");
    TEST_ASSERT_EQUAL_UINT64(2, f.calls(HardwareCall::STANDBY));
    f.tick(1600000); TEST_ASSERT_EQUAL_UINT(0, hostReset().calls); f.tick(1600001); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls);
    TEST_ASSERT_EQUAL_UINT64(1, f.calls(HardwareCall::START_TX));
}
void test_reboot_waits_for_driver_tx_timeout_and_then_stops() {
    Fixture f(DeviceRole::CLIENT); f.start(); f.tick(1000000); f.send("reboot\n");
    f.tick(2999999); TEST_ASSERT_EQUAL_UINT(0, hostReset().calls);
    f.tick(3000000); f.flush(); contains("result=tx_failed"); contains("result=radio_stopped");
    TEST_ASSERT_EQUAL_UINT64(3, f.calls(HardwareCall::STANDBY)); // TX start, driver timeout, runtime stop.
    f.tick(3100000); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls);
}
void test_reboot_stop_failure_logs_and_still_requests_reset() {
    Fixture f(DeviceRole::GATEWAY); f.start(); f.clear(); f.hardware.standbyCode = injected; f.send("reboot\n");
    contains("code=-701,result=standby_failed_restarting"); TEST_ASSERT_EQUAL_UINT64(1, f.calls(HardwareCall::STANDBY));
    f.tick(100000); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls);
}
void test_repeated_reboot_commands_do_not_move_armed_deadline() {
    Fixture f(DeviceRole::GATEWAY); f.start(); f.send("reboot\n"); f.tick(50000); f.send("reboot\r\nreboot\n");
    f.tick(99999); TEST_ASSERT_EQUAL_UINT(0, hostReset().calls); f.tick(100000); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls);
    TEST_ASSERT_EQUAL_UINT64(1, f.calls(HardwareCall::STANDBY));
}
void test_reset_deadline_before_at_and_after_expiration() {
    for (uint64_t elapsed : {uint64_t(99999), uint64_t(100000), uint64_t(100001)}) {
        Fixture f; f.start(); f.send("reboot\n"); f.tick(elapsed);
        TEST_ASSERT_EQUAL_UINT(elapsed >= 100000 ? 1 : 0, hostReset().calls);
    }
}
void test_reset_deadline_crosses_uint32_micros_wrap() {
    Fixture f; f.now = uint64_t(UINT32_MAX) - 50000; f.start(); const auto start = f.now;
    f.send("reboot\n"); f.tick(start + 99999); TEST_ASSERT_EQUAL_UINT(0, hostReset().calls);
    f.tick(start + 100000); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls);
}
void test_reboot_pending_blocks_new_client_transactions() {
    Fixture f(DeviceRole::CLIENT); f.start(); f.send("reboot\n");
    f.tick(1000000); TEST_ASSERT_EQUAL_UINT64(0, f.calls(HardwareCall::START_TX)); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls);
}
void test_due_app_update_precedes_same_loop_reboot_command() {
    Fixture f(DeviceRole::CLIENT); f.start(); TEST_ASSERT_TRUE(Serial.inject("reboot\n", 7));
    f.tick(1000000); TEST_ASSERT_EQUAL_UINT64(1, f.calls(HardwareCall::START_TX));
    TEST_ASSERT_EQUAL_UINT(0, hostReset().calls); f.txDone(1000001); f.tick(1100001); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls);
}
void test_reset_does_not_wait_for_unavailable_serial_output() {
    Fixture f; Serial.capacity = 0; f.start(false); f.send("reboot\n", false);
    f.tick(100000); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls); TEST_ASSERT_TRUE(Serial.output.empty()); TEST_ASSERT_EQUAL_UINT(0, Serial.calls);
}
void test_partial_serial_write_can_leave_boot_log_undrained_at_reset() {
    Fixture f; Serial.capacity = 1; Serial.writeLimit = 1; f.start(false); f.send("reboot\n", false);
    TEST_ASSERT_EQUAL_UINT(1, Serial.output.size()); f.tick(100000);
    TEST_ASSERT_EQUAL_UINT(1, hostReset().calls); TEST_ASSERT_EQUAL_UINT(2, Serial.output.size()); TEST_ASSERT_EQUAL_UINT(1, Serial.largestWrite);
}
void test_logging_queue_drop_is_bounded_and_status_reports_it() {
    Fixture f; f.start(); f.clear(); std::string burst;
    for (unsigned i = 0; i < 14; ++i) burst += "help\n";
    f.send(burst); f.clear(); f.send("status\n"); contains("log_dropped="); absent("log_dropped=0");
    TEST_ASSERT_TRUE(Serial.largestWrite <= 128); TEST_ASSERT_EQUAL_UINT(0, hostReset().calls); f.noRadio();
}
void test_zero_random_values_are_rejected_by_actual_boot_generator() {
    Fixture f(DeviceRole::CLIENT); f.random({0, 0, 401}); f.start();
    TEST_ASSERT_EQUAL_UINT(3, hostRandom().calls); f.tick(1000000); TEST_ASSERT_EQUAL_UINT32(401, f.transmitted().bootId);
}
void test_gateway_runtime_generator_rotates_session_on_sequence_exhaustion() {
    Fixture f(DeviceRole::GATEWAY); f.random({100, 0, 100, 200}); f.start();
    for (uint32_t i = 0; i <= 65536; ++i) {
        f.clear(); const auto request = hello(static_cast<uint16_t>(i), 730 + i / 65536);
        f.rx(request, uint64_t(i) * 2 + 1); const auto response = f.transmitted();
        TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(i), response.sequence);
        TEST_ASSERT_EQUAL_UINT32(i < 65536 ? 100 : 200, response.bootId); TEST_ASSERT_TRUE(ackMatches(response, request, 1));
        f.txDone(uint64_t(i) * 2 + 2); f.flush();
    }
    TEST_ASSERT_EQUAL_UINT(4, hostRandom().calls); TEST_ASSERT_EQUAL_UINT(1, f.initObservations);
    TEST_ASSERT_EQUAL_UINT(1, f.storage.begins); TEST_ASSERT_EQUAL_UINT64(65537, f.calls(HardwareCall::START_TX));
}
void test_simulated_reboot_loads_saved_role_and_new_session() {
    Fixture f(DeviceRole::CLIENT); f.start(); f.tick(1000000); const auto prior = f.transmitted();
    f.send("config set role gateway\nconfig set node-id 1\nconfig save\nreboot\n");
    f.txDone(1000010); f.tick(1100010); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls);
    f.runtime.emplace(f.storage, f.hardware); f.now = 0; f.clear(); f.start();
    contains("mode=GATEWAY,radio=ready"); f.rx(hello(), 1); const auto next = f.transmitted();
    TEST_ASSERT_EQUAL_UINT32(200, next.bootId); TEST_ASSERT_NOT_EQUAL(prior.bootId, next.bootId); TEST_ASSERT_EQUAL_UINT16(0, next.sequence);
    TEST_ASSERT_EQUAL_UINT(2, f.initObservations); TEST_ASSERT_EQUAL_UINT(2, Serial.beginCalls);
}
void test_status_preserves_reset_reason_version_and_unset_diagnostics() {
    Fixture f; hostReset().reason = 11; f.start();
    contains("fw=0.2.0-alpha.1,protocol=1"); contains("reset_reason=11"); contains("chip=N/A");
    contains("scope=inactive_defaults,role=UNSET"); contains("rssi_dbm=N/A,snr_db=N/A"); f.noRadio();
}
}  // namespace
void setUp() {}
void tearDown() { TEST_ASSERT_FALSE(hostRandom().exhausted); }
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_gateway_boot_order_and_exclusive_activation);
    RUN_TEST(test_client_boot_schedules_first_hello_after_one_second);
    RUN_TEST(test_missing_config_provisioning_poll_has_no_hardware_access);
    RUN_TEST(test_corrupt_config_boot_fails_safe);
    RUN_TEST(test_unsupported_schema_boot_fails_safe);
    RUN_TEST(test_invalid_role_boot_fails_safe);
    RUN_TEST(test_invalid_network_and_device_ids_boot_fail_safe);
    RUN_TEST(test_nvs_open_failure_boot_fails_safe);
    RUN_TEST(test_short_read_boot_fails_safe);
    RUN_TEST(test_radio_init_failure_never_constructs_either_app);
    RUN_TEST(test_header_or_crc_init_failure_prevents_activation);
    RUN_TEST(test_provision_gateway_save_is_inactive_until_new_runtime_boot);
    RUN_TEST(test_provision_client_save_is_inactive_until_new_runtime_boot);
    RUN_TEST(test_client_staging_and_save_preserve_active_role_and_ids);
    RUN_TEST(test_gateway_staging_client_preserves_active_gateway);
    RUN_TEST(test_invalid_config_save_rejected_without_storage_access);
    RUN_TEST(test_storage_write_failure_dispatches_exact_error);
    RUN_TEST(test_storage_readback_failure_does_not_activate_saved_role);
    RUN_TEST(test_valid_different_readback_reports_verify_error);
    RUN_TEST(test_serial_partial_command_spans_loops);
    RUN_TEST(test_serial_input_budget_is_exactly_64_bytes);
    RUN_TEST(test_multiple_serial_commands_cr_lf_and_help_dispatch);
    RUN_TEST(test_crlf_save_dispatches_only_once);
    RUN_TEST(test_invalid_command_and_number_do_not_mutate_or_reboot);
    RUN_TEST(test_control_byte_line_cannot_execute_embedded_reboot);
    RUN_TEST(test_overflow_line_recovery_keeps_following_status);
    RUN_TEST(test_gateway_receives_hello_and_only_gateway_sends_ack);
    RUN_TEST(test_client_receives_matching_ack_and_only_client_accepts_it);
    RUN_TEST(test_gateway_receive_start_fault_is_reported_then_recovers);
    RUN_TEST(test_runtime_dispatches_receive_integrity_error);
    RUN_TEST(test_idle_provisioning_reboot_stop_is_hardware_noop);
    RUN_TEST(test_reboot_while_receiving_stops_once);
    RUN_TEST(test_reboot_waits_for_actual_tx_done_before_stop);
    RUN_TEST(test_reboot_waits_for_driver_tx_timeout_and_then_stops);
    RUN_TEST(test_reboot_stop_failure_logs_and_still_requests_reset);
    RUN_TEST(test_repeated_reboot_commands_do_not_move_armed_deadline);
    RUN_TEST(test_reset_deadline_before_at_and_after_expiration);
    RUN_TEST(test_reset_deadline_crosses_uint32_micros_wrap);
    RUN_TEST(test_reboot_pending_blocks_new_client_transactions);
    RUN_TEST(test_due_app_update_precedes_same_loop_reboot_command);
    RUN_TEST(test_reset_does_not_wait_for_unavailable_serial_output);
    RUN_TEST(test_partial_serial_write_can_leave_boot_log_undrained_at_reset);
    RUN_TEST(test_logging_queue_drop_is_bounded_and_status_reports_it);
    RUN_TEST(test_zero_random_values_are_rejected_by_actual_boot_generator);
    RUN_TEST(test_gateway_runtime_generator_rotates_session_on_sequence_exhaustion);
    RUN_TEST(test_simulated_reboot_loads_saved_role_and_new_session);
    RUN_TEST(test_status_preserves_reset_reason_version_and_unset_diagnostics);
    return UNITY_END();
}
