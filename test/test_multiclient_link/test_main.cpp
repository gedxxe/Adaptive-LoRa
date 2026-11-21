#include <unity.h>
#include <Arduino.h>
#include <functional>
#include <vector>
#include "app/multiclient_link.h"
#include "../support/fake_radio_transport.h"

using namespace adaptive_lora;
using namespace adaptive_lora::multiclient;
using namespace adaptive_lora::test;

namespace {
uint32_t identity;
uint32_t boot(uint32_t) { return ++identity; }
DeviceConfig config(uint16_t id) {
    DeviceConfig c;
    c.role = id == 1 ? DeviceRole::GATEWAY : DeviceRole::CLIENT;
    c.nodeId = id;
    return c;
}
Roster roster(uint8_t count) {
    Roster result{10000000, count, {}};
    for (uint8_t i = 0; i < count; ++i) result.nodes[i] = {uint16_t(i + 2), 64};
    return result;
}
struct Peer {
    FakeRadioTransport radio;
    SerialLogger logger;
    PayloadPort payload;
    MulticlientLink link;
    MulticlientLink* active = &link;

    Peer(uint16_t id, Roster schedule = {}, PayloadPort::Receiver receive = nullptr, void* context = nullptr)
        : payload(receive, context), link(config(id), radio, logger, payload, boot, schedule) { logger.begin(config(id)); }
};
struct Delivered { uint16_t id; std::vector<uint8_t> bytes; };
struct AirFrame { Frame frame; CapturedPacket packet; uint64_t start; uint64_t end; };
struct Network {
    std::vector<Delivered> deliveries;
    std::vector<AirFrame> air;
    std::function<bool(uint8_t receiver, Frame& frame)> filter;
    bool accept = true;
    unsigned callbacks = 0;
    uint8_t count;
    Peer gateway;
    Peer clients[8]{{2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}};
    Peer* sender = nullptr;
    uint64_t now;
    uint64_t end = 0;

    static bool receive(void* context, const PayloadDelivery& packet) {
        auto& n = *static_cast<Network*>(context);
        ++n.callbacks;
        if (!n.accept) return false;
        TEST_ASSERT_EQUAL_UINT16(uint16_t(packet.frame), packet.sequence);
        n.deliveries.push_back({packet.source, {packet.bytes, packet.bytes + packet.size}});
        return true;
    }
    explicit Network(uint8_t n, Roster schedule, uint64_t start = 0)
        : count(n), gateway(1, schedule, receive, this), now(start) {}
    explicit Network(uint8_t n) : Network(n, roster(n)) {}
    Peer& peer(uint8_t i) { return i ? clients[i - 1] : gateway; }
    void submit(uint8_t i, const std::vector<uint8_t>& bytes) {
        TEST_ASSERT_EQUAL_INT(int(SubmitResult::QUEUED), int(clients[i].payload.submit(bytes.data(), bytes.size())));
    }
    void tick() {
        RadioEvent events[9];
        if (sender && now >= end) {
            events[0] = {};
            for (uint8_t i = 0; i <= count; ++i) {
                auto& p = peer(i);
                if (&p == sender) { events[i] = p.radio.completeTx(); continue; }
                if (!p.radio.receiving()) continue;
                auto f = air.back().frame;
                if (f.destinationId != kGroup && f.destinationId != i + 1) continue;
                if (filter && !filter(i, f)) continue;
                CapturedPacket packet;
                TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(encode(f, packet.bytes, sizeof(packet.bytes), packet.length)));
                events[i] = p.radio.deliver(packet);
            }
            sender = nullptr;
        }
        host_clock::set(now);
        for (uint8_t i = 0; i <= count; ++i) {
            auto& p = peer(i);
            p.active->update(now, events[i]);
            if (p.radio.transmitting() && sender != &p) {
                TEST_ASSERT_NULL_MESSAGE(sender, "Two radios transmitted in the same serialized slot");
                sender = &p;
                const auto& packet = sender->radio.lastTx;
                Frame f;
                const uint16_t dest = get16(packet.bytes + 10);
                TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(decode(packet.bytes, packet.length, 1, dest, f)));
                end = now + airtimeUs(packet.length);
                air.push_back({f, packet, now, end});
            }
            TEST_ASSERT_EQUAL_UINT(0, p.radio.contractViolations);
            for (unsigned drain = 0; drain < 24 && !p.logger.empty(); ++drain) p.logger.drain();
        }
    }
    void run(uint64_t duration) {
        const uint64_t stop = now + duration;
        for (; now < stop; now += 1000) tick();
    }
    std::vector<AirFrame> packets(MessageType type, uint16_t id = 0) const {
        std::vector<AirFrame> result;
        for (const auto& x : air) if (x.frame.type == type && (!id || x.frame.sourceId == id)) result.push_back(x);
        return result;
    }
    bool hasLog(const char* text) const { return Serial.output.find(text) != std::string::npos; }
};
std::vector<uint8_t> bytes(size_t length) {
    std::vector<uint8_t> result(length);
    for (size_t i = 0; i < length; ++i) result[i] = uint8_t(i * 51);
    return result;
}

