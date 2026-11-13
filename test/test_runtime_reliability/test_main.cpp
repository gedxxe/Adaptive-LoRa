#include <unity.h>
#include <Arduino.h>
#include <esp_system.h>
#include <esp_random.h>
#include <cstring>
#include <string>
#include "app/firmware_runtime.h"
#include "../support/fake_radio_hardware.h"
#include "../support/fake_config_storage.h"

using namespace adaptive_lora;
using namespace adaptive_lora::test;
namespace {
struct Fixture {
    FakeConfigStorage storage;
    FakeRadioHardware hardware;
    FirmwareRuntime runtime{storage, hardware};
    uint64_t totals[static_cast<size_t>(HardwareCall::COUNT)]{};
    explicit Fixture(DeviceRole role = DeviceRole::CLIENT) {
        Serial.reset(); host_clock::set(0); hostReset() = HostReset{}; hostRandom() = HostRandom{};
        hostRandom().values[0] = 100; hostRandom().values[1] = 200; hostRandom().length = 2; hostRandom().guardAfter = 64;
        DeviceConfig c; c.role = role; c.nodeId = role == DeviceRole::GATEWAY ? 1 : 2; c.helloIntervalMs = 1000;
        TEST_ASSERT_TRUE(encodeConfig(c, storage.bytes, sizeof(storage.bytes))); storage.length = kConfigBlobSize;
    }
    uint64_t now() { return static_cast<uint64_t>(host_clock::value()); }
    void collect() {
        TEST_ASSERT_FALSE(hardware.violation); TEST_ASSERT_FALSE(storage.violation); TEST_ASSERT_FALSE(storage.active);
        TEST_ASSERT_EQUAL_UINT(storage.successfulBegins, storage.ends); TEST_ASSERT_FALSE(hostRandom().exhausted);
        for (size_t i = 0; i < static_cast<size_t>(HardwareCall::COUNT); ++i) totals[i] += hardware.calls[i];
        hardware.resetTrace(); hardware.signal(0, false);
    }
    void step() { runtime.loop(); collect(); }
    void at(uint64_t time) { host_clock::set(time); step(); }
    void drain() {
        for (unsigned i = 0; i < 256; ++i) { const auto before = Serial.output.size(); step(); if (Serial.output.size() == before) return; }
        TEST_FAIL_MESSAGE("Diagnostic fixture budget exhausted");
    }
    void start() {
        bool guard = false;
        try { runtime.setup(); } catch (const HostRandomLimit&) { guard = true; }
        TEST_ASSERT_FALSE_MESSAGE(guard, "Production generator exceeded finite native guard64"); collect(); drain();
    }
    void command(const char* text) { TEST_ASSERT_TRUE(Serial.inject(text, std::strlen(text))); while (Serial.available()) step(); }
    void cyclic(uint32_t first, uint32_t second, size_t length = 1) {
        hostRandom() = HostRandom{}; auto& r = hostRandom(); r.values[0] = first; r.values[1] = second;
        r.length = length; r.cycle = true; r.guardAfter = 64;
    }
    uint64_t txCount() { return totals[static_cast<size_t>(HardwareCall::START_TX)]; }
    Frame tx() {
        Frame f; TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(decodeFrame(hardware.txBytes, hardware.txLength,
            get16(hardware.txBytes + 6), get16(hardware.txBytes + 10), f))); TEST_ASSERT_NOT_EQUAL(0, f.bootId); return f;
    }
    void done(uint64_t time) { hardware.signal(radio_irq::txDone); at(time); }
    void rx(const Frame& f, uint64_t time) {
        TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(encodeFrame(f, hardware.rxBytes, sizeof(hardware.rxBytes), hardware.packetLength)));
        hardware.signal(radio_irq::rxDone); at(time);
    }
};
void has(const char* text) { TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find(text)); }
void lacks(const char* text) { TEST_ASSERT_EQUAL(std::string::npos, Serial.output.find(text)); }
Frame response(const Frame& request) {
    Frame f; f.type = MessageType::ACK; f.sourceId = 1; f.destinationId = 2; f.networkId = 1; f.bootId = 500; setAckPayload(f, request); return f;
}
Frame hello(uint16_t sequence = 0, uint32_t boot = 700) {
    Frame f; f.sourceId = 2; f.destinationId = 1; f.networkId = 1; f.bootId = boot; f.sequence = sequence; return f;
}
void test_permanent_zero_initial_client_is_bounded_and_diagnosable() {
    Fixture f; f.cyclic(0, 0); f.start(); TEST_ASSERT_EQUAL_UINT(8, hostRandom().calls);
    has("event=SESSION_ERROR"); f.command("help\nstatus\nconfig show\n"); f.drain(); has("event=STATUS");
    f.at(999999); TEST_ASSERT_EQUAL_UINT(8, hostRandom().calls); TEST_ASSERT_EQUAL_UINT64(0, f.txCount());
}
void test_permanent_zero_initial_gateway_suppresses_ack() {
    Fixture f(DeviceRole::GATEWAY); f.cyclic(0, 0); f.start(); f.rx(hello(), 1); f.drain();
    TEST_ASSERT_EQUAL_UINT(8, hostRandom().calls); TEST_ASSERT_EQUAL_UINT64(0, f.txCount()); has("event=SESSION_ERROR");
}
void test_delayed_rx_cannot_accept_ack_after_deadline() {
    Fixture f; f.start(); f.at(1000000); const auto request = f.tx(); f.done(1000010); Serial.output.clear();
    f.hardware.delayUs[static_cast<size_t>(HardwareCall::READ)] = 20;
    f.rx(response(request), 2000000); f.drain(); TEST_ASSERT_EQUAL_UINT64(2000020, f.now());
    lacks("result=accepted"); has("event=ACK_TIMEOUT"); has("ack_identity_or_state_mismatch");
}
void test_tx_done_processing_delay_anchors_ack_window_after_poll() {
    Fixture f; f.start(); f.at(1000000); const auto request = f.tx();
    f.hardware.delayUs[static_cast<size_t>(HardwareCall::FINISH_TX)] = 20;
    f.done(2999990); TEST_ASSERT_EQUAL_UINT64(3000010, f.now());
    f.rx(response(request), 3999995); f.drain(); has("result=accepted");
}
void test_slow_config_save_preserves_full_post_stop_reboot_interval() {
    Fixture f; f.start(); f.storage.delayUs[static_cast<size_t>(StorageCall::WRITE)] = 200000;
    f.command("config save\nreboot\n"); TEST_ASSERT_EQUAL_UINT64(200000, f.now());
    f.step(); TEST_ASSERT_EQUAL_UINT(0, hostReset().calls);
    f.at(299999); TEST_ASSERT_EQUAL_UINT(0, hostReset().calls); f.at(300000); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls);
}
void test_slow_stop_preserves_full_post_stop_reboot_interval() {
    Fixture f(DeviceRole::GATEWAY); f.start(); f.hardware.delayUs[static_cast<size_t>(HardwareCall::STANDBY)] = 200000;
    f.command("reboot\n"); f.step(); TEST_ASSERT_EQUAL_UINT(0, hostReset().calls);
    f.at(299999); TEST_ASSERT_EQUAL_UINT(0, hostReset().calls); f.at(300000); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls);
}
void test_reset_evaluates_fresh_time_after_blocked_serial_write() {
    Fixture f; f.start(); Serial.writeDelayUs = 150000; f.command("reboot\n");
    TEST_ASSERT_EQUAL_UINT64(150000, f.now()); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls);
}
void test_reset_evaluates_fresh_time_after_unavailable_output_wait() {
    Fixture f; f.start(); Serial.capacity = 0; Serial.availableDelayUs = 150000; f.command("reboot\n");
    TEST_ASSERT_EQUAL_UINT64(150000, f.now()); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls);
}
void test_save_and_reboot_deadline_remain_correct_across_wrap() {
    Fixture f(DeviceRole::GATEWAY); f.start(); const uint64_t begin = uint64_t(UINT32_MAX) - 50000; host_clock::set(begin);
    f.storage.delayUs[static_cast<size_t>(StorageCall::WRITE)] = 200000; f.command("config save\nreboot\n");
    f.at(begin + 299999); TEST_ASSERT_EQUAL_UINT(0, hostReset().calls);
    f.at(begin + 300000); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls);
}
void test_delayed_failed_rx_retains_existing_attempt_start_recovery_policy() {
    Fixture f(DeviceRole::GATEWAY); f.hardware.startRxCode = -701;
    f.hardware.delayUs[static_cast<size_t>(HardwareCall::START_RX)] = 200000; f.start();
    f.at(999999); TEST_ASSERT_EQUAL_UINT64(1, f.totals[static_cast<size_t>(HardwareCall::START_RX)]);
    f.hardware.startRxCode = 0; f.at(1000000); TEST_ASSERT_EQUAL_UINT64(1200000, f.now());
    TEST_ASSERT_EQUAL_UINT64(2, f.totals[static_cast<size_t>(HardwareCall::START_RX)]);
}
void test_valid_max_id_after_zero_prefix() {
    Fixture f; auto& r = hostRandom(); r.values[0] = 0; r.values[1] = 0; r.values[2] = UINT32_MAX; r.length = 3;
    f.start(); TEST_ASSERT_EQUAL_UINT(3, r.calls); f.at(1000000); TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, f.tx().bootId);
}
void test_last_attempt_and_next_batch_boundary() {
    for (unsigned validAt : {7u, 8u}) {
        Fixture f; auto& r = hostRandom(); std::memset(r.values, 0, sizeof(r.values)); r.values[validAt] = 321; r.length = validAt + 1;
        f.start(); TEST_ASSERT_EQUAL_UINT(8, r.calls); TEST_ASSERT_EQUAL_UINT64(0, f.txCount());
        f.at(999999); TEST_ASSERT_EQUAL_UINT(8, r.calls); f.at(1000000);
        TEST_ASSERT_EQUAL_UINT(validAt == 7 ? 8 : 9, r.calls); TEST_ASSERT_EQUAL_UINT32(321, f.tx().bootId);
    }
}
void test_failed_client_retries_once_per_second_then_recovers() {
    Fixture f; f.cyclic(0, 0); f.start(); f.at(1000000); TEST_ASSERT_EQUAL_UINT(16, hostRandom().calls);
    f.step(); f.at(1999999); TEST_ASSERT_EQUAL_UINT(16, hostRandom().calls); TEST_ASSERT_EQUAL_UINT64(0, f.txCount());
    auto& r = hostRandom(); r.values[0] = 333; r.at = 0; r.cycle = false; f.at(2000000);
    TEST_ASSERT_EQUAL_UINT(17, r.calls); TEST_ASSERT_EQUAL_UINT32(333, f.tx().bootId); TEST_ASSERT_EQUAL_UINT16(0, f.tx().sequence);
    f.drain(); has("result=identity_recovered"); TEST_ASSERT_EQUAL_UINT(1, f.storage.begins);
}
void test_failed_gateway_recovers_without_reinitializing_services() {
    Fixture f(DeviceRole::GATEWAY); f.cyclic(0, 0); f.start(); f.rx(hello(), 1); f.at(999999);
    TEST_ASSERT_EQUAL_UINT64(0, f.txCount()); auto& r = hostRandom(); r.values[0] = 300; r.at = 0; r.cycle = false;
    f.at(1000000); f.rx(hello(), 1000001); const auto packet = f.tx();
    TEST_ASSERT_EQUAL_UINT32(300, packet.bootId); TEST_ASSERT_EQUAL_UINT16(0, packet.sequence); f.drain(); has("result=identity_recovered");
    TEST_ASSERT_EQUAL_UINT(1, f.storage.begins); TEST_ASSERT_EQUAL_UINT64(1, f.totals[static_cast<size_t>(HardwareCall::BEGIN)]);
}
void test_identity_recovery_is_suppressed_while_reboot_pending() {
    Fixture f; f.cyclic(0, 0); f.start(); f.command("reboot\n"); f.at(1000000);
    TEST_ASSERT_EQUAL_UINT(8, hostRandom().calls); TEST_ASSERT_EQUAL_UINT64(0, f.txCount()); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls);
}
void test_identity_retry_deadline_crosses_micros_wrap() {
    Fixture f; const uint64_t begin = uint64_t(UINT32_MAX) - 500000; host_clock::set(begin); f.cyclic(0, 0); f.start();
    f.at(begin + 999999); TEST_ASSERT_EQUAL_UINT(8, hostRandom().calls);
    f.at(begin + 1000000); TEST_ASSERT_EQUAL_UINT(16, hostRandom().calls); TEST_ASSERT_EQUAL_UINT64(0, f.txCount());
}
uint32_t replacement = 0, observedPrevious = 0;
uint32_t generateReplacement(uint32_t previous) { observedPrevious = previous; return replacement; }
void test_counter_renewal_preserves_exhausted_identity_on_invalid_result() {
    SequenceCounter counter; counter.begin(100); uint16_t value = 0;
    for (unsigned i = 0; i < 65536; ++i) TEST_ASSERT_TRUE(counter.take(value));
    for (auto invalid : {0u, 100u}) {
        replacement = invalid; TEST_ASSERT_FALSE(counter.renew(generateReplacement));
        TEST_ASSERT_EQUAL_UINT32(100, observedPrevious); TEST_ASSERT_EQUAL_UINT32(100, counter.bootId());
        value = 42; TEST_ASSERT_FALSE(counter.take(value)); TEST_ASSERT_EQUAL_UINT16(42, value);
    }
    replacement = 200; TEST_ASSERT_TRUE(counter.renew(generateReplacement)); TEST_ASSERT_EQUAL_UINT32(100, observedPrevious);
    TEST_ASSERT_TRUE(counter.take(value)); TEST_ASSERT_EQUAL_UINT16(0, value); TEST_ASSERT_EQUAL_UINT32(200, counter.bootId());
}
void exhaustGateway(Fixture& f) {
    for (uint32_t i = 0; i < 65536; ++i) {
        Serial.output.clear(); const auto time = f.now(); f.rx(hello(static_cast<uint16_t>(i)), time + 1);
        const auto packet = f.tx(); TEST_ASSERT_EQUAL_UINT16(i, packet.sequence); TEST_ASSERT_EQUAL_UINT32(100, packet.bootId);
        f.done(time + 2); f.drain();
    }
}
void test_gateway_rollover_zero_previous_and_alternating_fail_then_recover() {
    for (unsigned mode = 0; mode < 3; ++mode) {
        Fixture f(DeviceRole::GATEWAY); f.start(); exhaustGateway(f);
        f.cyclic(mode == 1 ? 100 : 0, 100, mode == 2 ? 2 : 1); const auto failedAt = f.now() + 1;
        Serial.output.clear(); f.rx(hello(0, 701), failedAt); f.drain();
        TEST_ASSERT_EQUAL_UINT(8, hostRandom().calls); TEST_ASSERT_EQUAL_UINT64(65536, f.txCount()); has("event=SESSION_ERROR");
        f.rx(hello(0, 701), failedAt + 999999); TEST_ASSERT_EQUAL_UINT(8, hostRandom().calls); TEST_ASSERT_EQUAL_UINT64(65536, f.txCount());
        auto& r = hostRandom(); r.values[0] = 0; r.values[1] = 100; r.values[2] = 200; r.length = 3; r.at = 0; r.cycle = false;
        f.at(failedAt + 1000000); f.rx(hello(0, 701), failedAt + 1000001); const auto packet = f.tx();
        TEST_ASSERT_EQUAL_UINT(11, r.calls); TEST_ASSERT_EQUAL_UINT32(200, packet.bootId); TEST_ASSERT_EQUAL_UINT16(0, packet.sequence);
        TEST_ASSERT_TRUE(ackMatches(packet, hello(0, 701), 1)); TEST_ASSERT_EQUAL_UINT(1, f.storage.begins);
        TEST_ASSERT_EQUAL_UINT64(65537, f.txCount());
    }
}
void test_client_rollover_failure_preserves_identity_until_recovery() {
    Fixture f; f.start(); uint64_t next = 1000000;
    for (uint32_t i = 0; i < 65536; ++i) {
        Serial.output.clear(); f.at(next); const auto packet = f.tx();
        TEST_ASSERT_EQUAL_UINT16(i, packet.sequence); TEST_ASSERT_EQUAL_UINT32(100, packet.bootId);
        f.done(next + 1); f.rx(response(packet), next + 2); f.drain(); next += 1000002;
    }
    f.cyclic(0, 0); f.at(next); f.drain(); has("event=SESSION_ERROR");
    TEST_ASSERT_EQUAL_UINT(8, hostRandom().calls); TEST_ASSERT_EQUAL_UINT64(65536, f.txCount());
    auto& r = hostRandom(); r.values[0] = 0; r.values[1] = 100; r.values[2] = 200; r.length = 3; r.at = 0; r.cycle = false;
    f.at(next + 1000000); const auto packet = f.tx(); TEST_ASSERT_EQUAL_UINT(11, r.calls);
    TEST_ASSERT_EQUAL_UINT32(200, packet.bootId); TEST_ASSERT_EQUAL_UINT16(0, packet.sequence); TEST_ASSERT_EQUAL_UINT64(65537, f.txCount());
    TEST_ASSERT_EQUAL_UINT(1, f.storage.begins);
}
void test_delayed_ack_rejection_across_uint32_wrap() {
    Fixture f; f.start(); const uint64_t start = uint64_t(UINT32_MAX) - 1000000;
    f.at(start); const auto request = f.tx(); f.done(start + 500000);
    f.hardware.delayUs[static_cast<size_t>(HardwareCall::READ)] = 20; Serial.output.clear();
    f.rx(response(request), start + 1499990); f.drain(); lacks("result=accepted"); has("event=ACK_TIMEOUT");
}
void test_serial_wait_defers_ack_timeout_service_until_next_loop() {
    Fixture f; f.start(); f.at(1000000); f.done(1000010); Serial.output.clear();
    host_clock::set(2000000); Serial.writeDelayUs = 20; f.command("status\n");
    TEST_ASSERT_EQUAL_UINT64(2000020, f.now()); lacks("event=ACK_TIMEOUT");
    Serial.writeDelayUs = 0; f.step(); f.drain(); has("event=ACK_TIMEOUT");
}
void test_save_delay_defers_ack_timeout_but_cannot_accept_late_ack() {
    Fixture f; f.start(); f.at(1000000); const auto packet = f.tx(); f.done(1000010); Serial.output.clear();
    host_clock::set(2000000); f.storage.delayUs[static_cast<size_t>(StorageCall::WRITE)] = 200000; f.command("config save\n");
    lacks("event=ACK_TIMEOUT"); f.rx(response(packet), f.now()); f.drain(); lacks("result=accepted"); has("event=ACK_TIMEOUT");
}
void test_tx_start_delay_remains_inside_existing_driver_deadline_budget() {
    Fixture f; f.start(); f.hardware.delayUs[static_cast<size_t>(HardwareCall::START_TX)] = 500000;
    f.at(1000000); TEST_ASSERT_EQUAL_UINT64(1500000, f.now()); f.at(2999999); TEST_ASSERT_EQUAL_UINT64(1, f.txCount());
    f.at(3000000); f.drain(); has("result=tx_failed"); TEST_ASSERT_EQUAL_UINT64(1, f.txCount());
}
void test_delayed_stop_failure_still_resets_after_full_interval() {
    Fixture f(DeviceRole::GATEWAY); f.start(); f.hardware.standbyCode = -701;
    f.hardware.delayUs[static_cast<size_t>(HardwareCall::STANDBY)] = 200000; f.command("reboot\n"); f.drain();
    has("result=standby_failed_restarting"); f.at(299999); TEST_ASSERT_EQUAL_UINT(0, hostReset().calls);
    f.at(300000); TEST_ASSERT_EQUAL_UINT(1, hostReset().calls);
}
}
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_permanent_zero_initial_client_is_bounded_and_diagnosable);
    RUN_TEST(test_permanent_zero_initial_gateway_suppresses_ack);
    RUN_TEST(test_delayed_rx_cannot_accept_ack_after_deadline);
    RUN_TEST(test_tx_done_processing_delay_anchors_ack_window_after_poll);
    RUN_TEST(test_slow_config_save_preserves_full_post_stop_reboot_interval);
    RUN_TEST(test_slow_stop_preserves_full_post_stop_reboot_interval);
    RUN_TEST(test_reset_evaluates_fresh_time_after_blocked_serial_write);
    RUN_TEST(test_reset_evaluates_fresh_time_after_unavailable_output_wait);
    RUN_TEST(test_save_and_reboot_deadline_remain_correct_across_wrap);
    RUN_TEST(test_delayed_failed_rx_retains_existing_attempt_start_recovery_policy);
    RUN_TEST(test_valid_max_id_after_zero_prefix);
    RUN_TEST(test_last_attempt_and_next_batch_boundary);
    RUN_TEST(test_failed_client_retries_once_per_second_then_recovers);
    RUN_TEST(test_failed_gateway_recovers_without_reinitializing_services);
    RUN_TEST(test_identity_recovery_is_suppressed_while_reboot_pending);
    RUN_TEST(test_identity_retry_deadline_crosses_micros_wrap);
    RUN_TEST(test_counter_renewal_preserves_exhausted_identity_on_invalid_result);
    RUN_TEST(test_gateway_rollover_zero_previous_and_alternating_fail_then_recover);
    RUN_TEST(test_client_rollover_failure_preserves_identity_until_recovery);
    RUN_TEST(test_delayed_ack_rejection_across_uint32_wrap);
    RUN_TEST(test_serial_wait_defers_ack_timeout_service_until_next_loop);
    RUN_TEST(test_save_delay_defers_ack_timeout_but_cannot_accept_late_ack);
    RUN_TEST(test_tx_start_delay_remains_inside_existing_driver_deadline_budget);
    RUN_TEST(test_delayed_stop_failure_still_resets_after_full_interval);
    return UNITY_END();
}
