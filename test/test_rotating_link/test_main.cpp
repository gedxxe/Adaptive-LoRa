// SPDX-License-Identifier: GPL-3.0-only
#include <Arduino.h>
#include <unity.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include "app/multiclient_link.h"
#include "common/diagnostics/oled_status_display.h"
#include "../support/fake_radio_transport.h"

using namespace adaptive_lora;
using namespace adaptive_lora::multiclient;
using namespace adaptive_lora::test;

namespace {
uint32_t newBoot(uint32_t previous) { return previous + 1; }

DeviceConfig config(uint16_t id) {
    DeviceConfig c;
    c.role = id == 1 ? DeviceRole::GATEWAY : DeviceRole::CLIENT;
    c.nodeId = id;
    return c;
}

void rotation_covers_each_population_once_at_each_sf() {
    const uint16_t populations[] = {1, 4, 8, 16, 32, 64, 70, 84, 128};
    for (const auto count : populations) {
        for (uint8_t sf = kBaseSf; sf <= phy::maxDataSf; ++sf) {
            std::array<uint8_t, kPopulationLimit> profiles;
            profiles.fill(sf);
            Rotation rotation{count};
            unsigned visits = 0;
            unsigned frames = 0;
            do {
                const auto group = rotation.select(17, 32, profiles.data());
                TEST_ASSERT_TRUE(group.valid(17));
                Plan ceiling;
                ceiling.count = group.count;
                for (uint8_t i = 0; i < group.count; ++i) {
                    TEST_ASSERT_EQUAL_UINT16(registeredId(visits, 17), group.nodes[i].id);
                    TEST_ASSERT_EQUAL_UINT16(visits, registryPosition(group.nodes[i].id, 17));
                    TEST_ASSERT_EQUAL_UINT8(32, group.nodes[i].budget);
                    ceiling.slots[i] = {1, group.nodes[i].budget, sf};
                    ++visits;
                }
                TEST_ASSERT_TRUE(ceiling.matches(group));
                ++frames;
            } while (rotation.next);
            TEST_ASSERT_EQUAL_UINT(count, visits);
            std::printf("CAPACITY,n=%u,sf=%u,frames=%u,period_s=%lu\n", unsigned(count), unsigned(sf), frames,
                        static_cast<unsigned long>(frames) * kAdaptivePeriodUs / 1000000u);
        }
    }
}

void expensive_group_leaves_next_client_at_cursor() {
    std::array<uint8_t, kPopulationLimit> profiles;
    profiles.fill(phy::maxDataSf);
    Rotation rotation{32};
    const auto group = rotation.select(1, kMaxData, profiles.data());
    TEST_ASSERT_TRUE(group.count > 0 && group.count < kMaxClients);
    TEST_ASSERT_EQUAL_UINT(group.count, rotation.next);
    profiles[rotation.next] = kBaseSf;
    TEST_ASSERT_EQUAL_UINT16(group.count + 2, rotation.select(1, kMaxData, profiles.data()).nodes[0].id);
}

void estimate_authenticated_airtime_with_full_retry_reserve() {
    // Analytical sensitivity only: the rotating firmware has no authenticated wire format.
    constexpr uint8_t tag = 16;
    for (const auto signature : {false, true}) {
        for (const auto length : {8, 32, 64}) {
            for (uint8_t sf = kBaseSf; sf <= phy::maxDataSf; ++sf) {
                unsigned maxGroup = 0;
                for (uint8_t count = 1; count <= kMaxClients; ++count) {
                    const uint8_t envelope = signature ? 64 : tag * count;
                    const auto controlDelta = [envelope](uint8_t body) {
                        return airAtSf(kHeaderSize + body + envelope, kControlSf) - payloadAir(body);
                    };
                    Plan plain;
                    plain.count = count;
                    for (uint8_t i = 0; i < count; ++i) plain.slots[i] = {1, uint8_t(length), sf};
                    const auto dataDelta = airAtSf(kHeaderSize + kPayloadPrefix + length + tag, sf) -
                        dataAir(length, sf);
                    const auto requestDelta = airAtSf(kHeaderSize + kRequestSize + tag, kControlSf) - payloadAir(kRequestSize);
                    const auto used = reservationUs(plain) + kAdaptiveTailUs +
                        controlDelta(rosterBytes(count)) + controlDelta(grantBytes(count)) +
                        kAttempts * controlDelta(feedbackBytes(count)) + count * requestDelta +
                        (count + (kAttempts - 1) * poolLimit(count)) * dataDelta;
                    if (used <= kAdaptivePeriodUs) maxGroup = count;
                }
                TEST_ASSERT_GREATER_THAN_UINT(0, maxGroup);
                std::printf("AUTH_PROJECTION,mechanism=%s,bytes=%d,sf=%u,group=%u\n",
                            signature ? "signature64" : "per_client_tag16", length, unsigned(sf), maxGroup);
            }
        }
    }
}

struct Peer {
    FakeRadioTransport radio;
    SerialLogger log;
    PayloadPort payload;
    MulticlientLink link;