void runClients(uint8_t count) {
    Network n(count);
    uint8_t lengths[]{0, 3, 8, 34, 64, 9, 46, 62};
    if (count == kMaxClients)
        for (auto& length : lengths) length = kMaxData;
    for (uint8_t i = 0; i < count; ++i) n.submit(i, bytes(lengths[i]));
    n.run(10000000);
    TEST_ASSERT_EQUAL_UINT(count, n.deliveries.size());
    const auto grants = n.packets(MessageType::SLOT_GRANT);
    TEST_ASSERT_EQUAL_UINT(1, grants.size());
    Plan plan;
    TEST_ASSERT_TRUE(readPlan(grants[0].frame, plan));
    const auto feedback = n.packets(MessageType::ACK_BITMAP);
    TEST_ASSERT_EQUAL_UINT(4, feedback.size());
    for (uint8_t i = 0; i < count; ++i) {
        TEST_ASSERT_EQUAL_UINT(i + 2, n.deliveries[i].id);
        TEST_ASSERT_TRUE(bytes(lengths[i]) == n.deliveries[i].bytes);
        TEST_ASSERT_EQUAL_UINT8(lengths[i], plan.slots[i].bytes);
        TEST_ASSERT_EQUAL_INT(int(PayloadResult::ACCEPTED), int(n.clients[i].payload.result()));
    }
    TEST_ASSERT_EQUAL_UINT8(uint8_t((1u << count) - 1), feedback[0].frame.payload[1]);
    TEST_ASSERT_EQUAL_UINT8(0, feedback[0].frame.payload[2]);
    TEST_ASSERT_TRUE(n.hasLog("event=V3_LINK_SAMPLE"));
    for (uint8_t i = 0; i < count; ++i) {
        const auto sample = ",frame_id=1,source_id=" + std::to_string(i + 2) +
            ",client_boot=" + std::to_string(n.packets(MessageType::DATA, i + 2)[0].frame.bootId) +
            ",sf=7,outcome=first,app=accepted";
        TEST_ASSERT_TRUE(n.hasLog(sample.c_str()));
    }
    if (count > 1) {
        const auto data = n.packets(MessageType::DATA);
        TEST_ASSERT_UINT32_WITHIN(1000, dataAir(lengths[0]) + kGuardUs, data[1].start - data[0].start);
    }
}
void one_client_zero_payload() { runClients(1); }
void two_clients_share_the_radio() { runClients(2); }
void four_clients_with_mixed_payload_lengths() { runClients(4); }
void eight_clients_with_maximum_size_and_binary_bytes() { runClients(8); }

void payload_port_validates_and_copies() {
    Network n(1);
    TEST_ASSERT_EQUAL_INT(int(SubmitResult::INVALID_BYTES), int(n.clients[0].payload.submit(nullptr, 4)));
    auto original = bytes(64);
    TEST_ASSERT_EQUAL_INT(int(SubmitResult::TOO_LARGE), int(n.clients[0].payload.submit(original.data(), 65)));
    n.submit(0, original);
    TEST_ASSERT_EQUAL_INT(int(SubmitResult::BUSY), int(n.clients[0].payload.submit(nullptr, 0)));
    original[1] = 0;
    n.run(8000000);
    TEST_ASSERT_EQUAL_UINT(1, n.deliveries.size());
    TEST_ASSERT_TRUE(n.deliveries[0].bytes == bytes(64));
}

