#include <Arduino.h>
#include <unity.h>
#include <vector>
#include "research_roster.h"
#include "app/firmware_runtime.h"
#include "app/multiclient_link.h"
#include "common/diagnostics/oled_status_display.h"
#include "common/radio/radio_driver.h"
#include "common/radio/sx1262_hardware.h"
#include "../support/fake_config_storage.h"
#include "../support/fake_radio_hardware.h"
#include "../support/fake_radio_transport.h"

using namespace adaptive_lora;
using namespace adaptive_lora::multiclient;
using namespace adaptive_lora::test;

namespace {
uint32_t identity = 0;
uint32_t boot(uint32_t) { return ++identity; }

DeviceConfig config(uint16_t id) {
    DeviceConfig device;
    device.role = id == 1 ? DeviceRole::GATEWAY : DeviceRole::CLIENT;
    device.nodeId = id;
    return device;
}

Roster roster(uint8_t count, uint8_t ceiling = kMaxData) {
    Roster result{kAdaptivePeriodUs, count, {}};
    for (uint8_t i = 0; i < count; ++i) result.nodes[i] = {uint16_t(i + 2), ceiling};
    return result;
}

struct Peer {
    FakeRadioTransport radio;
    SerialLogger log;
    PayloadPort payload;
    MulticlientLink link;

    Peer(uint16_t id, Roster schedule = {}, PayloadPort::Receiver callback = nullptr, void* context = nullptr,
         bool adaptDataSf = true)
        : payload(callback, context), link(config(id), radio, log, payload, boot, schedule, adaptDataSf) {
        log.begin(config(id));
    }
};

struct Tx {
    MessageType type;
    uint8_t sender;
    uint8_t sf;
    uint32_t frame;
    uint16_t round;
};

struct Network {
    unsigned delivered[kMaxClients]{};
    uint8_t minDataSf[kMaxClients]{};
    float snr[kMaxClients]{};
    uint8_t count;
    Peer gateway;
    Peer clients[kMaxClients]{{2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}};
    std::vector<Tx> tx;
    uint64_t now = 0;
    uint64_t txEnd = 0;
    int sender = -1;
    uint16_t listening = 0;
    uint8_t txSf = 0;
    Frame frame;
    bool dropGrant = false;
    bool dropFirstAck = false;
    bool dropAllAck = false;
    bool dropControl = false;
    bool dropFirstData = false;
    bool failDataLaunch = false;
    bool loseFinalTxDone = false;
    bool stallFirstRx = false;
    uint64_t resumeGateway = 0;
    bool outage = false;

    explicit Network(uint8_t n, uint8_t rosterCount = 0, uint8_t ceiling = kMaxData, bool adaptDataSf = true)
        : count(n), gateway(1, roster(rosterCount ? rosterCount : n, ceiling), accept, this, adaptDataSf) {}
    static bool accept(void* context, const PayloadDelivery& data) {
        auto& n = *static_cast<Network*>(context);
        ++n.delivered[data.source - 2];
        TEST_ASSERT_EQUAL_UINT16(uint16_t(data.frame), data.sequence);
        TEST_ASSERT_TRUE(data.size == 8 || data.size == 16 || data.size == 32 || data.size == 64);
        return true;
    }
    Peer& peer(uint8_t i) { return i ? clients[i - 1] : gateway; }