    Peer(uint16_t id, PayloadPort::Receiver receiver = nullptr, void* context = nullptr)
        : payload(receiver, context), link(config(id), radio, log, payload, newBoot, kPopulationLimit, 32) {
        log.begin(config(id));
    }
};

struct Network {
    enum { GATEWAY, FIRST, NEXT, LAST, SIZE };
    unsigned deliveries[SIZE]{};
    unsigned firstMaxSf = 0;
    unsigned nextMaxSf = 0;
    bool weakenFirst = false;
    uint32_t dropBeaconFrame = 0;
    uint32_t dropAckFrame = 0;
    Peer gateway{1, accept, this};
    Peer first{2};
    Peer next{10};
    Peer last{129};
    Peer* peers[SIZE]{&gateway, &first, &next, &last};
    uint64_t now = 0;
    uint64_t txEnd = 0;
    unsigned listening = 0;
    int sender = -1;
    Frame packet;

    static bool accept(void* context, const PayloadDelivery& data) {
        auto& n = *static_cast<Network*>(context);
        const auto ix = data.source == 2 ? FIRST : data.source == 10 ? NEXT : LAST;
        TEST_ASSERT_EQUAL_UINT8(32, data.size);
        TEST_ASSERT_EQUAL_UINT8(0x37, data.bytes[0]);
        TEST_ASSERT_EQUAL_UINT16(uint16_t(data.frame), data.sequence);
        ++n.deliveries[ix];
        return true;
    }
    void submit() {
        uint8_t bytes[32]{0x37};
        for (auto peer : {&first, &next, &last})
            TEST_ASSERT_EQUAL_INT(int(SubmitResult::QUEUED), int(peer->payload.submit(bytes, sizeof(bytes))));
    }
    void tick() {
        RadioEvent events[SIZE];
        if (sender >= 0 && now >= txEnd) {
            auto& tx = peers[sender]->radio;
            events[sender] = tx.completeTx();
            for (int i = 0; i < SIZE; ++i) {
                auto& rx = peers[i]->radio;
                if (!(listening & (1u << i)) || rx.sf != tx.sf || !rx.receiving()) continue;
                if (packet.type == MessageType::BEACON && packet.frameId == dropBeaconFrame) continue;
                if (packet.type == MessageType::ACK_BITMAP && packet.frameId == dropAckFrame) continue;
                if (packet.type == MessageType::DATA && sender == FIRST && weakenFirst && tx.sf == kBaseSf)
                    continue;
                if (packet.destinationId == kGroup || i == GATEWAY) events[i] = rx.deliver(tx.lastTx);
                if (events[i].type == RadioEventType::RX_DONE) {
                    events[i].metrics = true;
                    events[i].snr = sender == FIRST && weakenFirst ? -12.0f : 3.0f;
                }
            }
            for (auto& event : events) {
                event.hasCompletionTime = true;
                event.completionUs = txEnd;
            }
            sender = -1;
        }
        host_clock::set(now);
        for (int i = 0; i < SIZE; ++i) peers[i]->link.update(now, events[i]);
        for (int i = 0; i < SIZE; ++i) {
            auto& radio = peers[i]->radio;
            TEST_ASSERT_EQUAL_UINT(0, radio.contractViolations);
            if (!radio.transmitting() || i == sender) continue;
            TEST_ASSERT_EQUAL_INT_MESSAGE(-1, sender, "overlapping slots");
            sender = i;
            const auto& tx = radio.lastTx;
            TEST_ASSERT_EQUAL_INT(int(CodecResult::OK),
                int(decode(tx.bytes, tx.length, 1, get16(tx.bytes + 10), packet)));
            txEnd = now + airAtSf(tx.length, radio.sf);
            if (packet.type == MessageType::DATA && i == FIRST) firstMaxSf = std::max(firstMaxSf, unsigned(radio.sf));
            if (packet.type == MessageType::DATA && i == NEXT) nextMaxSf = std::max(nextMaxSf, unsigned(radio.sf));
            listening = 0;
            for (int j = 0; j < SIZE; ++j)
                if (peers[j]->radio.receiving() && peers[j]->radio.sf == radio.sf) listening |= (1u << j);
        }
        for (auto peer : peers)
            while (!peer->log.empty()) peer->log.drain();
        Serial.output.clear();
    }
    void frames(unsigned count) {
        const auto end = now + uint64_t(count) * kAdaptivePeriodUs;
        for (; now < end; now += 2000) tick();
    }
};

void registered_client_129_queues_until_its_slot() {
    Network n;
    n.submit();
    n.frames(1);
    TEST_ASSERT_TRUE(n.last.payload.pending());
    TEST_ASSERT_EQUAL_UINT(0, n.last.radio.successfulTxStarts);
    DisplaySnapshot display;
    display.config = config(129);
    display.protocol = kWireVersion;
    display.multiclient = n.last.link.telemetry();
    TEST_ASSERT_EQUAL_STRING("CLIENT 129 GROUP WAIT", formatOledStatus(display, n.now - 2000).rows[1]);
    TEST_ASSERT_EQUAL_STRING("BEACON SYNC", formatOledStatus(display, n.now - 2000).rows[2]);
    display.searchingForBeacon = true;
    TEST_ASSERT_EQUAL_STRING("CLIENT 129 SEARCH", formatOledStatus(display, n.now + kAdaptivePeriodUs).rows[1]);
    n.frames(15);
    TEST_ASSERT_EQUAL_UINT(1, n.deliveries[Network::FIRST]);
    TEST_ASSERT_EQUAL_UINT(1, n.deliveries[Network::NEXT]);
    TEST_ASSERT_EQUAL_UINT(1, n.deliveries[Network::LAST]);
    TEST_ASSERT_EQUAL_INT(int(PayloadResult::ACCEPTED), int(n.last.payload.result()));
}

void rejects_noncontiguous_groups_and_incompatible_wire_version() {
    Peer client(2);
    Frame beacon;
    beacon.type = MessageType::BEACON;
    beacon.networkId = 1;
    beacon.sourceId = 1;
    beacon.destinationId = kGroup;
    beacon.bootId = 1;
    beacon.frameId = 1;
    beacon.scheduleVersion = kScheduleVersion;
    const Roster malformed{kAdaptivePeriodUs, 2, {{2, 32}, {4, 32}}};
    writeRoster(beacon, malformed);
    CapturedPacket bytes;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(encode(beacon, bytes.bytes, sizeof(bytes.bytes), bytes.length)));
    client.link.update(0, {});
    client.link.update(1, client.radio.deliver(bytes));
    TEST_ASSERT_EQUAL_UINT(0, client.link.telemetry().frame);
    TEST_ASSERT_TRUE(client.link.searchingForBeacon(1));
    const Roster valid{kAdaptivePeriodUs, 2, {{2, 32}, {3, 32}}};
    writeRoster(beacon, valid);
    TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(encode(beacon, bytes.bytes, sizeof(bytes.bytes), bytes.length)));
    bytes.bytes[2] = 5;
    client.link.update(2, client.radio.deliver(bytes));
    TEST_ASSERT_EQUAL_UINT(0, client.link.telemetry().frame);
}

