#include <unity.h>
#include <Arduino.h>
#include <esp_random.h>
#if defined(ADAPTIVE_LORA_OLED)
#include <Wire.h>
#include <driver/gpio.h>
#endif
#include "app/firmware_runtime.h"
#include "research_roster.h"
#include "../support/fake_radio_hardware.h"
#include "../support/fake_config_storage.h"

using namespace adaptive_lora;
using namespace adaptive_lora::multiclient;
using namespace adaptive_lora::test;

namespace {
struct Fixture {
    FakeConfigStorage storage;
    FakeRadioHardware hardware;
#if defined(ADAPTIVE_LORA_OLED)
    OledStatusDisplay panel;
    FirmwareRuntime runtime;
    explicit Fixture(bool display = false)
        : runtime(storage, hardware, false, nullptr, nullptr, display ? &panel : nullptr) {}
#else
    FirmwareRuntime runtime{storage, hardware};
#endif
    void setup(DeviceRole role, uint16_t gateway = 1) {
        if (role != DeviceRole::UNSET) {
            DeviceConfig c;
            c.role = role;
            c.gatewayId = gateway;
            c.nodeId = role == DeviceRole::GATEWAY ? gateway : 2;
            TEST_ASSERT_TRUE(encodeConfig(c, storage.bytes, sizeof(storage.bytes)));
            storage.length = kConfigBlobSize;
        }
        runtime.setup();
    }
    void at(uint64_t now) {
        host_clock::set(now);
        hardware.resetTrace();
        runtime.loop();
        hardware.signal(0, false);
        TEST_ASSERT_FALSE(hardware.violation);
    }
    void irq(uint32_t flags, uint64_t now, uint64_t edgeUs) {
        hardware.signal(flags);
        hardware.irqTimeValid = edgeUs != UINT64_MAX;
        hardware.irqTimeUs = edgeUs;
        at(now);
    }
    void done(uint64_t now, uint64_t edgeUs = UINT64_MAX) { irq(radio_irq::txDone, now, edgeUs); }
    void rx(const Frame& f, uint64_t now, uint64_t edgeUs = UINT64_MAX) {
        TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(encode(f, hardware.rxBytes, sizeof(hardware.rxBytes), hardware.packetLength)));
        irq(radio_irq::rxDone, now, edgeUs);
    }
    Frame tx(const MessageType type, uint16_t dst) {
        Frame f;
        TEST_ASSERT_EQUAL_UINT(1, hardware.count(HardwareCall::START_TX));
        TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(decode(hardware.txBytes, hardware.txLength, 1, dst, f)));
        TEST_ASSERT_EQUAL_INT(int(type), int(f.type));
        return f;
    }
};

Frame incoming(MessageType type, bool group) {
    Frame f;
    f.type = type; f.networkId = 1; f.frameId = 1; f.scheduleVersion = kScheduleVersion;
    f.sourceId = group ? 1 : 2;
    f.destinationId = group ? kGroup : 1;
    f.bootId = 200;
    return f;
}

void runtime_client_submits_arbitrary_bytes_to_actual_driver() {
    Fixture f;
    f.setup(DeviceRole::CLIENT);
    const uint8_t bytes[]{0x00, 0xe0, 0x14};
    TEST_ASSERT_EQUAL_INT(int(SubmitResult::QUEUED), int(f.runtime.submitPayload(bytes, sizeof(bytes))));
    f.at(0);
    Roster roster{10000000, 1, {{2, 64}}};
    auto b = incoming(MessageType::BEACON, true);
    writeRoster(b, roster);
    const uint64_t beaconEnd = payloadAir(rosterBytes(1));
    f.rx(b, beaconEnd + 30000, beaconEnd);
    const uint64_t requestAt = beaconEnd + requestStart(0);
    f.at(requestAt);
    const auto request = f.tx(MessageType::PAYLOAD_REQUEST, 1);
    TEST_ASSERT_EQUAL_UINT8(sizeof(bytes), request.payload[4]);
    f.done(requestAt + payloadAir(kRequestSize));
    Plan plan{1, {{request.bootId, sizeof(bytes)}}};
    auto grant = incoming(MessageType::SLOT_GRANT, true);
    writePlan(grant, plan);
    const uint64_t grantEnd = beaconEnd + grantStart(1) + payloadAir(grantBytes(1));
    f.rx(grant, grantEnd + 30000, grantEnd);
    const auto dataAt = grantEnd + plan.dataStart(0, 0, 1);
    f.at(dataAt);
    const auto data = f.tx(MessageType::DATA, 1);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, data.payload + kPayloadPrefix, sizeof(bytes));
    f.done(dataAt + dataAir(sizeof(bytes)));
    auto ack = incoming(MessageType::ACK_BITMAP, true);
    ack.payloadLength = feedbackBytes(1); ack.payload[0] = 1; ack.payload[1] = 1;
    put32(ack.payload + 3, request.bootId);
    const auto ackEnd = grantEnd + plan.feedbackStart(0) + payloadAir(feedbackBytes(1));
    f.rx(ack, ackEnd + 30000, ackEnd);
    TEST_ASSERT_EQUAL_INT(int(PayloadResult::ACCEPTED), int(f.runtime.payloadResult()));
}