    void tick() {
        RadioEvent events[kMaxClients + 1];
        if (sender >= 0 && now >= txEnd) {
            auto& radio = peer(sender).radio;
            events[sender] = radio.completeTx();
            events[sender].hasCompletionTime = true;
            events[sender].completionUs = txEnd;
            if (loseFinalTxDone && sender == 0 && frame.type == MessageType::ACK_BITMAP &&
                frame.sequence == kAttempts - 1)
                events[sender] = radio.error(RadioEventType::ERROR, -910);
            for (uint8_t i = 0; i <= count; ++i) {
                if (!(listening & (1u << i)) || !peer(i).radio.receiving() || peer(i).radio.sf != txSf) continue;
                if (frame.destinationId != kGroup && frame.destinationId != i + 1) continue;
                if (outage && (i == 1 || sender == 1)) continue;
                if (sender > 0 && frame.type == MessageType::DATA && txSf < minDataSf[sender - 1])
                    continue;
                if (i == 1 && ((dropControl && frame.type != MessageType::DATA) ||
                    (dropGrant && frame.type == MessageType::SLOT_GRANT) ||
                    (frame.type == MessageType::ACK_BITMAP &&
                     (dropAllAck || (dropFirstAck && frame.sequence == 0))))) continue;
                events[i] = peer(i).radio.deliver(radio.lastTx);
                events[i].hasCompletionTime = true;
                events[i].completionUs = txEnd;
                if (i == 0 && frame.type == MessageType::DATA) {
                    events[i].metrics = true;
                    events[i].rssi = -95.0f;
                    events[i].snr = snr[sender - 1];
                }
            }
            sender = -1;
        }
        host_clock::set(now);
        if (failDataLaunch && events[1].type == RadioEventType::RX_DONE &&
            (frame.type == MessageType::SLOT_GRANT ||
             (frame.type == MessageType::ACK_BITMAP && frame.sequence + 1 < kAttempts)))
            clients[0].radio.failNextTx = true;
        for (uint8_t i = 0; i <= count; ++i) {
            if (i == 0 && now < resumeGateway) continue;
            peer(i).link.update(now, events[i]);
        }
        if (stallFirstRx && events[0].type == RadioEventType::TX_DONE &&
            frame.type == MessageType::SLOT_GRANT) {
            // Grant completion was serviced. Sleep across the first RX retune and DATA.
            resumeGateway = txEnd + kSlotGuardUs + dataAir(8, kBaseSf);
        }
        for (uint8_t i = 0; i <= count; ++i) {
            auto& p = peer(i);
            TEST_ASSERT_EQUAL_UINT(0, p.radio.contractViolations);
            if (p.radio.transmitting() && int(i) != sender) {
                TEST_ASSERT_EQUAL_INT_MESSAGE(-1, sender, "Two transmissions overlap on the single-radio calendar");
                sender = i;
                txSf = p.radio.sf;
                const auto& bytes = p.radio.lastTx;
                const auto dest = get16(bytes.bytes + 10);
                TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(decode(bytes.bytes, bytes.length, 1, dest, frame)));
                txEnd = now + airAtSf(bytes.length, txSf);
                listening = 0;
                for (uint8_t j = 0; j <= count; ++j)
                    if (peer(j).radio.receiving() && peer(j).radio.sf == txSf) listening |= 1u << j;
                if (frame.type != MessageType::DATA) TEST_ASSERT_EQUAL_UINT8(kControlSf, txSf);
                tx.push_back({frame.type, i, txSf, frame.frameId, frame.sequence});
                if (dropFirstData && i == 1 && frame.type == MessageType::DATA) {
                    listening &= ~1u;
                    dropFirstData = false;
                }
            }
            for (unsigned drain = 0; drain < 24 && !p.log.empty(); ++drain) p.log.drain();
        }
    }
    void rounds(unsigned n, uint8_t size = 8) {
        const uint8_t payload[kMaxData]{};
        for (unsigned frameIndex = 0; frameIndex < n; ++frameIndex) {
            for (uint8_t i = 0; i < count; ++i)
                if (!clients[i].payload.pending())
                    TEST_ASSERT_EQUAL_INT(int(SubmitResult::QUEUED), int(clients[i].payload.submit(payload, size)));
            const auto stop = now + kAdaptivePeriodUs;
            for (; now < stop; now += 1000) tick();
        }
    }
    bool sent(uint8_t client, uint8_t sf) const {
        for (const auto& packet : tx)
            if (packet.type == MessageType::DATA && packet.sender == client && packet.sf == sf) return true;
        return false;
    }
};

void mixed_profile_codec_and_budget() {
    const auto r = roster(8);
    Plan plan;
    plan.count = r.count;
    TEST_ASSERT_EQUAL_UINT32(164096, dataAir(64, 7));
    TEST_ASSERT_EQUAL_UINT32(297472, dataAir(64, 8));
    TEST_ASSERT_EQUAL_UINT32(533504, dataAir(64, 9));
    for (uint8_t i = 0; i < r.count; ++i) plan.slots[i] = {uint32_t(i + 1), 64, 7};
    TEST_ASSERT_TRUE(plan.matches(r));
    plan.slots[0] = {1, 8, 9};
    TEST_ASSERT_TRUE(plan.matches(r));
    TEST_ASSERT_EQUAL_UINT32(dataAir(8, 9) + 3 * dataAir(64, 7) + 4 * kSlotGuardUs, plan.retrySpan());
    Frame grant;
    grant.type = MessageType::SLOT_GRANT;
    grant.networkId = grant.sourceId = grant.bootId = grant.frameId = 1;
    grant.destinationId = kGroup;
    grant.scheduleVersion = kScheduleVersion;
    writePlan(grant, plan);
    uint8_t bytes[kMaxFrameSize]{};
    size_t length = 0;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(encode(grant, bytes, sizeof(bytes), length)));
    TEST_ASSERT_EQUAL_UINT8(5, bytes[2]);
    Frame decoded;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(decode(bytes, length, 1, 2, decoded)));
    Plan decodedPlan;
    TEST_ASSERT_TRUE(readPlan(decoded, decodedPlan));
    TEST_ASSERT_EQUAL_UINT8(9, decodedPlan.slots[0].sf);
    bytes[kHeaderSize + 6] = 10;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(decode(bytes, length, 1, 2, decoded)));
    bytes[kHeaderSize + 6] = 11;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::PAYLOAD), int(decode(bytes, length, 1, 2, decoded)));
    bytes[2] = 4;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::VERSION), int(decode(bytes, length, 1, 2, decoded)));
    for (auto& slot : plan.slots) slot = {1, 64, 9};
    TEST_ASSERT_FALSE(plan.matches(r));
}

void roaming_client_adapts_while_three_peers_deliver() {
    Network n(4, 8);
    n.rounds(3);
    TEST_ASSERT_EQUAL_UINT8(7, n.clients[0].link.telemetry().confirmedDataSf);
    n.minDataSf[0] = 9;
    n.rounds(7);
    TEST_ASSERT_TRUE(n.sent(1, 8));
    TEST_ASSERT_TRUE(n.sent(1, 9));
    TEST_ASSERT_EQUAL_UINT8(9, n.clients[0].link.telemetry().confirmedDataSf);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("sf=9,outcome=first,app=accepted"));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("next_sf=9,locked=1,adr=locked"));
    for (uint8_t i = 1; i < 4; ++i) {
        TEST_ASSERT_EQUAL_UINT(10, n.delivered[i]);
        TEST_ASSERT_FALSE(n.sent(i + 1, 8));
        TEST_ASSERT_FALSE(n.sent(i + 1, 9));
    }
    n.rounds(4);  // Strong SNR estimate alone does not prove a lower SF works.
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("adr=rollback"));
    n.minDataSf[0] = 7;
    n.rounds(20);
    TEST_ASSERT_EQUAL_UINT8(7, n.clients[0].link.telemetry().confirmedDataSf);
    for (uint8_t i = 1; i < 4; ++i) TEST_ASSERT_EQUAL_UINT(34, n.delivered[i]);
}