void roster_validation_and_period_admission() {
    TEST_ASSERT_EQUAL_UINT32(3000, kDefaultHelloIntervalMs);
    TEST_ASSERT_EQUAL_UINT32(1000000, kAckTimeoutUs);
    TEST_ASSERT_EQUAL_UINT32(1000000, kTargetPeriodUs);
    TEST_ASSERT_EQUAL_UINT32(10000000, kMaxPeriodUs);
    TEST_ASSERT_EQUAL_UINT32(50000, kGuardUs);
    TEST_ASSERT_EQUAL_UINT(4, kAttempts);
    auto r = roster(8);
    TEST_ASSERT_TRUE(r.valid(1));
    TEST_ASSERT_EQUAL_UINT32(6516000, requiredPeriodUs(r));
    r.periodUs = kTargetPeriodUs;
    TEST_ASSERT_FALSE(r.valid(1));
    r.periodUs = requiredPeriodUs(r);
    TEST_ASSERT_TRUE(r.valid(1));
    r.periodUs -= 1000;
    TEST_ASSERT_FALSE(r.valid(1));
    r.periodUs = 10000000;
    auto bad = r; bad.nodes[2].id = bad.nodes[1].id; TEST_ASSERT_FALSE(bad.valid(1));
    bad = r; bad.nodes[2].id = 1; TEST_ASSERT_FALSE(bad.valid(1));
    bad = r; bad.nodes[2].id = kGroup; TEST_ASSERT_FALSE(bad.valid(1));
    bad = r; bad.nodes[2].budget = 65; TEST_ASSERT_FALSE(bad.valid(1));
    bad = r; bad.count = 9; TEST_ASSERT_FALSE(bad.valid(1));
    bad = r; bad.count = 0; TEST_ASSERT_FALSE(bad.valid(1));
    Network n(1, bad); n.submit(0, {}); n.run(1000000); TEST_ASSERT_TRUE(n.air.empty());
}

void unlisted_and_over_budget_clients_stay_silent() {
    auto r = roster(2);
    r.nodes[0].budget = 3;
    Network n(3, r);
    n.submit(0, bytes(4)); n.submit(1, bytes(3)); n.submit(2, bytes(3));
    n.run(8000000);
    TEST_ASSERT_EQUAL_UINT(1, n.deliveries.size());
    TEST_ASSERT_EQUAL_UINT16(3, n.deliveries[0].id);
    TEST_ASSERT_TRUE(n.packets(MessageType::PAYLOAD_REQUEST, 2).empty());
    TEST_ASSERT_TRUE(n.packets(MessageType::PAYLOAD_REQUEST, 4).empty());
    TEST_ASSERT_EQUAL_INT(int(PayloadResult::NOT_ADMITTED), int(n.clients[0].payload.result()));
    TEST_ASSERT_EQUAL_INT(int(PayloadResult::NOT_ADMITTED), int(n.clients[2].payload.result()));
}