void runtime_gateway_delivers_binary_to_its_real_application_boundary() {
    Fixture f;
    f.setup(DeviceRole::GATEWAY);
    f.at(0);
    const auto beacon = f.tx(MessageType::BEACON, kGroup);
    Roster roster;
    TEST_ASSERT_TRUE(readRoster(beacon, roster));
    TEST_ASSERT_EQUAL_UINT(kBenchRoster.count, roster.count);
    const uint64_t beaconEnd = payloadAir(rosterBytes(roster.count));
    f.done(beaconEnd + 30000, beaconEnd);
    auto request = incoming(MessageType::PAYLOAD_REQUEST, false);
    request.sequence = 1; request.payloadLength = kRequestSize; request.payload[4] = 3;
    put32(request.payload, beacon.bootId);
    f.rx(request, beaconEnd + requestStart(0) + payloadAir(kRequestSize));
    const uint64_t grantAt = beaconEnd + grantStart(roster.count);
    f.at(grantAt);
    const auto grant = f.tx(MessageType::SLOT_GRANT, kGroup);
    Plan plan;
    TEST_ASSERT_TRUE(readPlan(grant, plan));
    const uint64_t grantEnd = grantAt + payloadAir(grantBytes(plan.count));
    f.done(grantEnd + 30000, grantEnd);
    auto data = request;
    data.type = MessageType::DATA; data.payloadLength = kPayloadPrefix + 3;
    data.payload[4] = 0; data.payload[5] = 0xa4; data.payload[6] = 0;
    const auto dataEnd = grantEnd + plan.dataStart(0, 0, 1) + dataAir(3);
    f.rx(data, dataEnd + 30000, dataEnd);
    for (unsigned i = 1; i < 80; ++i) f.at(dataEnd + 30000 + i);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("hex=00a400"));
    f.at(grantEnd + plan.feedbackStart(0));
    const auto feedback = f.tx(MessageType::ACK_BITMAP, kGroup);
    TEST_ASSERT_EQUAL_UINT8(1, feedback.payload[1]);
}

void runtime_fail_safe_for_missing_config_or_gateway_id_in_roster() {
    for (const auto role : {DeviceRole::UNSET, DeviceRole::GATEWAY}) {
        Fixture f;
        f.setup(role, 3);
        TEST_ASSERT_EQUAL_UINT(0, f.hardware.count(HardwareCall::BEGIN));
        f.at(1000000);
        TEST_ASSERT_EQUAL_UINT(0, f.hardware.count(HardwareCall::START_TX));
        TEST_ASSERT_EQUAL_INT(int(SubmitResult::INACTIVE), int(f.runtime.submitPayload(nullptr, 0)));
    }
}

#if defined(ADAPTIVE_LORA_OLED)
void v3_oled_rotates_in_order_after_skipped_refresh_windows() {
    for (const auto role : {DeviceRole::CLIENT, DeviceRole::GATEWAY}) {
        Wire = HostWire{};
        OledStatusDisplay panel;
        DisplaySnapshot snapshot;
        snapshot.protocol = kVersion;
        snapshot.config.role = role;
        snapshot.multiclient.frame = 1;
        panel.start(0);
        panel.service(100000);
        panel.service(110000);
        panel.service(111000);
        panel.service(112000);

        // A completed first chunk of row zero contains a distinguishing
        // glyph column from LINK, DATA or ACK at pixel 19.
        uint8_t title = 0;
        const auto paint = [&](uint64_t now, unsigned steps = 60) {
            if (panel.needsSnapshot(now)) panel.render(snapshot, now);
            for (unsigned step = 0; step < steps; ++step) {
                host_clock::set(now + step * 1000);
                const auto writes = Wire.writes;
                panel.service(host_clock::value());
                if (Wire.writes != writes && Wire.length == 31 &&
                    Wire.bytes[1] == 0xb0 && Wire.bytes[3] == 0 && Wire.bytes[5] == 0x10)
                    title = Wire.bytes[7 + 19];
            }
        };
        paint(120000);
        TEST_ASSERT_TRUE(panel.ready());
        TEST_ASSERT_EQUAL_HEX8(0x40, title);  // LINK
        paint(11000000, 2);  // Two wall-clock boundaries elapsed, but only two chunks fit.
        TEST_ASSERT_EQUAL_HEX8(0x40, title);  // Do not announce DATA while its body is incomplete.
        paint(12000000);
        TEST_ASSERT_EQUAL_HEX8(0x41, title);  // DATA
        paint(17000000);  // Less than 5 s since the previous page finished.
        TEST_ASSERT_EQUAL_HEX8(0x41, title);
        paint(18000000);
        TEST_ASSERT_EQUAL_HEX8(0x11, title);  // ACK
        paint(29000000);
        TEST_ASSERT_EQUAL_HEX8(0x40, title);  // LINK
    }
}