void missing_grant_ack_and_gateway_outage_leave_peers_safe() {
    Network n(2);
    n.rounds(1);
    n.dropGrant = true;
    n.rounds(1);
    TEST_ASSERT_EQUAL_UINT(1, n.delivered[0]);
    TEST_ASSERT_EQUAL_UINT(2, n.delivered[1]);
    n.dropGrant = false;
    n.dropFirstAck = true;
    n.rounds(1);
    TEST_ASSERT_EQUAL_UINT(2, n.delivered[0]);
    TEST_ASSERT_EQUAL_INT(int(PayloadResult::ACCEPTED), int(n.clients[0].payload.result()));
    for (const auto& packet : n.tx)
        if (packet.type == MessageType::DATA && packet.sender == 1)
            TEST_ASSERT_NOT_EQUAL(2u, packet.frame);
    n.outage = true;
    n.rounds(2);
    TEST_ASSERT_TRUE(n.clients[0].link.searchingForBeacon(n.now));
    TEST_ASSERT_EQUAL_UINT8(kControlSf, n.clients[0].radio.sf);
    n.outage = false;
    n.dropFirstAck = false;
    n.rounds(1);
    TEST_ASSERT_EQUAL_UINT(3, n.delivered[0]);
    TEST_ASSERT_EQUAL_UINT(6, n.delivered[1]);
}

void one_failed_retune_recovers_with_full_roster() {
    Network n(8);
    n.clients[0].radio.failNextRetune = true;
    n.rounds(2, 64);
    for (uint8_t i = 1; i < 8; ++i) TEST_ASSERT_EQUAL_UINT(2, n.delivered[i]);
    TEST_ASSERT_EQUAL_UINT(1, n.clients[0].link.telemetry().radioErrors);
    TEST_ASSERT_EQUAL_UINT(1, n.delivered[0]);
    TEST_ASSERT_EQUAL_UINT8(7, n.clients[0].link.telemetry().confirmedDataSf);
}

void higher_sf_reservations_protect_two_existing_clients() {
    Network n(8);
    n.minDataSf[0] = n.minDataSf[1] = 9;
    n.snr[0] = n.snr[1] = -12;
    n.rounds(7, 64);
    TEST_ASSERT_EQUAL_UINT8(9, n.clients[0].link.telemetry().confirmedDataSf);
    TEST_ASSERT_EQUAL_UINT8(9, n.clients[1].link.telemetry().confirmedDataSf);
    const auto firstDelivered = n.delivered[0];
    const auto secondDelivered = n.delivered[1];
    n.minDataSf[2] = 9;
    n.rounds(6, 64);
    TEST_ASSERT_FALSE(n.sent(3, 9));
    TEST_ASSERT_EQUAL_UINT(firstDelivered + 6, n.delivered[0]);
    TEST_ASSERT_EQUAL_UINT(secondDelivered + 6, n.delivered[1]);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("adr=budget_limited"));
    for (uint8_t i = 3; i < 8; ++i) TEST_ASSERT_EQUAL_UINT(13, n.delivered[i]);
}

void stable_lock_requires_client_ack_receipt() {
    Network n(1);
    n.dropAllAck = true;
    n.rounds(4);
    TEST_ASSERT_EQUAL_UINT(4, n.delivered[0]);
    TEST_ASSERT_EQUAL_UINT8(0, n.clients[0].link.telemetry().confirmedDataSf);
    TEST_ASSERT_EQUAL(std::string::npos, Serial.output.find("locked=1"));
    n.dropAllAck = false;
    n.rounds(2);
    TEST_ASSERT_EQUAL_UINT8(7, n.clients[0].link.telemetry().confirmedDataSf);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("next_sf=7,locked=1,adr=locked"));
}

void accepted_data_remains_observable_when_final_ack_completion_fails() {
    Network n(1);
    n.rounds(2);
    n.loseFinalTxDone = true;
    n.rounds(1);
    TEST_ASSERT_EQUAL_UINT(3, n.delivered[0]);
    TEST_ASSERT_EQUAL_INT(int(PayloadResult::ACCEPTED), int(n.clients[0].payload.result()));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("sf=7,locked=0,adr=timing_invalid"));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("timing=invalid,payload_bytes=8,rx_us="));
    TEST_ASSERT_EQUAL(std::string::npos, Serial.output.find("timing=invalid,payload_bytes=8,rx_us=N/A"));
    TEST_ASSERT_EQUAL(std::string::npos, Serial.output.find("locked=1"));

    n.loseFinalTxDone = false;
    n.rounds(1);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("next_sf=7,locked=1,adr=locked"));
}