void missing_client_retries_byte_for_byte() {
    Network n(2);
    bool dropped = false;
    n.filter = [&](uint8_t i, Frame& f) {
        if (!i && f.type == MessageType::DATA && f.sourceId == 3 && !dropped) { dropped = true; return false; }
        return true;
    };
    n.submit(0, bytes(3)); n.submit(1, bytes(8)); n.run(8000000);
    const auto retry = n.packets(MessageType::DATA, 3);
    TEST_ASSERT_EQUAL_UINT(2, retry.size());
    TEST_ASSERT_EQUAL_UINT8_ARRAY(retry[0].packet.bytes, retry[1].packet.bytes, retry[0].packet.length);
    TEST_ASSERT_EQUAL_UINT(1, n.packets(MessageType::DATA, 2).size());
    const auto ack = n.packets(MessageType::ACK_BITMAP);
    TEST_ASSERT_EQUAL_UINT8(1, ack[0].frame.payload[1]);
    TEST_ASSERT_EQUAL_UINT8(2, ack[0].frame.payload[2]);
    TEST_ASSERT_EQUAL_UINT8(3, ack[1].frame.payload[1]);
    TEST_ASSERT_EQUAL_UINT(2, n.callbacks);
    const auto gateway = n.gateway.link.telemetry();
    TEST_ASSERT_EQUAL_UINT32(2, gateway.data);
    TEST_ASSERT_EQUAL_UINT32(2, gateway.accepted);
    TEST_ASSERT_EQUAL_UINT32(1, gateway.retries);
    TEST_ASSERT_EQUAL_UINT16(3, gateway.peer.peer);
    TEST_ASSERT_EQUAL_UINT32(10000000, gateway.periodUs);
    TEST_ASSERT_EQUAL_UINT32(1, gateway.frame);
    TEST_ASSERT_TRUE(gateway.bitmap.seen);
    TEST_ASSERT_EQUAL_UINT8(3, gateway.bitmap.round);
    TEST_ASSERT_EQUAL_UINT8(3, gateway.bitmap.accepted);
    TEST_ASSERT_EQUAL_UINT8(0, gateway.bitmap.retries);
    TEST_ASSERT_EQUAL_INT(int(LinkFreshness::ONLINE), int(gateway.freshness(gateway.peer.atUs + 19999999)));
    TEST_ASSERT_EQUAL_INT(int(LinkFreshness::STALE), int(gateway.freshness(gateway.peer.atUs + 20000000)));
    const auto client = n.clients[1].link.telemetry();
    TEST_ASSERT_EQUAL_UINT32(2, client.data);
    TEST_ASSERT_EQUAL_UINT32(1, client.retries);
    TEST_ASSERT_EQUAL_UINT32(1, client.accepted);
    TEST_ASSERT_EQUAL_UINT8(1, client.bitmap.round);
    TEST_ASSERT_EQUAL_INT(int(PayloadResult::ACCEPTED), int(client.result));
}

void partial_retry_pool_and_exhaustion_with_eight_clients() {
    Network n(8);
    n.filter = [](uint8_t i, Frame& f) { return i || f.type != MessageType::DATA; };
    for (uint8_t i = 0; i < 8; ++i) n.submit(i, bytes(i));
    n.run(8000000);
    TEST_ASSERT_EQUAL_UINT(20, n.packets(MessageType::DATA).size()); // Eight primary plus four per retry phase.
    TEST_ASSERT_EQUAL_UINT(0, n.callbacks);
    for (uint8_t i = 0; i < 8; ++i) {
        const auto sent = n.packets(MessageType::DATA, i + 2);
        TEST_ASSERT_TRUE(sent.size() <= kAttempts);
        TEST_ASSERT_EQUAL_INT(int(PayloadResult::MISSING), int(n.clients[i].payload.result()));
        for (const auto& x : sent) TEST_ASSERT_EQUAL_UINT8_ARRAY(sent[0].packet.bytes, x.packet.bytes, x.packet.length);
    }
    TEST_ASSERT_TRUE(n.hasLog("retry_exhausted_missing"));
    TEST_ASSERT_TRUE(n.hasLog("outcome=missed,app=n/a"));
}

void lost_feedback_cannot_authorize_retry_and_cumulative_ack_recovers() {
    Network n(2);
    n.filter = [](uint8_t i, Frame& f) { return i != 1 || f.type != MessageType::ACK_BITMAP || f.sequence != 0; };
    n.submit(0, bytes(3)); n.submit(1, bytes(8)); n.run(8000000);
    TEST_ASSERT_EQUAL_UINT(2, n.callbacks);
    TEST_ASSERT_EQUAL_UINT(1, n.packets(MessageType::DATA, 2).size());
    TEST_ASSERT_TRUE(n.hasLog("result=unknown"));
    TEST_ASSERT_EQUAL_INT(int(PayloadResult::ACCEPTED), int(n.clients[0].payload.result()));
}

