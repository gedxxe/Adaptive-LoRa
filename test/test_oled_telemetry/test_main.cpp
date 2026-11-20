#include <unity.h>
#include <Arduino.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <esp_random.h>
#include <cstring>
#include "app/firmware_runtime.h"
#include "../support/fake_config_storage.h"
#include "../support/fake_radio_hardware.h"
#include "../support/fake_radio_transport.h"

using namespace adaptive_lora;
using namespace adaptive_lora::test;

namespace {
DeviceConfig config(DeviceRole role) {
    DeviceConfig value;
    value.role = role;
    value.nodeId = role == DeviceRole::CLIENT ? 2 : 1;
    value.helloIntervalMs = 15000;
    return value;
}

uint32_t bootId(uint32_t previous) { return previous + 1; }

Frame helloFrame() {
    Frame hello;
    hello.networkId = 1;
    hello.sourceId = 2;
    hello.destinationId = 1;
    hello.bootId = 7;
    return hello;
}

Frame ackFrame(const Frame& hello) {
    Frame ack;
    ack.type = MessageType::ACK;
    ack.networkId = hello.networkId;
    ack.sourceId = hello.destinationId;
    ack.destinationId = hello.sourceId;
    ack.bootId = 8;
    setAckPayload(ack, hello);
    return ack;
}

RadioEvent receive(FakeRadioTransport& radio, const Frame& frame, bool metrics = true) {
    CapturedPacket packet;
    TEST_ASSERT_EQUAL(int(CodecResult::OK), int(encodeFrame(frame, packet.bytes, sizeof(packet.bytes), packet.length)));
    auto event = radio.deliver(packet);
    TEST_ASSERT_EQUAL(int(RadioEventType::RX_DONE), int(event.type));
    event.metrics = metrics;
    event.rssi = -82.5f;
    event.snr = 7.5f;
    return event;
}

void test_gateway_counts_hello_and_ack_events_separately() {
    FakeRadioTransport radio;
    SerialLogger log;
    GatewayApp gateway(config(DeviceRole::GATEWAY), radio, log, bootId);
    gateway.begin(0);
    TEST_ASSERT_FALSE(gateway.telemetry().hello.seen);

    host_clock::set(1234);
    gateway.update(1234, receive(radio, helloFrame()));
    auto state = gateway.telemetry();
    TEST_ASSERT_EQUAL_UINT32(1, state.uniqueHellos);
    TEST_ASSERT_EQUAL_UINT32(1, state.ackStarts);
    TEST_ASSERT_EQUAL_UINT32(0, state.ackCompletions);
    TEST_ASSERT_EQUAL_UINT32(1234, state.hello.atUs);
    TEST_ASSERT_EQUAL_UINT16(2, state.hello.peer);
    TEST_ASSERT_TRUE(state.hello.metrics);
    TEST_ASSERT_TRUE(state.hello.rssi == -82.5f && state.hello.snr == 7.5f);

    gateway.update(1235, radio.completeTx());
    TEST_ASSERT_EQUAL_UINT32(1, gateway.telemetry().ackCompletions);
    host_clock::set(1236);
    gateway.update(1236, receive(radio, helloFrame(), false));
    gateway.update(1237, radio.error(RadioEventType::ERROR, -1));
    state = gateway.telemetry();
    TEST_ASSERT_EQUAL_UINT32(1, state.uniqueHellos);
    TEST_ASSERT_EQUAL_UINT32(1, state.duplicates);
    TEST_ASSERT_EQUAL_UINT32(2, state.ackStarts);
    TEST_ASSERT_EQUAL_UINT32(1, state.ackCompletions);
    TEST_ASSERT_EQUAL_UINT32(1, state.radioErrors);
    TEST_ASSERT_EQUAL_UINT32(1236, state.hello.atUs);
    TEST_ASSERT_FALSE(state.hello.metrics);
}

void test_gateway_failed_ack_start_is_not_a_start_or_completion() {
    FakeRadioTransport radio;
    SerialLogger log;
    GatewayApp gateway(config(DeviceRole::GATEWAY), radio, log, bootId);
    gateway.begin(0);
    radio.failNextTx = true;
    gateway.update(10, receive(radio, helloFrame()));
    const auto state = gateway.telemetry();
    TEST_ASSERT_EQUAL_UINT32(1, state.uniqueHellos);
    TEST_ASSERT_EQUAL_UINT32(1, state.radioErrors);
    TEST_ASSERT_EQUAL_UINT32(0, state.ackStarts);
    TEST_ASSERT_EQUAL_UINT32(0, state.ackCompletions);
}

void test_client_only_updates_metrics_on_accepted_ack_and_keeps_accounting() {
    FakeRadioTransport radio;
    SerialLogger log;
    ClientApp client(config(DeviceRole::CLIENT), radio, log, bootId);
    client.begin(0);
    TEST_ASSERT_FALSE(client.telemetry().ack.seen);
    host_clock::set(1000000);
    client.update(1000000, {});
    client.update(1000001, radio.completeTx());
    TEST_ASSERT_TRUE(client.awaitingAck());
    Frame hello;
    TEST_ASSERT_EQUAL(int(CodecResult::OK), int(decodeFrame(radio.lastTx.bytes, radio.lastTx.length, 1, 1, hello)));
    Frame ack = ackFrame(hello);
    Frame wrong = ack;
    wrong.payload[6] ^= 1;
    client.update(1000002, receive(radio, wrong));
    TEST_ASSERT_FALSE(client.telemetry().ack.seen);
    host_clock::set(1000003);
    client.update(1000003, receive(radio, ack));
    TEST_ASSERT_FALSE(client.awaitingAck());
    TEST_ASSERT_TRUE(client.telemetry().ack.metrics);
    TEST_ASSERT_TRUE(client.telemetry().ack.rssi == -82.5f && client.telemetry().ack.snr == 7.5f);
    host_clock::set(16000003);
    client.update(16000003, {});
    client.update(16000004, radio.completeTx());
    host_clock::set(26000004);
    client.update(26000004, {});
    host_clock::set(41000004);
    radio.failNextTx = true;
    client.update(41000004, {});
    const auto state = client.telemetry();
    TEST_ASSERT_EQUAL_UINT32(3, state.attempts);
    TEST_ASSERT_EQUAL_UINT32(1, state.successes);
    TEST_ASSERT_EQUAL_UINT32(1, state.timeouts);
    TEST_ASSERT_EQUAL_UINT32(1, state.txFailures);
    TEST_ASSERT_EQUAL_UINT32(1000003, state.ack.atUs);
}

void test_freshness_exact_boundary_across_32bit_wrap() {
    LastReception sample;
    constexpr uint64_t received = 0xFFFFFFF0;
    TEST_ASSERT_EQUAL(int(LinkFreshness::WAIT), int(sample.freshness(received, 15000)));
    sample = {true, false, 1, received, 0, 0};
    TEST_ASSERT_EQUAL(int(LinkFreshness::ONLINE), int(sample.freshness(received, 15000)));
    TEST_ASSERT_EQUAL(int(LinkFreshness::ONLINE), int(sample.freshness(received + 39999999, 15000)));
    TEST_ASSERT_EQUAL(int(LinkFreshness::STALE), int(sample.freshness(received + 40000000, 15000)));
}

void test_format_provisioning_unknown_and_maximum_counters() {
    DisplaySnapshot snapshot;
    snapshot.configError = "nvs_missing_or_unreadable";
    auto screen = formatOledStatus(snapshot, 0);
    TEST_ASSERT_EQUAL_STRING("PROVISIONING", screen.rows[1]);
    TEST_ASSERT_EQUAL_STRING("CONFIG INVALID", screen.rows[2]);
    snapshot.configError = nullptr;
    snapshot.config = config(DeviceRole::CLIENT);
    snapshot.config.nodeId = 65534;
    snapshot.config.gatewayId = 65533;
    snapshot.radioReady = true;
    snapshot.radioState = "RX";
    snapshot.logDropped = UINT32_MAX;
    screen = formatOledStatus(snapshot, 0);
    TEST_ASSERT_EQUAL_STRING("CLIENT 65534 GW 65533", screen.rows[1]);
    TEST_ASSERT_EQUAL_STRING("WAIT PEER N/A", screen.rows[3]);
    TEST_ASSERT_EQUAL_STRING("RSSI N/A", screen.rows[4]);
    TEST_ASSERT_EQUAL_STRING("SNR N/A", screen.rows[5]);
    TEST_ASSERT_EQUAL_STRING("AGE N/A", screen.rows[6]);
    TEST_ASSERT_EQUAL_STRING("LOG DROP 4294967295", screen.rows[7]);
    snapshot.config = config(DeviceRole::GATEWAY);
    snapshot.gateway.hello = {true, true, 2, 0, -82.5f, 0};
    snapshot.gateway.ackStarts = UINT32_MAX;
    screen = formatOledStatus(snapshot, 0);
    TEST_ASSERT_EQUAL_STRING("ONLINE PEER 2", screen.rows[3]);
    TEST_ASSERT_EQUAL_STRING("RSSI -82.5 DBM", screen.rows[4]);
    TEST_ASSERT_EQUAL_STRING("SNR 0.0 DB", screen.rows[5]);
    screen = formatOledStatus(snapshot, 5000000);
    TEST_ASSERT_EQUAL_STRING("ACK START 4294967295", screen.rows[5]);
    TEST_ASSERT_EQUAL_STRING("ACK DONE 0", screen.rows[6]);
}

void service(OledStatusDisplay& display, uint64_t now) {
    host_clock::set(now);
    display.service(now);
}

void initialize(OledStatusDisplay& display) {
    display.start(0);
    service(display, 99999);
    TEST_ASSERT_EQUAL_INT(5, hostOledGpio().calls);
    service(display, 100000);
    service(display, 109999);
    TEST_ASSERT_EQUAL_INT(-1, Wire.sda);
    service(display, 110000);
    service(display, 110001);
    service(display, 110002);
}

void test_oled_power_reset_then_bus_and_bounded_changed_pages() {
    OledStatusDisplay display;
    Wire.elapsedUs = 120;
    initialize(display);
    TEST_ASSERT_EQUAL_INT(36, hostOledGpio().pins[0]);
    TEST_ASSERT_EQUAL_INT(1, hostOledGpio().values[0]);
    TEST_ASSERT_EQUAL_INT(21, hostOledGpio().pins[2]);
    TEST_ASSERT_EQUAL_INT(0, hostOledGpio().values[2]);
    TEST_ASSERT_EQUAL_INT(0, hostOledGpio().values[4]);
    TEST_ASSERT_EQUAL_INT(1, hostOledGpio().values[5]);
    TEST_ASSERT_EQUAL_INT(17, Wire.sda);
    TEST_ASSERT_EQUAL_INT(18, Wire.scl);
    TEST_ASSERT_EQUAL_UINT32(400000, Wire.frequency);
    TEST_ASSERT_EQUAL_UINT16(3, Wire.timeout);
    TEST_ASSERT_EQUAL_HEX8(0x3C, Wire.address);
    TEST_ASSERT_EQUAL_HEX8(0xAE, Wire.bytes[1]);
    DisplaySnapshot snapshot;
    snapshot.configError = "invalid";
    TEST_ASSERT_TRUE(display.needsSnapshot(110200));
    display.render(snapshot, 110200);
    TEST_ASSERT_FALSE(display.ready());
    for (unsigned i = 0; i < 49; ++i) {
        const auto before = Wire.writes;
        service(display, 111000 + i * 130);
        TEST_ASSERT_EQUAL_UINT(before + 1, Wire.writes);
        TEST_ASSERT_FALSE(display.needsSnapshot(200000));
    }
    TEST_ASSERT_TRUE(display.ready());
    TEST_ASSERT_EQUAL_UINT(31, Wire.largest);
    TEST_ASSERT_EQUAL_HEX8(0xAF, Wire.bytes[1]);
    TEST_ASSERT_EQUAL_UINT32(120, display.worstIoUs());
    TEST_ASSERT_EQUAL_UINT32(49 * 120, display.worstRefreshUs());
    TEST_ASSERT_TRUE(display.needsSnapshot(1110200));
    display.render(snapshot, 1110200);
    service(display, 1110200);
    TEST_ASSERT_EQUAL_UINT(50, Wire.writes);
}

void test_display_mid_frame_error_stops_future_bus_work() {
    OledStatusDisplay display;
    initialize(display);
    display.render({}, 110200);
    Wire.failure = 5;
    service(display, 111000);
    TEST_ASSERT_EQUAL_STRING("i2c_transfer", display.error());
    Wire.failure = 0;
    service(display, 2000000);
    TEST_ASSERT_EQUAL_UINT(2, Wire.writes);
}

void runClientWithDisplay(bool enabled, bool fail, CapturedPacket& packet) {
    FakeConfigStorage storage;
    auto cfg = config(DeviceRole::CLIENT);
    storage.exists = true;
    storage.length = kConfigBlobSize;
    TEST_ASSERT_TRUE(encodeConfig(cfg, storage.bytes, sizeof(storage.bytes)));
    FakeRadioHardware radio;
    OledStatusDisplay display;
    FirmwareRuntime runtime(storage, radio, enabled ? &display : nullptr);
    Wire.beginOk = !fail;
    runtime.setup();
    for (uint64_t ms = 1; ms <= 115; ++ms) {
        host_clock::set(ms * 1000);
        radio.resetTrace();
        runtime.loop();
    }
    if (enabled && fail) TEST_ASSERT_EQUAL_STRING("i2c_begin", display.error());
    host_clock::set(1000000);
    auto writes = Wire.writes;
    runtime.loop();
    TEST_ASSERT_EQUAL_UINT(kHeaderSize, radio.txLength);
    packet.length = radio.txLength;
    std::memcpy(packet.bytes, radio.txBytes, packet.length);
    TEST_ASSERT_EQUAL_UINT(writes, Wire.writes);
    radio.signal(radio_irq::txDone);
    host_clock::set(1070000);
    runtime.loop();
    TEST_ASSERT_EQUAL_UINT(writes, Wire.writes);
    radio.signal(0, false);
    host_clock::set(1300000);
    runtime.loop();
    TEST_ASSERT_EQUAL_UINT(writes, Wire.writes);
    Frame hello;
    TEST_ASSERT_EQUAL(int(CodecResult::OK), int(decodeFrame(packet.bytes, packet.length, 1, 1, hello)));
    TEST_ASSERT_EQUAL(int(CodecResult::OK), int(encodeFrame(ackFrame(hello), radio.rxBytes,
                                                        sizeof(radio.rxBytes), radio.packetLength)));
    radio.signal(radio_irq::rxDone);
    host_clock::set(1370000);
    runtime.loop();
    radio.signal(0, false);
    for (unsigned i = 0; i < 40; ++i) runtime.loop();
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("event=ACK_RX,"));
    if (enabled && fail) TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("result=i2c_begin"));
}