void sf9_capacity_uses_the_production_calendar() {
    constexpr uint8_t nodeCounts[] = {4, 6, 8};
    constexpr uint8_t ceilings[] = {16, 24, 32, 64};
    constexpr uint8_t expected[][4] = {{4, 4, 4, 4}, {6, 6, 6, 3}, {8, 8, 5, 2}};
    for (unsigned row = 0; row < 3; ++row) {
        for (unsigned column = 0; column < 4; ++column) {
            const auto r = roster(nodeCounts[row], ceilings[column]);
            TEST_ASSERT_TRUE(r.valid(1));
            Plan plan;
            plan.count = r.count;
            for (uint8_t i = 0; i < plan.count; ++i) plan.slots[i] = {1, r.nodes[i].budget, 7};
            TEST_ASSERT_TRUE(plan.matches(r));
            uint8_t admitted = 0;
            for (uint8_t i = 0; i < plan.count; ++i) {
                plan.slots[i].sf = 9;
                if (!plan.matches(r)) break;
                ++admitted;
            }
            TEST_ASSERT_EQUAL_UINT8(expected[row][column], admitted);
        }
    }
}

void full_bench_roster_delivers_at_mixed_sf_with_32_byte_samples() {
    TEST_ASSERT_EQUAL_UINT8(32, kBenchPayloadBytes);
    Network n(kBenchRoster.count, kBenchRoster.count, kBenchPayloadBytes);
    for (uint8_t i = 0; i < n.count; ++i) {
        n.minDataSf[i] = i < 5 ? 9 : 7;
        n.snr[i] = i < 5 ? -9 : 6;
    }
    n.rounds(7, kBenchPayloadBytes);
    for (uint8_t i = 0; i < n.count; ++i) {
        TEST_ASSERT_EQUAL_UINT(i < 5 ? 3 : 7, n.delivered[i]);
        TEST_ASSERT_EQUAL_UINT8(n.minDataSf[i], n.clients[i].link.telemetry().confirmedDataSf);
    }
}

void bench_sf10_retries_keep_room_for_six_sf7_peers() {
    Network n(kBenchRoster.count, kBenchRoster.count, kBenchPayloadBytes);
    n.minDataSf[0] = n.minDataSf[1] = 10;
    n.snr[0] = n.snr[1] = -13;
    n.rounds(12, kBenchPayloadBytes);
    TEST_ASSERT_EQUAL_UINT8(10, n.clients[0].link.telemetry().confirmedDataSf);
    TEST_ASSERT_EQUAL_UINT8(10, n.clients[1].link.telemetry().confirmedDataSf);
    n.dropFirstData = true;
    n.rounds(1, kBenchPayloadBytes);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("sf=10,outcome=retry,app=accepted"));
    for (uint8_t i = 2; i < n.count; ++i) TEST_ASSERT_EQUAL_UINT(13, n.delivered[i]);

    n.minDataSf[2] = 10;
    n.rounds(8, kBenchPayloadBytes);
    TEST_ASSERT_FALSE(n.sent(3, 8));
    TEST_ASSERT_FALSE(n.sent(3, 10));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("adr=budget_limited"));
    for (uint8_t i = 3; i < n.count; ++i) TEST_ASSERT_EQUAL_UINT(21, n.delivered[i]);
}

void runtime_sends_exact_ascii_sample_and_reuses_bytes_on_retry() {
    FakeConfigStorage storage;
    FakeRadioHardware radio;
    const auto device = config(2);
    TEST_ASSERT_TRUE(encodeConfig(device, storage.bytes, sizeof(storage.bytes)));
    storage.length = kConfigBlobSize;
    FirmwareRuntime runtime(storage, radio, true);
    host_clock::set(0);
    runtime.setup();

    auto loop = [&](uint64_t now, uint32_t irq = 0) {
        host_clock::set(now);
        radio.resetTrace();
        radio.signal(irq, irq != 0);
        runtime.loop();
        radio.signal(0, false);
        TEST_ASSERT_FALSE(radio.violation);
    };
    auto receive = [&](Frame control, MessageType type, uint64_t end) {
        control.type = type;
        TEST_ASSERT_EQUAL_INT(int(CodecResult::OK),
            int(encode(control, radio.rxBytes, sizeof(radio.rxBytes), radio.packetLength)));
        loop(end, radio_irq::rxDone);
    };
    loop(0);
    Frame control;
    control.networkId = control.sourceId = control.bootId = control.frameId = 1;
    control.destinationId = kGroup;
    control.scheduleVersion = kScheduleVersion;
    for (uint32_t frameId = 1; frameId <= 2; ++frameId) {
        control.frameId = frameId;
        control.sequence = 0;
        writeRoster(control, kBenchRoster);
        const auto count = kBenchRoster.count;
        const uint64_t beaconEnd = uint64_t(frameId - 1) * kAdaptivePeriodUs + payloadAir(rosterBytes(count));
        receive(control, MessageType::BEACON, beaconEnd);
        const auto requestAt = beaconEnd + requestStart(0);
        loop(requestAt);
        Frame request;
        TEST_ASSERT_EQUAL_UINT(1, radio.count(HardwareCall::START_TX));
        TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(decode(radio.txBytes, radio.txLength, 1, 1, request)));
        TEST_ASSERT_EQUAL_INT(int(MessageType::PAYLOAD_REQUEST), int(request.type));
        TEST_ASSERT_EQUAL_UINT8(32, request.payload[4]);
        loop(requestAt + payloadAir(kRequestSize), radio_irq::txDone);

        Plan plan{count, {{request.bootId, 32, kControlSf}}};
        writePlan(control, plan);
        const auto grantEnd = beaconEnd + grantStart(count) + payloadAir(grantBytes(count));
        receive(control, MessageType::SLOT_GRANT, grantEnd);
        const auto dataAt = grantEnd + plan.dataStart(0, 0, 1);
        loop(dataAt);
        TEST_ASSERT_EQUAL_UINT(1, radio.count(HardwareCall::START_TX));
        TEST_ASSERT_EQUAL_UINT(64, radio.txLength);
        const auto* expected = frameId == 1 ? "ID=0002|SEQ=0001|MSG=HELLO WORLD" : "ID=0002|SEQ=0002|MSG=HELLO WORLD";
        TEST_ASSERT_EQUAL_MEMORY(expected, radio.txBytes + kHeaderSize + kPayloadPrefix, 32);
        uint8_t original[64];
        std::memcpy(original, radio.txBytes, sizeof(original));
        loop(dataAt + dataAir(32, kControlSf), radio_irq::txDone);

        control.payloadLength = feedbackBytes(count);
        std::memset(control.payload, 0, control.payloadLength);
        control.payload[0] = count;
        control.payload[2] = 1;
        receive(control, MessageType::ACK_BITMAP, grantEnd + plan.feedbackStart(0) + payloadAir(feedbackBytes(count)));
        const auto retryAt = grantEnd + plan.dataStart(1, 0, 1);
        loop(retryAt);
        TEST_ASSERT_EQUAL_UINT(1, radio.count(HardwareCall::START_TX));
        TEST_ASSERT_EQUAL_UINT(sizeof(original), radio.txLength);
        TEST_ASSERT_EQUAL_MEMORY(original, radio.txBytes, sizeof(original));
        loop(retryAt + dataAir(32, kControlSf), radio_irq::txDone);
        control.sequence = 1;
        control.payload[1] = 1;
        control.payload[2] = 0;
        put32(control.payload + 3, request.bootId);
        receive(control, MessageType::ACK_BITMAP, grantEnd + plan.feedbackStart(1) + payloadAir(feedbackBytes(count)));
        TEST_ASSERT_EQUAL_INT(int(PayloadResult::ACCEPTED), int(runtime.payloadResult()));
        loop(uint64_t(frameId) * kAdaptivePeriodUs);
    }
}

