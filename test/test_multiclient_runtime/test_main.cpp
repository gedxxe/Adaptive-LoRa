#include <unity.h>
#include <Arduino.h>
#include <esp_random.h>
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
    FirmwareRuntime runtime{storage, hardware};
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
    void done(uint64_t now) { hardware.signal(radio_irq::txDone); at(now); }
    void rx(const Frame& f, uint64_t now) {
        TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(encode(f, hardware.rxBytes, sizeof(hardware.rxBytes), hardware.packetLength)));
        hardware.signal(radio_irq::rxDone); at(now);
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
    f.rx(b, beaconEnd);
    const uint64_t requestAt = beaconEnd + requestStart(0);
    f.at(requestAt);
    const auto request = f.tx(MessageType::PAYLOAD_REQUEST, 1);
    TEST_ASSERT_EQUAL_UINT8(sizeof(bytes), request.payload[4]);
    f.done(requestAt + payloadAir(kRequestSize));
    Plan plan{1, {{request.bootId, sizeof(bytes)}}};
    auto grant = incoming(MessageType::SLOT_GRANT, true);
    writePlan(grant, plan);
    const uint64_t grantEnd = beaconEnd + grantStart(1) + payloadAir(grantBytes(1));
    f.rx(grant, grantEnd);
    const auto dataAt = grantEnd + plan.dataStart(0, 0, 1);
    f.at(dataAt);
    const auto data = f.tx(MessageType::DATA, 1);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, data.payload + kPayloadPrefix, sizeof(bytes));
    f.done(dataAt + dataAir(sizeof(bytes)));
    auto ack = incoming(MessageType::ACK_BITMAP, true);
    ack.payloadLength = feedbackBytes(1); ack.payload[0] = 1; ack.payload[1] = 1;
    put32(ack.payload + 3, request.bootId);
    f.rx(ack, grantEnd + plan.feedbackStart(0) + payloadAir(feedbackBytes(1)));
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
    f.done(beaconEnd);
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
    f.done(grantEnd);
    auto data = request;
    data.type = MessageType::DATA; data.payloadLength = kPayloadPrefix + 3;
    data.payload[4] = 0; data.payload[5] = 0xa4; data.payload[6] = 0;
    const auto dataEnd = grantEnd + plan.dataStart(0, 0, 1) + dataAir(3);
    f.rx(data, dataEnd);
    for (unsigned i = 1; i < 80; ++i) f.at(dataEnd + i);
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
}

void setUp() {
    Serial.reset(); host_clock::set(0); hostRandom() = HostRandom{};
    hostRandom().values[0] = 100; hostRandom().length = 1;
}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(runtime_client_submits_arbitrary_bytes_to_actual_driver);
    RUN_TEST(runtime_gateway_delivers_binary_to_its_real_application_boundary);
    RUN_TEST(runtime_fail_safe_for_missing_config_or_gateway_id_in_roster);
    return UNITY_END();
}