void feedback_fault_keeps_retry_closed(bool lateCompletion) {
    Network n(1);
    n.filter = [](uint8_t receiver, Frame& f) { return receiver || f.type != MessageType::DATA; };
    n.submit(0, bytes(8));
    while (n.packets(MessageType::ACK_BITMAP).empty() && n.now < 2000000) n.run(1000);
    const auto acks = n.packets(MessageType::ACK_BITMAP);
    TEST_ASSERT_EQUAL_UINT(1, acks.size());
    TEST_ASSERT_EQUAL_UINT8(1, acks.front().frame.payload[2]);
    const auto data = n.packets(MessageType::DATA).front();
    const auto grant = n.packets(MessageType::SLOT_GRANT).front();
    Plan plan;
    TEST_ASSERT_TRUE(readPlan(grant.frame, plan));

    const uint64_t dataAt = (grant.end + 999) / 1000 * 1000;
    const uint64_t retryStart = dataAt + plan.dataStart(1, 0, 1);
    n.now = lateCompletion ? retryStart + 1 : (acks.front().end + 999) / 1000 * 1000;
    n.sender = nullptr;
    auto& radio = n.gateway.radio;
    n.gateway.link.update(n.now, lateCompletion ? radio.completeTx() : radio.error(RadioEventType::ERROR, -1));
    TEST_ASSERT_TRUE(radio.receiving());
    TEST_ASSERT_EQUAL(lateCompletion, n.gateway.link.telemetry().bitmap.seen);
    n.gateway.link.update(retryStart + dataAir(8), radio.deliver(data.packet));
    TEST_ASSERT_EQUAL_UINT(0, n.callbacks);
}

void failed_feedback_transmission_does_not_open_gateway_retry_slot() {
    feedback_fault_keeps_retry_closed(false);
}

void late_feedback_completion_does_not_open_gateway_retry_slot() {
    feedback_fault_keeps_retry_closed(true);
}

void wrong_feedback_identity_and_late_feedback_remain_unknown() {
    Network n(1);
    n.filter = [](uint8_t i, Frame& f) {
        if (i && f.type == MessageType::ACK_BITMAP) {
            if (!f.sequence) ++f.bootId;
            else if (f.sequence == 1) ++f.frameId;
            else if (f.sequence == 2) put32(f.payload + 3, get32(f.payload + 3) + 1);
            else --f.sequence;
        }
        return true;
    };
    n.submit(0, bytes(3)); n.run(8000000);
    TEST_ASSERT_EQUAL_UINT(1, n.callbacks);
    TEST_ASSERT_EQUAL_INT(int(PayloadResult::UNKNOWN), int(n.clients[0].payload.result()));
}

void refusal_does_not_set_ack_or_redeliver() {
    Network n(1); n.accept = false;
    n.submit(0, bytes(8)); n.run(8000000);
    TEST_ASSERT_EQUAL_UINT(1, n.callbacks);
    TEST_ASSERT_TRUE(n.deliveries.empty());
    TEST_ASSERT_EQUAL_UINT(1, n.packets(MessageType::DATA).size());
    TEST_ASSERT_EQUAL_INT(int(PayloadResult::MISSING), int(n.clients[0].payload.result()));
    TEST_ASSERT_TRUE(n.hasLog("application_refused"));
    const auto telemetry = n.gateway.link.telemetry();
    TEST_ASSERT_EQUAL_UINT32(1, telemetry.data);
    TEST_ASSERT_EQUAL_UINT32(0, telemetry.accepted);
    TEST_ASSERT_EQUAL_UINT32(1, telemetry.refused);
    TEST_ASSERT_TRUE(telemetry.peer.seen);
}

void missing_beacon_and_stale_frame_then_recovery() {
    Network n(2);
    n.filter = [](uint8_t i, Frame& f) {
        if (i == 1 && f.type == MessageType::BEACON) {
            if (f.frameId == 1) return false;
            if (f.frameId == 3) f.frameId = 2;
        }
        return true;
    };
    n.submit(0, bytes(8)); n.submit(1, bytes(3)); n.run(10000000);
    TEST_ASSERT_TRUE(n.clients[0].payload.pending());
    n.run(10000000);
    TEST_ASSERT_EQUAL_UINT(2, n.deliveries.size());
    n.submit(0, bytes(8)); n.run(10000000);
    TEST_ASSERT_TRUE(n.clients[0].payload.pending());
    TEST_ASSERT_TRUE(n.hasLog("stale_beacon"));
    n.run(9000000);
    TEST_ASSERT_EQUAL_UINT(3, n.deliveries.size());
}

void dropped_grant_and_client_restart_do_not_transmit() {
    Network n(1);
    n.filter = [](uint8_t i, Frame& f) { return !i || f.type != MessageType::SLOT_GRANT; };
    n.submit(0, bytes(8)); n.run(1000000);
    MulticlientLink restarted(config(2), n.clients[0].radio, n.clients[0].logger, n.clients[0].payload, boot);
    n.clients[0].active = &restarted;
    n.run(9000000);
    TEST_ASSERT_TRUE(n.packets(MessageType::DATA).empty());
    n.filter = nullptr; n.run(8000000);
    TEST_ASSERT_EQUAL_UINT(1, n.callbacks);
}