void display_reports_the_last_acknowledged_data_sf() {
    DisplaySnapshot snapshot;
    snapshot.config = config(2);
    snapshot.protocol = kWireVersion;
    snapshot.multiclient.confirmedDataSf = 8;
    const auto formatted = formatOledStatus(snapshot, 0, 1);
    TEST_ASSERT_EQUAL_STRING("ACKED DATA SF8", formatted.rows[6]);
}

void oled_tracks_actual_data_on_both_roles_even_without_ack() {
    Network n(1);
    DisplaySnapshot client, gateway;
    client.config = config(2);
    gateway.config = config(1);
    client.protocol = gateway.protocol = kWireVersion;
    TEST_ASSERT_EQUAL_STRING("1/3 TX -- CR4/5", formatOledStatus(client, 0, 0).rows[0]);
    n.dropGrant = true;
    n.rounds(1);
    client.multiclient = n.clients[0].link.telemetry();
    TEST_ASSERT_EQUAL_STRING("2/3 TX -- CR4/5", formatOledStatus(client, n.now, 1).rows[0]);
    n.dropGrant = false;
    n.dropAllAck = true;
    n.rounds(1);
    client.multiclient = n.clients[0].link.telemetry();
    gateway.multiclient = n.gateway.link.telemetry();
    TEST_ASSERT_EQUAL_STRING("2/3 TX SF7 CR4/5", formatOledStatus(client, n.now, 1).rows[0]);
    TEST_ASSERT_EQUAL_STRING("ACKED DATA SF N/A", formatOledStatus(client, n.now, 1).rows[6]);
    TEST_ASSERT_EQUAL_STRING("3/3 RX SF7 CR4/5", formatOledStatus(gateway, n.now, 2).rows[0]);
    n.dropAllAck = false;
    n.minDataSf[0] = 9;
    n.rounds(3);
    client.multiclient = n.clients[0].link.telemetry();
    gateway.multiclient = n.gateway.link.telemetry();
    TEST_ASSERT_EQUAL_STRING("1/3 TX SF8 CR4/5", formatOledStatus(client, n.now, 0).rows[0]);
    TEST_ASSERT_EQUAL_STRING("1/3 RX SF7 CR4/5", formatOledStatus(gateway, n.now, 0).rows[0]);
}

void gateway_stall_cannot_train_rf_loss_for_the_missed_slot() {
    Network n(2);
    n.stallFirstRx = true;
    n.rounds(4);
    TEST_ASSERT_EQUAL_UINT(4, n.delivered[0]);  // Recovered in the retry pool.
    TEST_ASSERT_EQUAL_UINT(4, n.delivered[1]);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("adr=rx_window_unobserved,timing=invalid"));
    TEST_ASSERT_FALSE(n.sent(1, 8));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("locked=1,adr=locked,timing=ok"));
}

void fixed_comparison_keeps_peers_and_feedback_on_the_same_calendar() {
    Network n(8, 8, 16, false);
    n.minDataSf[0] = 8;
    n.rounds(6, 16);
    TEST_ASSERT_EQUAL_UINT(0, n.delivered[0]);
    for (uint8_t i = 1; i < n.count; ++i) TEST_ASSERT_EQUAL_UINT(6, n.delivered[i]);
    TEST_ASSERT_GREATER_THAN_UINT32(0, n.clients[0].link.telemetry().retries);
    for (const auto& packet : n.tx)
        TEST_ASSERT_EQUAL_UINT8(packet.type == MessageType::DATA ? kBaseSf : kControlSf, packet.sf);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("adr=fixed_sf,timing=ok"));
}