void v3_client_draws_boot_screen_before_listening_for_beacons() {
    Fixture f(true);
    f.setup(DeviceRole::CLIENT);
    bool receiving = false;
    for (uint64_t now = 0; now < 400000; now += 3000) {
        const bool alreadyLit = f.panel.ready();
        f.at(now);
        if (f.hardware.count(HardwareCall::START_RX)) {
            TEST_ASSERT_TRUE_MESSAGE(alreadyLit, "Client began beacon RX before showing its boot screen");
            receiving = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(f.panel.ready());
    TEST_ASSERT_TRUE(receiving);
    const auto writes = Wire.writes;
    for (uint64_t now = 7000000; now < 7100000; now += 1000) {
        f.at(now);
        TEST_ASSERT_EQUAL_UINT(0, f.hardware.count(HardwareCall::STANDBY));
        TEST_ASSERT_EQUAL_UINT(0, f.hardware.count(HardwareCall::START_TX));
    }
    TEST_ASSERT_GREATER_THAN_UINT(writes, Wire.writes);
}

void v3_client_starts_listening_at_oled_startup_deadline() {
    Fixture f(true);
    f.setup(DeviceRole::CLIENT);
    f.at(100000);  // Start reset settling but leave the first image incomplete.
    f.at(399999);
    TEST_ASSERT_EQUAL_UINT(0, f.hardware.count(HardwareCall::START_RX));
    TEST_ASSERT_FALSE(f.panel.ready());
    f.at(400000);
    TEST_ASSERT_EQUAL_UINT(1, f.hardware.count(HardwareCall::START_RX));
    TEST_ASSERT_EQUAL_UINT(0, Wire.writes);
    TEST_ASSERT_TRUE(f.runtime.safeToRefreshDisplay(400000));
}

void v3_client_starts_listening_after_oled_bus_failure() {
    Fixture f(true);
    Wire.beginOk = false;
    f.setup(DeviceRole::CLIENT);
    f.at(100000); f.at(110000); f.at(111000);
    TEST_ASSERT_EQUAL_STRING("i2c_begin", f.panel.error());
    f.at(112000);
    TEST_ASSERT_EQUAL_UINT(1, f.hardware.count(HardwareCall::START_RX));
    TEST_ASSERT_EQUAL_UINT(0, Wire.writes);
}

void v3_oled_renders_link_data_and_bitmap() {
    DisplaySnapshot snapshot;
    snapshot.protocol = kVersion;
    snapshot.config.role = DeviceRole::CLIENT;
    snapshot.multiclient.frame = 34;
    snapshot.multiclient.periodUs = 3000000;
    auto screen = formatOledStatus(snapshot, 0);
    TEST_ASSERT_EQUAL_STRING("V3 LINK 1/3", screen.rows[0]);
    for (size_t i = sizeof("V3 LINK 1/3"); i < sizeof(screen.rows[0]); ++i)
        TEST_ASSERT_EQUAL_HEX8(0, screen.rows[0][i]);
    TEST_ASSERT_EQUAL_STRING("CLIENT 2", screen.rows[1]);
    TEST_ASSERT_EQUAL_STRING("LAST WAIT", screen.rows[2]);
    TEST_ASSERT_EQUAL_STRING("RSSI N/A", screen.rows[3]);
    TEST_ASSERT_EQUAL_STRING("FRAME 34", screen.rows[6]);
    TEST_ASSERT_EQUAL_STRING("PERIOD 3000 MS", screen.rows[7]);
    snapshot.searchingForBeacon = true;
    screen = formatOledStatus(snapshot, 0, 2);
    TEST_ASSERT_EQUAL_STRING("CLIENT 2 SEARCH", screen.rows[1]);
    snapshot.searchingForBeacon = false;

    auto& v3 = snapshot.multiclient;
    v3.peer = {true, true, 1, 11000000, -81.5f, 5.0f};
    v3.peerPeriodUs = 3000000;
    screen = formatOledStatus(snapshot, 16999999);
    TEST_ASSERT_EQUAL_STRING("LAST ONLINE P 1", screen.rows[2]);
    TEST_ASSERT_EQUAL_STRING("RSSI -81.5 DBM", screen.rows[3]);
    TEST_ASSERT_EQUAL_STRING("SNR 5.0 DB", screen.rows[4]);
    screen = formatOledStatus(snapshot, 17000000);
    TEST_ASSERT_EQUAL_STRING("LAST STALE P 1", screen.rows[2]);

    v3.data = 12; v3.accepted = 9; v3.retries = 3; v3.radioErrors = 1;
    v3.result = PayloadResult::MISSING;
    screen = formatOledStatus(snapshot, 5000000, 1);
    TEST_ASSERT_EQUAL_STRING("DATA TX 12", screen.rows[2]);
    TEST_ASSERT_EQUAL_STRING("RETRY 3", screen.rows[3]);
    TEST_ASSERT_EQUAL_STRING("APP OK 9", screen.rows[4]);
    TEST_ASSERT_EQUAL_STRING("APP MISSING", screen.rows[5]);
    TEST_ASSERT_EQUAL_STRING("RADIO ERR 1", screen.rows[7]);

    snapshot.config.role = DeviceRole::GATEWAY;
    snapshot.config.nodeId = 1;
    v3.refused = 2; v3.duplicates = 5;
    screen = formatOledStatus(snapshot, 5000000, 1);
    TEST_ASSERT_EQUAL_STRING("GATEWAY 1", screen.rows[1]);
    TEST_ASSERT_EQUAL_STRING("DATA RX 12", screen.rows[2]);
    TEST_ASSERT_EQUAL_STRING("REFUSED 2", screen.rows[5]);
    TEST_ASSERT_EQUAL_STRING("DUP 5", screen.rows[6]);
    v3.bitmap = {true, 33, 2, 0x05, 0x0a};
    screen = formatOledStatus(snapshot, 10000000, 2);
    TEST_ASSERT_EQUAL_STRING("ACK FRAME 33", screen.rows[2]);
    TEST_ASSERT_EQUAL_STRING("ROUND 2", screen.rows[3]);
    TEST_ASSERT_EQUAL_STRING("ACCEPT MASK 05", screen.rows[4]);
    TEST_ASSERT_EQUAL_STRING("RETRY MASK 0A", screen.rows[5]);
}

void v3_gateway_defers_all_i2c_until_after_final_feedback_and_protects_beacon() {
    Fixture f(true);
    f.setup(DeviceRole::GATEWAY);
    f.at(0);
    const auto beacon = f.tx(MessageType::BEACON, kGroup);
    Roster roster;
    TEST_ASSERT_TRUE(readRoster(beacon, roster));
    TEST_ASSERT_EQUAL_UINT32(6766000, roster.periodUs);
    const auto beaconEnd = payloadAir(rosterBytes(roster.count));
    f.done(beaconEnd);
    auto request = incoming(MessageType::PAYLOAD_REQUEST, false);
    request.sequence = 1; request.payloadLength = kRequestSize; request.payload[4] = 64;
    put32(request.payload, beacon.bootId);
    for (uint8_t i = 0; i < roster.count; ++i) {
        request.sourceId = roster.nodes[i].id;
        request.bootId = 200 + i;
        f.rx(request, beaconEnd + requestStart(i) + payloadAir(kRequestSize));
    }
    const auto grantAt = beaconEnd + grantStart(roster.count);
    f.at(grantAt);
    const auto grant = f.tx(MessageType::SLOT_GRANT, kGroup);
    Plan plan;
    TEST_ASSERT_TRUE(readPlan(grant, plan));
    TEST_ASSERT_EQUAL_UINT8(0xff, plan.activeMask());
    const auto grantEnd = grantAt + payloadAir(grantBytes(plan.count));
    f.done(grantEnd);
    const auto data = grantEnd + plan.dataStart(0, 0, plan.activeMask());
    f.at(data);
    TEST_ASSERT_FALSE(f.runtime.safeToRefreshDisplay(data));
    uint64_t lastEnd = 0;
    for (uint8_t round = 0; round < kAttempts; ++round) {
        const auto ackAt = grantEnd + plan.feedbackStart(round);
        f.at(ackAt);
        const auto ack = f.tx(MessageType::ACK_BITMAP, kGroup);
        TEST_ASSERT_EQUAL_UINT8(round, ack.sequence);
        TEST_ASSERT_EQUAL_UINT8(round + 1 == kAttempts ? 0 : retryMask(0xff, roster.count, 1, round), ack.payload[2]);
        TEST_ASSERT_FALSE(f.runtime.safeToRefreshDisplay(ackAt));
        lastEnd = ackAt + payloadAir(feedbackBytes(plan.count));
        f.done(lastEnd);
        TEST_ASSERT_FALSE(f.runtime.safeToRefreshDisplay(lastEnd));
        TEST_ASSERT_EQUAL_INT(-1, Wire.sda);
        TEST_ASSERT_EQUAL_UINT(0, Wire.writes);
    }
    const uint64_t quiet = lastEnd + kLaunchUs;
    TEST_ASSERT_TRUE(f.runtime.safeToRefreshDisplay(quiet));
    f.hardware.signal(radio_irq::rxDone);
    TEST_ASSERT_FALSE(f.runtime.safeToRefreshDisplay(quiet));
    f.hardware.signal(0, false);
    Wire.elapsedUs = 3000;
    for (unsigned i = 0; i < 57; ++i) f.at(quiet + i * 4000);
    TEST_ASSERT_TRUE(f.panel.ready());
    TEST_ASSERT_EQUAL_UINT(3, Wire.timeout);
    TEST_ASSERT_LESS_OR_EQUAL_UINT(31, Wire.largest);
    const uint64_t boundary = roster.periodUs - kGuardUs / 2 - kOledStepBudgetUs;
    TEST_ASSERT_TRUE(f.runtime.safeToRefreshDisplay(boundary));
    TEST_ASSERT_FALSE(f.runtime.safeToRefreshDisplay(boundary + 1));
    const auto writes = Wire.writes;
    // An earlier UART step can use the last available time budget.
    TEST_ASSERT_TRUE(Serial.inject("status\n", 7));
    Serial.writeDelayUs = 1000;
    f.at(boundary);
    Serial.writeDelayUs = 0;
    TEST_ASSERT_EQUAL_UINT(writes, Wire.writes);
    f.at(roster.periodUs);
    TEST_ASSERT_EQUAL_UINT32(2, f.tx(MessageType::BEACON, kGroup).frameId);
    TEST_ASSERT_EQUAL_UINT(writes, Wire.writes);

    const auto secondBeaconEnd = roster.periodUs + beaconEnd;
    f.done(secondBeaconEnd);
    f.at(secondBeaconEnd + grantStart(roster.count));  // Quiet tail with no requests.
    const auto before = Wire.writes;
    const auto secondBoundary = boundary + roster.periodUs;
    // RX polling consumes 50 us and the first pending IRQ check consumes
    // another 50 us. The second clock sample must stop the pending draw.
    f.hardware.delayUs[static_cast<size_t>(HardwareCall::SIGNAL)] = 50;
    f.at(secondBoundary - 75);
    TEST_ASSERT_EQUAL_UINT(before, Wire.writes);
}

void v3_client_repaints_after_full_roster_calendar() {
    Fixture f(true);
    f.setup(DeviceRole::CLIENT);
    Wire.elapsedUs = 3000;
    for (uint64_t now = 0; now < 400000; now += 4000) f.at(now);
    TEST_ASSERT_TRUE(f.panel.ready());

    auto roster = kBenchRoster;
    roster.periodUs = 6766000;
    auto beacon = incoming(MessageType::BEACON, true);
    writeRoster(beacon, roster);
    auto grant = incoming(MessageType::SLOT_GRANT, true);
    Plan plan;
    plan.count = roster.count;
    for (uint8_t i = 0; i < roster.count; ++i) plan.slots[i] = {200u + i, 64};
    writePlan(grant, plan);

    uint64_t beaconStart = 450000;
    for (uint32_t frame = 1; frame <= 2; ++frame, beaconStart += roster.periodUs) {
        beacon.frameId = grant.frameId = frame;
        const uint64_t beaconEnd = beaconStart + payloadAir(rosterBytes(roster.count));
        f.rx(beacon, beaconEnd);
        const uint64_t grantEnd = beaconEnd + grantStart(roster.count) + payloadAir(grantBytes(plan.count));
        f.rx(grant, grantEnd);
        const uint64_t quiet = grantEnd + plan.feedbackStart(kAttempts - 1) +
            payloadAir(feedbackBytes(plan.count)) + kGuardUs / 2;
        const auto previousWrites = Wire.writes;
        f.at(quiet - 1);
        TEST_ASSERT_EQUAL_UINT(previousWrites, Wire.writes);
        uint8_t title = 0;
        for (auto now = quiet; now < beaconStart + roster.periodUs - kGuardUs / 2; now += 4000) {
            const auto writes = Wire.writes;
            f.at(now);
            if (Wire.writes != writes && Wire.length == 31 && Wire.bytes[1] == 0xb0 &&
                Wire.bytes[3] == 0 && Wire.bytes[5] == 0x10)
                title = Wire.bytes[7 + 19];
        }
        TEST_ASSERT_GREATER_THAN_UINT(previousWrites, Wire.writes);
        TEST_ASSERT_EQUAL_HEX8(frame == 1 ? 0x41 : 0x11, title);
        const auto lastWrites = Wire.writes;
        f.at(beaconStart + roster.periodUs - 1);
        TEST_ASSERT_EQUAL_UINT(lastWrites, Wire.writes);
        TEST_ASSERT_NULL(f.panel.error());
    }
    // The last frame ended on ACK. Missing the third beacon should advance
    // to LINK in search without turning the receiver off.
    const auto searchAt = beaconStart + payloadAir(rosterBytes(roster.count)) + kGuardUs / 2;
    uint8_t title = 0;
    for (unsigned i = 0; i < 1500; ++i) {
        const auto writes = Wire.writes;
        f.at(searchAt + i * 4000);
        TEST_ASSERT_EQUAL_UINT(0, f.hardware.count(HardwareCall::STANDBY));
        if (Wire.writes != writes && Wire.length == 31 && Wire.bytes[1] == 0xb0 &&
            Wire.bytes[3] == 0 && Wire.bytes[5] == 0x10)
            title = Wire.bytes[7 + 19];
    }
    TEST_ASSERT_EQUAL_HEX8(0x40, title);
}

void v3_client_protects_peer_slots_then_refreshes_and_recovers_after_outage() {
    Fixture f(true);
    f.setup(DeviceRole::CLIENT);
    TEST_ASSERT_EQUAL_INT(int(SubmitResult::QUEUED), int(f.runtime.submitPayload(nullptr, 0)));
    for (uint64_t now = 0; now < 400000; now += 3000) f.at(now);
    TEST_ASSERT_TRUE(f.panel.ready());
    const auto startupWrites = Wire.writes;
    Roster roster{10000000, 2, {{2, 64}, {3, 64}}};
    auto beacon = incoming(MessageType::BEACON, true);
    writeRoster(beacon, roster);
    const uint64_t beaconStart = 450000;
    const auto beaconEnd = beaconStart + payloadAir(rosterBytes(roster.count));
    f.rx(beacon, beaconEnd);
    const auto requestAt = beaconEnd + requestStart(0);
    f.at(requestAt);
    const auto request = f.tx(MessageType::PAYLOAD_REQUEST, 1);
    f.done(requestAt + payloadAir(kRequestSize));
    const Plan plan{2, {{request.bootId, 0}, {555, 64}}};
    auto grant = incoming(MessageType::SLOT_GRANT, true);
    writePlan(grant, plan);
    const auto grantEnd = beaconEnd + grantStart(roster.count) + payloadAir(grantBytes(plan.count));
    f.rx(grant, grantEnd);
    const auto dataAt = grantEnd + plan.dataStart(0, 0, 3);
    f.at(dataAt);
    f.tx(MessageType::DATA, 1);
    f.done(dataAt + dataAir(0));
    auto ack = incoming(MessageType::ACK_BITMAP, true);
    ack.payloadLength = feedbackBytes(2); ack.payload[0] = 2; ack.payload[1] = 1; ack.payload[2] = 2;
    put32(ack.payload + 3, request.bootId);
    const auto ackAir = payloadAir(feedbackBytes(plan.count));
    const auto ackEnd = grantEnd + plan.feedbackStart(0) + ackAir;
    f.rx(ack, ackEnd);
    TEST_ASSERT_EQUAL_INT(int(PayloadResult::ACCEPTED), int(f.runtime.payloadResult()));
    TEST_ASSERT_FALSE(f.runtime.safeToRefreshDisplay(ackEnd));
    TEST_ASSERT_EQUAL_UINT(startupWrites, Wire.writes);
    const auto quiet = grantEnd + plan.feedbackStart(3) + ackAir + kGuardUs / 2;
    TEST_ASSERT_FALSE(f.runtime.safeToRefreshDisplay(quiet - 1));
    TEST_ASSERT_TRUE(f.runtime.safeToRefreshDisplay(quiet));
    for (unsigned i = 0; i < 60; ++i) f.at(quiet + i * 5000);
    TEST_ASSERT_TRUE(f.panel.ready());
    const auto writes = Wire.writes;
    const uint64_t searchAt = beaconEnd + roster.periodUs + kGuardUs / 2;
    f.at(searchAt - 1);
    TEST_ASSERT_EQUAL_UINT(writes, Wire.writes);
    TEST_ASSERT_FALSE(f.runtime.safeToRefreshDisplay(host_clock::value()));
    // No beacon for several periods. Inspect the titles written to the panel,
    // and make sure the receiver stays on while a fresh payload is queued.
    TEST_ASSERT_EQUAL_INT(int(SubmitResult::QUEUED), int(f.runtime.submitPayload(nullptr, 0)));
    constexpr uint8_t expected[]{0x41, 0x11, 0x40};
    uint64_t prevTitleAt = 0;
    size_t titleCount = 0;
    for (uint64_t now = searchAt; now < searchAt + 3 * uint64_t(roster.periodUs); now += 2000) {
        const auto before = Wire.writes;
        f.at(now);
        TEST_ASSERT_EQUAL_UINT(0, f.hardware.count(HardwareCall::STANDBY));
        TEST_ASSERT_EQUAL_UINT(0, f.hardware.count(HardwareCall::START_TX));
        if (Wire.writes != before && Wire.length == 31 && Wire.bytes[1] == 0xb0 &&
            Wire.bytes[3] == 0 && Wire.bytes[5] == 0x10) {
            TEST_ASSERT_EQUAL_HEX8(expected[titleCount % sizeof(expected)], Wire.bytes[7 + 19]);
            if (titleCount) TEST_ASSERT_TRUE_MESSAGE(now - prevTitleAt >= 5000000, "Page dwell less than five seconds");
            prevTitleAt = now;
            ++titleCount;
        }
    }
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(6, titleCount);
    TEST_ASSERT_TRUE(f.runtime.safeToRefreshDisplay(host_clock::value()));
    TEST_ASSERT_TRUE(Serial.inject("status\n", 7));
    const auto restoredAt = searchAt + 4 * uint64_t(roster.periodUs);
    for (unsigned i = 0; i < 40; ++i) f.at(restoredAt - 100000 + i);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("event=V3_LINK_STATUS,freshness=STALE"));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find(",beacon=search"));
    f.rx(beacon, restoredAt - 75000);  // Reject replay of the previous session's last frame.
    f.at(restoredAt - 75000 + requestStart(0));
    TEST_ASSERT_EQUAL_UINT(0, f.hardware.count(HardwareCall::START_TX));
    const auto beforeRestore = Wire.writes;
    beacon.bootId++;
    f.rx(beacon, restoredAt + kOledStepBudgetUs);
    TEST_ASSERT_EQUAL_UINT(beforeRestore, Wire.writes);
    const auto restoredRequestAt = restoredAt + kOledStepBudgetUs + requestStart(0);
    f.at(restoredRequestAt);
    const auto restoredRequest = f.tx(MessageType::PAYLOAD_REQUEST, 1);
    TEST_ASSERT_EQUAL_UINT32(beacon.bootId, get32(restoredRequest.payload));
    TEST_ASSERT_EQUAL_UINT(beforeRestore, Wire.writes);
    f.done(restoredRequestAt + payloadAir(kRequestSize));
    const auto restoredGrantEnd = restoredAt + grantEnd - beaconEnd;
    grant.bootId = beacon.bootId;
    f.rx(grant, restoredGrantEnd);
    const auto restoredDataAt = restoredGrantEnd + plan.dataStart(0, 0, 3);
    f.at(restoredDataAt);
    const auto restoredData = f.tx(MessageType::DATA, 1);
    TEST_ASSERT_EQUAL_UINT32(beacon.bootId, get32(restoredData.payload));
    f.done(restoredDataAt + dataAir(0));
    ack.bootId = beacon.bootId;
    f.rx(ack, restoredGrantEnd + plan.feedbackStart(0) + ackAir);
    TEST_ASSERT_EQUAL_INT(int(PayloadResult::ACCEPTED), int(f.runtime.payloadResult()));
    TEST_ASSERT_EQUAL_UINT(beforeRestore, Wire.writes);
}