void gateway_restart_rejoins_after_previous_lease() {
    Network n(1);
    n.submit(0, bytes(8)); n.run(5000000);
    const auto old = n.packets(MessageType::BEACON)[0].frame.bootId;
    MulticlientLink restarted(config(1), n.gateway.radio, n.gateway.logger, n.gateway.payload, boot, roster(1));
    n.gateway.active = &restarted;
    n.submit(0, bytes(20)); n.run(10000000);
    TEST_ASSERT_EQUAL_UINT(1, n.callbacks);
    n.run(8000000);
    TEST_ASSERT_EQUAL_UINT(2, n.callbacks);
    TEST_ASSERT_NOT_EQUAL(old, n.packets(MessageType::BEACON)[1].frame.bootId);
}

void micros_wrap_and_delayed_request() {
    Network n(2, roster(2), uint64_t(UINT32_MAX) - 100000);
    n.submit(0, bytes(8)); n.submit(1, bytes(64));
    n.run(110000);
    n.now += 120000;  // Lose the first request launch window.
    n.run(7000000);
    TEST_ASSERT_EQUAL_UINT(1, n.callbacks);
    TEST_ASSERT_EQUAL_UINT16(3, n.deliveries[0].id);
    TEST_ASSERT_TRUE(n.hasLog("deadline_miss"));
}

void codec_group_rules_padding_and_version_isolation() {
    Network n(2);
    n.submit(0, bytes(8)); n.submit(1, bytes(64)); n.run(8000000);
    const auto ack = n.packets(MessageType::ACK_BITMAP)[0].packet;
    Frame out;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::VERSION), int(decodeFrame(ack.bytes, ack.length, 1, 2, out)));
    TEST_ASSERT_EQUAL_INT(int(CodecResult::VERSION), int(scheduled::decode(ack.bytes, ack.length, 1, 2, out)));
    auto bad = ack; bad.bytes[kHeaderSize + 1] |= 0x80;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::PAYLOAD), int(decode(bad.bytes, bad.length, 1, 2, out)));
    bad = ack; bad.bytes[26]--; bad.length--;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::PAYLOAD), int(decode(bad.bytes, bad.length, 1, 2, out)));
    bad = ack; put16(bad.bytes + 24, 2);
    TEST_ASSERT_EQUAL_INT(int(CodecResult::RESERVED), int(decode(bad.bytes, bad.length, 1, 2, out)));
    bad = n.packets(MessageType::DATA)[0].packet; put16(bad.bytes + 10, kGroup);
    TEST_ASSERT_EQUAL_INT(int(CodecResult::DESTINATION), int(decode(bad.bytes, bad.length, 1, 1, out)));
    bad = n.packets(MessageType::BEACON)[0].packet; bad.bytes[kHeaderSize + 4] = 9;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::PAYLOAD), int(decode(bad.bytes, bad.length, 1, 2, out)));
}

void guard_edges_are_half_open() {
    constexpr uint64_t start = uint64_t(UINT32_MAX) - 30;
    constexpr auto air = dataAir(64);
    TEST_ASSERT_FALSE(launchWindow(start - 1, start)); TEST_ASSERT_TRUE(launchWindow(start, start));
    TEST_ASSERT_FALSE(launchWindow(start + kLaunchUs, start));
    TEST_ASSERT_FALSE(receiveWindow(start + air - kGuardUs / 2 - 1, start, air));
    TEST_ASSERT_TRUE(receiveWindow(start + air - kGuardUs / 2, start, air));
    TEST_ASSERT_FALSE(receiveWindow(start + air + kGuardUs / 2, start, air));
}