void two_clients_use_sf10_with_sf9_control_and_retry() {
    Network n(8, 8, 16);
    n.minDataSf[0] = n.minDataSf[1] = 10;
    n.snr[0] = -8.6f;
    n.snr[1] = -13;
    n.rounds(12, 16);
    for (uint8_t sf = 7; sf <= 10; ++sf) TEST_ASSERT_TRUE(n.sent(1, sf));
    TEST_ASSERT_EQUAL_UINT8(10, n.clients[0].link.telemetry().confirmedDataSf);
    TEST_ASSERT_EQUAL_UINT8(10, n.clients[1].link.telemetry().confirmedDataSf);
    DisplaySnapshot screen;
    screen.config = config(2);
    screen.protocol = kWireVersion;
    screen.multiclient = n.clients[0].link.telemetry();
    const auto page = formatOledStatus(screen, n.now, 1);
    TEST_ASSERT_EQUAL_STRING("2/3 TX SF10 CR4/5", page.rows[0]);
    TEST_ASSERT_EQUAL_STRING("ACKED DATA SF10", page.rows[6]);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("next_sf=10,locked=1,adr=locked"));
    TEST_ASSERT_EQUAL(std::string::npos, Serial.output.find("adr=probe_lower"));

    const auto retriesBefore = n.clients[0].link.telemetry().retries;
    n.dropFirstData = true;
    n.rounds(1, 16);
    TEST_ASSERT_EQUAL_UINT8(10, n.clients[0].link.telemetry().confirmedDataSf);
    TEST_ASSERT_EQUAL_UINT(retriesBefore + 1, n.clients[0].link.telemetry().retries);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("sf=10,outcome=retry,app=accepted"));

    n.minDataSf[2] = 10;
    n.rounds(8, 16);
    TEST_ASSERT_TRUE(n.sent(3, 9));
    TEST_ASSERT_FALSE(n.sent(3, 10));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("adr=budget_limited"));
    for (uint8_t i = 3; i < n.count; ++i) TEST_ASSERT_EQUAL_UINT(21, n.delivered[i]);

    n.minDataSf[0] = 9;
    n.snr[0] = -7;
    n.rounds(18, 16);
    TEST_ASSERT_EQUAL_UINT8(9, n.clients[0].link.telemetry().confirmedDataSf);
    TEST_ASSERT_EQUAL_UINT8(10, n.clients[2].link.telemetry().confirmedDataSf);
    for (uint8_t i = 3; i < n.count; ++i) TEST_ASSERT_EQUAL_UINT(39, n.delivered[i]);
}

void sf10_client_stops_data_when_control_is_lost() {
    Network n(2, 8, 16);
    n.minDataSf[0] = 10;
    n.snr[0] = -10;
    n.rounds(9, 16);
    TEST_ASSERT_EQUAL_UINT8(10, n.clients[0].link.telemetry().confirmedDataSf);
    const auto delivered = n.delivered[0];
    n.dropControl = true;
    n.rounds(5, 16);  // Longer than the inactive airtime reservation.
    TEST_ASSERT_EQUAL_UINT(delivered, n.delivered[0]);
    TEST_ASSERT_TRUE(n.clients[0].link.searchingForBeacon(n.now));
    TEST_ASSERT_EQUAL_UINT8(kControlSf, n.clients[0].radio.sf);
    n.dropControl = false;
    n.rounds(1, 16);
    TEST_ASSERT_EQUAL_UINT(delivered + 1, n.delivered[0]);
    TEST_ASSERT_EQUAL_UINT(15, n.delivered[1]);
}

void returning_client_does_not_displace_other_sf10_reservations() {
    Network n(8, 8, 16);
    n.minDataSf[0] = n.minDataSf[1] = 10;
    n.snr[0] = n.snr[1] = n.snr[2] = -12;
    n.rounds(10, 16);
    TEST_ASSERT_EQUAL_UINT8(10, n.clients[0].link.telemetry().confirmedDataSf);

    n.dropControl = true;
    n.minDataSf[2] = 10;
    n.rounds(9, 16);
    TEST_ASSERT_EQUAL_UINT8(10, n.clients[2].link.telemetry().confirmedDataSf);

    n.dropControl = false;
    const auto start = n.tx.size();
    const auto previouslyDelivered = n.delivered[0];
    n.rounds(1, 16);
    TEST_ASSERT_EQUAL_UINT(previouslyDelivered, n.delivered[0]);
    bool attempted = false;
    for (size_t i = start; i < n.tx.size(); ++i) {
        const auto& tx = n.tx[i];
        if (tx.type != MessageType::DATA || tx.sender != 1) continue;
        TEST_ASSERT_EQUAL_UINT8(9, tx.sf);
        attempted = true;
    }
    TEST_ASSERT_TRUE(attempted);
    TEST_ASSERT_EQUAL_UINT8(10, n.clients[1].link.telemetry().confirmedDataSf);
    TEST_ASSERT_EQUAL_UINT8(10, n.clients[2].link.telemetry().confirmedDataSf);
    for (uint8_t i = 3; i < n.count; ++i) TEST_ASSERT_EQUAL_UINT(20, n.delivered[i]);
}