void v3_gateway_oled_progresses_when_beacon_launch_fails() {
    Fixture f(true);
    f.setup(DeviceRole::GATEWAY);
    f.hardware.startTxCode = radio_status::unknown;
    f.at(0);
    TEST_ASSERT_EQUAL_UINT(1, f.hardware.count(HardwareCall::START_TX));
    Roster roster;
    Frame beacon;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(decode(f.hardware.txBytes, f.hardware.txLength, 1, kGroup, beacon)));
    TEST_ASSERT_TRUE(readRoster(beacon, roster));
    const auto requestEnd = payloadAir(rosterBytes(roster.count)) + grantStart(roster.count) + kGuardUs / 2;
    f.at(requestEnd - 1);
    TEST_ASSERT_EQUAL_UINT(0, Wire.writes);
    for (unsigned i = 0; i < 180; ++i) f.at(requestEnd + i * 2000);
    TEST_ASSERT_TRUE(f.panel.ready());
    f.hardware.startTxCode = 0;
    f.at(roster.periodUs);
    TEST_ASSERT_EQUAL_UINT32(2, f.tx(MessageType::BEACON, kGroup).frameId);
}

void v3_oled_recovers_from_transient_i2c_error_during_beacon_search() {
    Fixture f(true);
    f.setup(DeviceRole::CLIENT);
    Wire.failure = 2;
    for (uint64_t now = 0; now < 400000; now += 3000) f.at(now);
    TEST_ASSERT_EQUAL_STRING("i2c_transfer", f.panel.error());
    const auto writes = Wire.writes;
    const auto gpioCalls = hostOledGpio().calls;
    for (uint64_t now = 400000; now < 2000000; now += 3000) f.at(now);
    TEST_ASSERT_EQUAL_UINT(writes, Wire.writes);
    Wire.failure = 0;
    for (uint64_t now = 2000000; now < 2700000; now += 3000) {
        f.at(now);
        TEST_ASSERT_EQUAL_UINT(0, f.hardware.count(HardwareCall::START_TX));
    }
    TEST_ASSERT_NULL(f.panel.error());
    TEST_ASSERT_TRUE(f.panel.ready());
    TEST_ASSERT_EQUAL_INT(gpioCalls + 2, hostOledGpio().calls);
    TEST_ASSERT_EQUAL_INT(RST_OLED, hostOledGpio().pins[gpioCalls]);
    TEST_ASSERT_EQUAL_INT(RST_OLED, hostOledGpio().pins[gpioCalls + 1]);
}