void resetOledFixture() {
    Wire = HostWire{};
    hostOledGpio() = HostOledGpio{};
    Serial.reset();
    hostRandom() = HostRandom{};
    host_clock::set(0);
}

void test_runtime_oled_off_on_and_failure_preserve_hello_ack() {
    CapturedPacket off, on, failed;
    runClientWithDisplay(false, false, off);
    resetOledFixture();
    runClientWithDisplay(true, false, on);
    resetOledFixture();
    runClientWithDisplay(true, true, failed);
    TEST_ASSERT_EQUAL_UINT(off.length, on.length);
    TEST_ASSERT_EQUAL_UINT(off.length, failed.length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(off.bytes, on.bytes, off.length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(off.bytes, failed.bytes, off.length);
}

void test_runtime_provisioning_initializes_oled_without_radio_calls() {
    FakeConfigStorage storage;
    FakeRadioHardware radio;
    OledStatusDisplay display;
    FirmwareRuntime runtime(storage, radio, &display);
    runtime.setup();
    for (uint64_t ms = 1; ms < 200; ++ms) {
        host_clock::set(ms * 1000);
        runtime.loop();
    }
    TEST_ASSERT_TRUE(display.ready());
    TEST_ASSERT_EQUAL_UINT(0, radio.traceLength);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("mode=PROVISIONING"));
}
}  // namespace

void setUp() { resetOledFixture(); }
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_gateway_counts_hello_and_ack_events_separately);
    RUN_TEST(test_gateway_failed_ack_start_is_not_a_start_or_completion);
    RUN_TEST(test_client_only_updates_metrics_on_accepted_ack_and_keeps_accounting);
    RUN_TEST(test_freshness_exact_boundary_across_32bit_wrap);
    RUN_TEST(test_format_provisioning_unknown_and_maximum_counters);
    RUN_TEST(test_oled_power_reset_then_bus_and_bounded_changed_pages);
    RUN_TEST(test_display_mid_frame_error_stops_future_bus_work);
    RUN_TEST(test_runtime_oled_off_on_and_failure_preserve_hello_ack);
    RUN_TEST(test_runtime_provisioning_initializes_oled_without_radio_calls);
    return UNITY_END();
}