void lower_sf_probe_expires_without_a_matching_ack_receipt() {
    Network n(1);
    n.minDataSf[0] = 8;
    n.snr[0] = -6;
    n.rounds(8);
    TEST_ASSERT_EQUAL_UINT8(8, n.clients[0].link.telemetry().confirmedDataSf);
    n.minDataSf[0] = 7;
    n.snr[0] = 5;
    for (unsigned i = 0; i < 12 && Serial.output.find("adr=probe_lower") == std::string::npos; ++i)
        n.rounds(1);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("adr=probe_lower"));
    n.dropAllAck = true;
    n.rounds(7);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("adr=probe_no_receipt"));
    TEST_ASSERT_EQUAL_UINT8(8, n.clients[0].link.telemetry().lastDataSf);
    n.dropAllAck = false;
    n.rounds(1);
    TEST_ASSERT_EQUAL_UINT8(8, n.clients[0].link.telemetry().confirmedDataSf);
}

void intermittent_first_attempt_loss_still_requests_a_higher_sf() {
    Network n(1);
    n.snr[0] = -9;
    n.rounds(4);
    n.dropFirstData = true;
    n.rounds(2);  // Retry success followed by a clean first attempt.
    TEST_ASSERT_FALSE(n.sent(1, 8));
    n.dropFirstData = true;
    n.rounds(2);
    TEST_ASSERT_TRUE(n.sent(1, 8));
    TEST_ASSERT_EQUAL_UINT(8, n.delivered[0]);
}

void outage_during_a_downshift_trial_recovers_the_previous_sf() {
    Network n(1);
    n.minDataSf[0] = 8;
    n.snr[0] = -6;
    n.rounds(8);
    TEST_ASSERT_EQUAL_UINT8(8, n.clients[0].link.telemetry().confirmedDataSf);
    n.minDataSf[0] = 7;
    n.snr[0] = 5;
    for (unsigned i = 0; i < 12 && Serial.output.find("adr=probe_lower") == std::string::npos; ++i)
        n.rounds(1);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("adr=probe_lower"));
    n.rounds(1);
    TEST_ASSERT_EQUAL_UINT8(7, n.clients[0].link.telemetry().confirmedDataSf);
    n.dropControl = true;
    n.minDataSf[0] = 8;
    const auto delivered = n.delivered[0];
    n.rounds(5);
    n.dropControl = false;
    n.rounds(1);
    TEST_ASSERT_EQUAL_UINT(delivered + 1, n.delivered[0]);
    TEST_ASSERT_EQUAL_UINT8(8, n.clients[0].link.telemetry().confirmedDataSf);
}

void first_request_after_outage_can_confirm_the_last_delivery() {
    Network n(2);
    n.minDataSf[0] = 8;
    n.rounds(3);  // The first SF8 DATA is ACKed just before the control outage.
    TEST_ASSERT_EQUAL_UINT(1, n.delivered[0]);
    TEST_ASSERT_EQUAL_UINT8(8, n.clients[0].link.telemetry().confirmedDataSf);
    n.dropControl = true;
    n.rounds(5);  // The other Client stays active while the Gateway frees the reservation.
    n.dropControl = false;
    n.rounds(1);
    TEST_ASSERT_EQUAL_UINT(2, n.delivered[0]);
    TEST_ASSERT_EQUAL_UINT8(8, n.clients[0].link.telemetry().confirmedDataSf);
    TEST_ASSERT_EQUAL_UINT(9, n.delivered[1]);
}

void display_distinguishes_grant_timeout_from_beacon_loss_and_ack_loss() {
    Network n(1);
    DisplaySnapshot snapshot;
    snapshot.config = config(2);
    snapshot.protocol = kWireVersion;
    n.rounds(1);
    n.dropGrant = true;
    n.rounds(1);
    snapshot.multiclient = n.clients[0].link.telemetry();
    TEST_ASSERT_EQUAL_STRING("CLIENT 2 NO GRANT", formatOledStatus(snapshot, n.now, 1).rows[1]);
    n.dropGrant = false;
    n.dropAllAck = true;
    n.rounds(1);
    snapshot.multiclient = n.clients[0].link.telemetry();
    TEST_ASSERT_EQUAL_STRING("CLIENT 2 DATA", formatOledStatus(snapshot, n.now, 1).rows[1]);
    TEST_ASSERT_EQUAL_STRING("APP UNKNOWN", formatOledStatus(snapshot, n.now, 1).rows[5]);
    n.dropControl = true;
    n.rounds(1);
    snapshot.searchingForBeacon = n.clients[0].link.searchingForBeacon(n.now);
    TEST_ASSERT_EQUAL_STRING("CLIENT 2 SEARCH", formatOledStatus(snapshot, n.now, 1).rows[1]);
}