void v3_oled_overrun_disables_display_and_preserves_provisioning() {
    Fixture f(true);
    f.setup(DeviceRole::UNSET);
    f.at(100000); f.at(110000); f.at(111000);
    Wire.elapsedUs = kOledStepBudgetUs + 1;
    f.at(112000);
    TEST_ASSERT_EQUAL_STRING("service_budget_overrun", f.panel.error());
    TEST_ASSERT_EQUAL_UINT(1, Wire.writes);
    TEST_ASSERT_TRUE(Serial.inject("status\n", 7));
    for (unsigned i = 0; i < 64; ++i) f.at(160000 + i);
    TEST_ASSERT_EQUAL_UINT(1, Wire.writes);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("worst_step_us=10001"));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("mode=PROVISIONING,radio=disabled"));
    TEST_ASSERT_EQUAL_UINT(0, f.hardware.traceLength);
}

void v3_gateway_keeps_next_beacon_after_oled_overrun_in_quiet_tail() {
    Fixture f(true);
    f.setup(DeviceRole::GATEWAY);
    f.at(0);
    const auto first = f.tx(MessageType::BEACON, kGroup);
    Roster roster;
    TEST_ASSERT_TRUE(readRoster(first, roster));
    const uint64_t end = payloadAir(rosterBytes(roster.count));
    f.done(end);
    const auto quiet = end + grantStart(roster.count);  // No requests in this frame.
    f.at(quiet); f.at(quiet + 10000); f.at(quiet + 11000);
    Wire.elapsedUs = kOledStepBudgetUs + 1;
    f.at(quiet + 12000);
    TEST_ASSERT_EQUAL_STRING("service_budget_overrun", f.panel.error());
    TEST_ASSERT_EQUAL_UINT(1, Wire.writes);
    f.at(roster.periodUs);
    const auto next = f.tx(MessageType::BEACON, kGroup);
    TEST_ASSERT_EQUAL_UINT32(first.frameId + 1, next.frameId);
    TEST_ASSERT_EQUAL_UINT32(first.bootId, next.bootId);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(first.payload, next.payload, first.payloadLength);
    TEST_ASSERT_EQUAL_UINT(1, Wire.writes);
}
#endif
}