void duplicate_and_conflict_are_not_delivered_again() {
    Network n(1);
    n.submit(0, bytes(8));
    while (n.packets(MessageType::DATA).empty() && n.now < 1000000) n.run(1000);
    const auto data = n.packets(MessageType::DATA)[0];
    n.run((data.end + 999) / 1000 * 1000 - n.now + 1000);
    TEST_ASSERT_EQUAL_UINT(1, n.callbacks);
    auto& gateway = n.gateway;
    gateway.link.update(n.now, gateway.radio.deliver(data.packet));
    auto changed = data.packet;
    changed.bytes[kHeaderSize + kPayloadPrefix] ^= 1;
    gateway.link.update(n.now + 1000, gateway.radio.deliver(changed));
    n.run(2000000);
    TEST_ASSERT_EQUAL_UINT(1, n.callbacks);
    TEST_ASSERT_TRUE(n.hasLog("result=duplicate"));
    TEST_ASSERT_TRUE(n.hasLog("result=conflict"));
    TEST_ASSERT_EQUAL_UINT32(1, gateway.link.telemetry().duplicates);
    TEST_ASSERT_EQUAL_UINT32(1, gateway.link.telemetry().data);
}

void radio_errors_do_not_refresh_v3_link() {
    Peer client(2);
    client.radio.rxFailuresLeft = 1;
    client.link.update(0, {});
    client.link.update(1000000, client.radio.error(RadioEventType::CRC_ERROR, -7));
    const auto snapshot = client.link.telemetry();
    TEST_ASSERT_EQUAL_UINT32(2, snapshot.radioErrors);
    TEST_ASSERT_EQUAL_UINT32(0, snapshot.data);
    TEST_ASSERT_EQUAL_INT(int(LinkFreshness::WAIT), int(snapshot.freshness(1000000)));
}

void early_late_and_unassigned_data_do_not_reach_application() {
    Network n(1);
    n.submit(0, bytes(8));
    n.filter = [](uint8_t receiver, Frame& f) { return receiver || f.type != MessageType::DATA; };
    while (n.packets(MessageType::DATA).empty() && n.now < 1000000) n.run(1000);
    const auto sent = n.packets(MessageType::DATA)[0];
    auto& gateway = n.gateway;
    auto wrong = sent.packet;
    put16(wrong.bytes + 8, 3);
    // Inject before the feedback slot, with monotonically increasing service time.
    const auto ended = (sent.end + 999) / 1000 * 1000;
    n.run(ended - kGuardUs / 2 - 2000 - n.now);
    gateway.link.update(n.now, gateway.radio.deliver(sent.packet));
    n.run(ended + 1000 - n.now);
    gateway.link.update(n.now, gateway.radio.deliver(wrong));
    n.run(kGuardUs / 2 + 1000);
    gateway.link.update(n.now, gateway.radio.deliver(sent.packet));
    TEST_ASSERT_EQUAL_UINT(0, n.callbacks);
}
}

void setUp() { Serial.reset(); identity = 100; }
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(one_client_zero_payload);
    RUN_TEST(two_clients_share_the_radio);
    RUN_TEST(four_clients_with_mixed_payload_lengths);
    RUN_TEST(eight_clients_with_maximum_size_and_binary_bytes);
    RUN_TEST(payload_port_validates_and_copies);
    RUN_TEST(roster_validation_and_period_admission);
    RUN_TEST(unlisted_and_over_budget_clients_stay_silent);
    RUN_TEST(missing_client_retries_byte_for_byte);
    RUN_TEST(partial_retry_pool_and_exhaustion_with_eight_clients);
    RUN_TEST(lost_feedback_cannot_authorize_retry_and_cumulative_ack_recovers);
    RUN_TEST(failed_feedback_transmission_does_not_open_gateway_retry_slot);
    RUN_TEST(late_feedback_completion_does_not_open_gateway_retry_slot);
    RUN_TEST(wrong_feedback_identity_and_late_feedback_remain_unknown);
    RUN_TEST(refusal_does_not_set_ack_or_redeliver);
    RUN_TEST(missing_beacon_and_stale_frame_then_recovery);
    RUN_TEST(dropped_grant_and_client_restart_do_not_transmit);
    RUN_TEST(gateway_restart_rejoins_after_previous_lease);
    RUN_TEST(micros_wrap_and_delayed_request);
    RUN_TEST(codec_group_rules_padding_and_version_isolation);
    RUN_TEST(guard_edges_are_half_open);
    RUN_TEST(duplicate_and_conflict_are_not_delivered_again);
    RUN_TEST(radio_errors_do_not_refresh_v3_link);
    RUN_TEST(early_late_and_unassigned_data_do_not_reach_application);
    return UNITY_END();
}