void missed_group_beacon_recovers_next_sweep() {
    Network n;
    n.dropBeaconFrame = 1;
    n.submit();
    n.frames(16);
    TEST_ASSERT_EQUAL_UINT(0, n.first.radio.successfulTxStarts);
    TEST_ASSERT_TRUE(n.first.payload.pending());
    n.frames(1);
    TEST_ASSERT_EQUAL_UINT(1, n.deliveries[Network::FIRST]);
}

void retry_after_lost_ack_is_not_delivered_twice() {
    Network n;
    n.dropAckFrame = 1;
    n.submit();
    n.frames(1);
    TEST_ASSERT_EQUAL_UINT(1, n.deliveries[Network::FIRST]);
    TEST_ASSERT_EQUAL_INT(int(PayloadResult::UNKNOWN), int(n.first.payload.result()));
}

void adr_state_survives_other_groups_without_affecting_peers() {
    Network n;
    n.weakenFirst = true;
    for (int sweep = 0; sweep < 4; ++sweep) {
        n.submit();
        n.frames(16);
    }
    TEST_ASSERT_GREATER_THAN_UINT(kBaseSf, n.firstMaxSf);
    TEST_ASSERT_EQUAL_UINT(kBaseSf, n.nextMaxSf);
    TEST_ASSERT_GREATER_THAN_UINT(0, n.deliveries[Network::FIRST]);
}
}

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(rotation_covers_each_population_once_at_each_sf);
    RUN_TEST(expensive_group_leaves_next_client_at_cursor);
    RUN_TEST(estimate_authenticated_airtime_with_full_retry_reserve);
    RUN_TEST(registered_client_129_queues_until_its_slot);
    RUN_TEST(rejects_noncontiguous_groups_and_incompatible_wire_version);
    RUN_TEST(missed_group_beacon_recovers_next_sweep);
    RUN_TEST(retry_after_lost_ack_is_not_delivered_twice);
    RUN_TEST(adr_state_survives_other_groups_without_affecting_peers);
    return UNITY_END();
}