void display_identifies_local_failures_before_request_and_data() {
    Network n(1);
    DisplaySnapshot snapshot;
    snapshot.config = config(2);
    snapshot.protocol = kWireVersion;
    n.rounds(1);
    n.clients[0].radio.failNextTx = true;
    n.rounds(1);
    snapshot.multiclient = n.clients[0].link.telemetry();
    TEST_ASSERT_EQUAL_STRING("CLIENT 2 REQ FAIL", formatOledStatus(snapshot, n.now, 1).rows[1]);
    n.failDataLaunch = true;
    n.rounds(1);
    snapshot.multiclient = n.clients[0].link.telemetry();
    TEST_ASSERT_EQUAL_STRING("CLIENT 2 NO TX", formatOledStatus(snapshot, n.now, 1).rows[1]);
    TEST_ASSERT_EQUAL_UINT(1, n.delivered[0]);
    n.failDataLaunch = false;
    n.rounds(1);
    snapshot.multiclient = n.clients[0].link.telemetry();
    TEST_ASSERT_EQUAL_STRING("CLIENT 2 DATA", formatOledStatus(snapshot, n.now, 1).rows[1]);
    TEST_ASSERT_EQUAL_UINT(2, n.delivered[0]);
}

void sf10_airtime_fits_driver_tx_deadline() {
    TEST_ASSERT_EQUAL_UINT32(575488, dataAir(16, 10));
    TEST_ASSERT_EQUAL_UINT32(985088, dataAir(kMaxData, 10));
    TEST_ASSERT_TRUE(dataAir(kMaxData, 10) + kSlotGuardUs < kTxTimeoutUs);
}

void driver_controls_adapter_retune_and_failure() {
    auto& physical = host_adapter::state();
    physical = {};
    Sx1262Hardware hardware;
    RadioDriver driver(hardware);
    TEST_ASSERT_EQUAL_INT16(0, driver.begin());
    TEST_ASSERT_EQUAL_INT16(0, driver.receive());
    TEST_ASSERT_EQUAL_INT16(0, driver.setSpreadingFactor(10));
    TEST_ASSERT_EQUAL_INT(host_adapter::STANDBY, physical.trace[physical.traceLength - 2]);
    TEST_ASSERT_EQUAL_INT(host_adapter::RETUNE, physical.trace[physical.traceLength - 1]);
    TEST_ASSERT_EQUAL_UINT8(10, physical.sf);
    TEST_ASSERT_FALSE(driver.receiving());
    TEST_ASSERT_EQUAL_INT16(0, driver.receive());
    const auto beforeInvalid = physical.calls[host_adapter::RETUNE];
    TEST_ASSERT_NOT_EQUAL(0, driver.setSpreadingFactor(11));
    TEST_ASSERT_EQUAL_UINT(beforeInvalid, physical.calls[host_adapter::RETUNE]);
    TEST_ASSERT_TRUE(driver.receiving());
    physical.codes[host_adapter::RETUNE] = -88;
    TEST_ASSERT_EQUAL_INT16(-88, driver.setSpreadingFactor(8));
    TEST_ASSERT_FALSE(driver.healthy());
    physical.codes[host_adapter::RETUNE] = 0;
    TEST_ASSERT_EQUAL_INT16(0, driver.setSpreadingFactor(9));
    const uint8_t packet[kHeaderSize]{};
    TEST_ASSERT_EQUAL_INT16(0, driver.transmit(packet, sizeof(packet), 0));
    const auto calls = physical.calls[host_adapter::RETUNE];
    TEST_ASSERT_NOT_EQUAL(0, driver.setSpreadingFactor(7));
    TEST_ASSERT_EQUAL_UINT(calls, physical.calls[host_adapter::RETUNE]);
}
}

void setUp() { Serial.output.clear(); }
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(mixed_profile_codec_and_budget);
    RUN_TEST(roaming_client_adapts_while_three_peers_deliver);
    RUN_TEST(missing_grant_ack_and_gateway_outage_leave_peers_safe);
    RUN_TEST(one_failed_retune_recovers_with_full_roster);
    RUN_TEST(higher_sf_reservations_protect_two_existing_clients);
    RUN_TEST(stable_lock_requires_client_ack_receipt);
    RUN_TEST(accepted_data_remains_observable_when_final_ack_completion_fails);
    RUN_TEST(sf9_capacity_uses_the_production_calendar);
    RUN_TEST(full_bench_roster_delivers_at_mixed_sf_with_32_byte_samples);
    RUN_TEST(bench_sf10_retries_keep_room_for_six_sf7_peers);
    RUN_TEST(runtime_sends_exact_ascii_sample_and_reuses_bytes_on_retry);
    RUN_TEST(display_reports_the_last_acknowledged_data_sf);
    RUN_TEST(oled_tracks_actual_data_on_both_roles_even_without_ack);
    RUN_TEST(gateway_stall_cannot_train_rf_loss_for_the_missed_slot);
    RUN_TEST(fixed_comparison_keeps_peers_and_feedback_on_the_same_calendar);
    RUN_TEST(two_clients_use_sf10_with_sf9_control_and_retry);
    RUN_TEST(sf10_client_stops_data_when_control_is_lost);
    RUN_TEST(returning_client_does_not_displace_other_sf10_reservations);
    RUN_TEST(lower_sf_probe_expires_without_a_matching_ack_receipt);
    RUN_TEST(intermittent_first_attempt_loss_still_requests_a_higher_sf);
    RUN_TEST(outage_during_a_downshift_trial_recovers_the_previous_sf);
    RUN_TEST(first_request_after_outage_can_confirm_the_last_delivery);
    RUN_TEST(display_distinguishes_grant_timeout_from_beacon_loss_and_ack_loss);
    RUN_TEST(display_identifies_local_failures_before_request_and_data);
    RUN_TEST(sf10_airtime_fits_driver_tx_deadline);
    RUN_TEST(driver_controls_adapter_retune_and_failure);
    return UNITY_END();
}