void setUp() {
    Serial.reset(); host_clock::set(0); hostRandom() = HostRandom{};
    hostRandom().values[0] = 100; hostRandom().length = 1;
#if defined(ADAPTIVE_LORA_OLED)
    Wire = HostWire{};
    hostOledGpio() = HostOledGpio{};
#endif
}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(runtime_client_submits_arbitrary_bytes_to_actual_driver);
    RUN_TEST(runtime_gateway_delivers_binary_to_its_real_application_boundary);
    RUN_TEST(runtime_fail_safe_for_missing_config_or_gateway_id_in_roster);
#if defined(ADAPTIVE_LORA_OLED)
    RUN_TEST(v3_oled_rotates_in_order_after_skipped_refresh_windows);
    RUN_TEST(v3_client_draws_boot_screen_before_listening_for_beacons);
    RUN_TEST(v3_client_starts_listening_at_oled_startup_deadline);
    RUN_TEST(v3_client_starts_listening_after_oled_bus_failure);
    RUN_TEST(v3_oled_renders_link_data_and_bitmap);
    RUN_TEST(v3_gateway_defers_all_i2c_until_after_final_feedback_and_protects_beacon);
    RUN_TEST(v3_client_repaints_after_full_roster_calendar);
    RUN_TEST(v3_client_protects_peer_slots_then_refreshes_and_recovers_after_outage);
    RUN_TEST(v3_gateway_oled_progresses_when_beacon_launch_fails);
    RUN_TEST(v3_oled_recovers_from_transient_i2c_error_during_beacon_search);
    RUN_TEST(v3_oled_overrun_disables_display_and_preserves_provisioning);
    RUN_TEST(v3_gateway_keeps_next_beacon_after_oled_overrun_in_quiet_tail);
#endif
    return UNITY_END();
}
