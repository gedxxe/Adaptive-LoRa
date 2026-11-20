#include <unity.h>
#include <Arduino.h>
#include "app/scheduled_link.h"
#include "../support/fake_radio_transport.h"

using namespace adaptive_lora;
using namespace adaptive_lora::scheduled;
using namespace adaptive_lora::test;

namespace {
constexpr Schedule calendar{10000000, 8};
uint32_t nextBoot;
uint32_t bootId(uint32_t) { return ++nextBoot; }
DeviceConfig config(DeviceRole role) {
    DeviceConfig c;
    c.role = role;
    c.nodeId = role == DeviceRole::CLIENT ? 2 : 1;
    return c;
}
CapturedPacket encoded(const Frame& f) {
    CapturedPacket p;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(encode(f, p.bytes, sizeof(p.bytes), p.length)));
    return p;
}
Frame decoded(const CapturedPacket& p, uint16_t destination) {
    Frame f;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(decode(p.bytes, p.length, 1, destination, f)));
    return f;
}
void sameBytes(const CapturedPacket& first, const CapturedPacket& current) {
    TEST_ASSERT_EQUAL_UINT(first.length, current.length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(first.bytes, current.bytes, first.length);
}
struct Pair {
    DeviceConfig gc = config(DeviceRole::GATEWAY), cc = config(DeviceRole::CLIENT);
    FakeRadioTransport gr, cr;
    SerialLogger gl, cl;
    ScheduledLink gateway{gc, gr, gl, bootId};
    ScheduledLink client{cc, cr, cl, bootId};
    ScheduledLink* g = &gateway;
    ScheduledLink* c = &client;
    uint64_t anchor = 0;
    CapturedPacket lastBeacon;

    Pair() { gl.begin(gc); cl.begin(cc); c->update(0, {}, true); }
    void beacon(uint64_t launch = 0, bool deliver = true, uint32_t clientLag = 0) {
        g->update(launch, {}, true);
        TEST_ASSERT_TRUE(gr.transmitting());
        lastBeacon = gr.lastTx;
        TEST_ASSERT_EQUAL_INT(int(MessageType::BEACON), int(decoded(lastBeacon, 2).type));
        anchor = launch + airtimeUs(lastBeacon.length);
        g->update(anchor, gr.completeTx(), true);
        if (deliver) c->update(anchor + clientLag, cr.deliver(lastBeacon), true);
        flush();
    }
    CapturedPacket data(uint8_t round, bool deliver = true, uint32_t launchOffset = 0) {
        const uint64_t launch = anchor + calendar.dataStart(round) + launchOffset;
        c->update(launch, {}, true);
        TEST_ASSERT_TRUE(cr.transmitting());
        const auto packet = cr.lastTx;
        const uint64_t end = launch + airtimeUs(packet.length);
        c->update(end, cr.completeTx(), true);
        if (deliver) g->update(end, gr.deliver(packet), true);
        flush();
        return packet;
    }
    CapturedPacket feedback(uint8_t round, bool deliver = true, uint32_t clientLag = 0) {
        const uint64_t launch = anchor + calendar.feedbackStart(round);
        g->update(launch, {}, true);
        TEST_ASSERT_TRUE(gr.transmitting());
        const auto packet = gr.lastTx;
        const uint64_t end = launch + airtimeUs(packet.length);
        g->update(end, gr.completeTx(), true);
        if (deliver) c->update(end + clientLag, cr.deliver(packet), true);
        flush();
        return packet;
    }
    void expireFeedback(uint8_t round, uint32_t clientLag = 0) {
        c->update(anchor + calendar.feedbackEnd(round) + clientLag, {}, true);
        flush();
    }
    void flush() {
        // Each real board owns its UART. Drain whole records before sharing
        // the single native UART shim with the other node.
        for (unsigned i = 0; i < 128 && !cl.empty(); ++i) cl.drain();
        for (unsigned i = 0; i < 128 && !gl.empty(); ++i) gl.drain();
        TEST_ASSERT_TRUE(cl.empty()); TEST_ASSERT_TRUE(gl.empty());
        TEST_ASSERT_EQUAL_UINT(0, cr.contractViolations + gr.contractViolations);
    }
};
bool contains(const char* text) {
    const std::string expected(text);
    const auto marker = expected.find(",result=");
    if (marker == std::string::npos) return Serial.output.find(text) != std::string::npos;
    const auto event = expected.substr(0, marker + 1);
    for (auto at = Serial.output.find(event); at != std::string::npos; at = Serial.output.find(event, at + 1)) {
        const auto end = Serial.output.find('\n', at);
        if (Serial.output.substr(at, end - at).find(expected.substr(marker)) != std::string::npos) return true;
    }
    return false;
}
void has(const char* text) { TEST_ASSERT_TRUE_MESSAGE(contains(text), text); }
void lacks(const char* text) { TEST_ASSERT_FALSE_MESSAGE(contains(text), text); }

void timing_and_payload_bounds() {
    const Schedule min{10000000, 0}, max{10000000, 64}, oversized{10000000, 65}, shortFrame{1000000, 8};
    TEST_ASSERT_TRUE(min.valid()); TEST_ASSERT_TRUE(max.valid());
    TEST_ASSERT_FALSE(oversized.valid()); TEST_ASSERT_FALSE(shortFrame.valid());
    const Schedule invalidPeriod{UINT32_MAX, 8};
    TEST_ASSERT_FALSE(invalidPeriod.valid());
    TEST_ASSERT_EQUAL_UINT32(77056, airtimeUs(34));
    TEST_ASSERT_EQUAL_UINT32(82176, airtimeUs(40));
    TEST_ASSERT_EQUAL_UINT32(164096, airtimeUs(96));
    TEST_ASSERT_GREATER_THAN_UINT32(min.dataUs(), max.dataUs());
    for (uint8_t i = 0; i < kAttempts; ++i) {
        TEST_ASSERT_GREATER_THAN_UINT32(max.dataStart(i) + max.dataUs(), max.feedbackStart(i));
        if (i + 1 < kAttempts) TEST_ASSERT_GREATER_THAN_UINT32(max.feedbackEnd(i), max.dataStart(i + 1));
    }
}

void research_role_and_period_requirements_preserve_nvs_schema() {
    auto client = config(DeviceRole::CLIENT);
    auto gateway = config(DeviceRole::GATEWAY);
    TEST_ASSERT_EQUAL_UINT16(1, client.schema);
    TEST_ASSERT_EQUAL_UINT16(1, client.networkId);
    TEST_ASSERT_EQUAL_UINT16(1, client.gatewayId);
    TEST_ASSERT_EQUAL_UINT16(2, client.nodeId);
    TEST_ASSERT_EQUAL_UINT32(10000, client.helloIntervalMs);
    TEST_ASSERT_TRUE(ScheduledLink::supports(client));
    TEST_ASSERT_TRUE(ScheduledLink::supports(gateway));
    client.nodeId = 3;
    TEST_ASSERT_NULL(validateConfig(client));
    TEST_ASSERT_FALSE(ScheduledLink::supports(client));
    gateway.helloIntervalMs = 1000;
    TEST_ASSERT_NULL(validateConfig(gateway));
    TEST_ASSERT_FALSE(ScheduledLink::supports(gateway));
}

void late_gateway_beacon_completion_suppresses_feedback() {
    Pair p;
    p.g->update(0, {}, true);
    TEST_ASSERT_TRUE(p.gr.transmitting());
    const auto beaconEnd = airtimeUs(p.gr.lastTx.length);
    p.g->update(beaconEnd + kGuardUs, p.gr.completeTx(), true);
    p.g->update(beaconEnd + kGuardUs + calendar.feedbackStart(0), {}, true);
    p.flush();
    TEST_ASSERT_EQUAL_UINT(1, p.gr.txCalls);
    has("event=BEACON_TX,result=completion_missing_or_late");
}

void bitmap_bounds_and_padding_for_1_10_70() {
    uint8_t bits[9]{};
    TEST_ASSERT_FALSE(validBitmap(0, bits, 0)); TEST_ASSERT_FALSE(validBitmap(71, bits, 9));
    TEST_ASSERT_FALSE(validBitmap(1, nullptr, 1));
    const uint8_t counts[]{1, 10, 70};
    for (const uint8_t count : counts) {
        const size_t length = (count + 7) / 8;
        bits[length - 1] = (1u << (count % 8)) - 1;
        TEST_ASSERT_TRUE(validBitmap(count, bits, length));
        TEST_ASSERT_FALSE(validBitmap(count, bits, length - 1));
        bits[length - 1] |= 1u << (count % 8);
        TEST_ASSERT_FALSE(validBitmap(count, bits, length));
    }
}

void codec_bounds_and_v1_isolation() {
    Pair p;
    p.beacon();
    auto data = decoded(p.data(0), 1);
    Frame out;
    const auto beacon = p.lastBeacon;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::VERSION), int(decodeFrame(beacon.bytes, beacon.length, 1, 2, out)));
    Frame hello;
    hello.networkId = 1; hello.sourceId = 2; hello.destinationId = 1; hello.bootId = 100;
    CapturedPacket v1;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::OK), int(encodeFrame(hello, v1.bytes, sizeof(v1.bytes), v1.length)));
    TEST_ASSERT_EQUAL_INT(int(CodecResult::VERSION), int(decode(v1.bytes, v1.length, 1, 1, out)));
    data.payloadLength = kDataPrefix;
    TEST_ASSERT_EQUAL_UINT(kHeaderSize + kDataPrefix, encoded(data).length);
    data.payloadLength = kMaxPayloadSize;
    const auto full = encoded(data);
    TEST_ASSERT_EQUAL_UINT(kMaxFrameSize, full.length);
    TEST_ASSERT_EQUAL_UINT(kMaxPayloadSize, decoded(full, 1).payloadLength);
    TEST_ASSERT_EQUAL_INT(int(CodecResult::LENGTH), int(decode(full.bytes, full.length - 1, 1, 1, out)));
    data.payloadLength = kMaxPayloadSize + 1;
    TEST_ASSERT_EQUAL_INT(int(CodecResult::LENGTH), int(encode(data, v1.bytes, sizeof(v1.bytes), v1.length)));
}

void happy_path_stays_inside_slot_and_repeats() {
    Pair p;
    p.beacon(0, true, 7000);
    p.c->update(p.anchor + calendar.dataStart(0) + 6999, {}, true);
    TEST_ASSERT_EQUAL_UINT(0, p.cr.txCalls);
    const auto initial = decoded(p.data(0, true, 7000), 1);
    const auto ack = decoded(p.feedback(0), 2);
    TEST_ASSERT_EQUAL_UINT8(1, ack.payload[2]);
    TEST_ASSERT_EQUAL_UINT32(initial.bootId, get32(ack.payload + 3));
    has("event=BITMAP_RX,result=received");
    p.feedback(1); p.feedback(2); p.feedback(3);
    TEST_ASSERT_EQUAL_UINT(1, p.cr.txCalls);
    p.beacon(calendar.periodUs); const auto next = decoded(p.data(0), 1);
    TEST_ASSERT_EQUAL_UINT32(initial.frameId + 1, next.frameId);
    p.feedback(0);
}

void lost_data_receives_negative_and_retries_identically() {
    Pair p;
    p.beacon(); const auto first = p.data(0, false);
    TEST_ASSERT_EQUAL_UINT8(0, decoded(p.feedback(0), 2).payload[2]);
    has("event=BITMAP_RX,result=missing");
    sameBytes(first, p.data(1)); p.feedback(1);
    has("event=BITMAP_RX,result=received");
}

void lost_feedback_retries_without_delivering_duplicate() {
    Pair p;
    p.beacon(); const auto first = p.data(0);
    const auto late = p.feedback(0, false); p.expireFeedback(0);
    has("event=BITMAP_RX,result=unknown");
    sameBytes(first, p.data(1));
    p.c->update(p.anchor + calendar.feedbackStart(1) + airtimeUs(late.length), p.cr.deliver(late), true);
    p.feedback(1); has("event=DATA_RX,result=duplicate");
    has("event=SCHEDULE_DROP,result=feedback_identity_or_slot");
    has("event=BITMAP_RX,result=received");
}

void retry_exhaustion_is_four_attempts() {
    Pair p;
    p.beacon(); const auto first = p.data(0, false); p.feedback(0);
    for (uint8_t i = 1; i < 4; ++i) { sameBytes(first, p.data(i, false)); p.feedback(i); }
    p.c->update(p.anchor + calendar.feedbackEnd(3) + kGuardUs, {}, true); p.flush();
    TEST_ASSERT_EQUAL_UINT(4, p.cr.txCalls);
    has("event=DATA_RESULT,result=retry_exhausted");
    lacks("event=BITMAP_RX,result=received");
}

void invalid_beacon_and_missing_beacon_suppress_transmit() {
    Pair p;
    p.beacon(0, false);
    auto malformed = p.lastBeacon;
    malformed.bytes[kHeaderSize + 4] = 65;
    p.c->update(p.anchor, p.cr.deliver(malformed), true);
    auto incompatible = decoded(p.lastBeacon, 2);
    put32(incompatible.payload, calendar.periodUs + 1000000);
    p.c->update(p.anchor, p.cr.deliver(encoded(incompatible)), true);
    p.c->update(p.anchor + calendar.dataStart(0), {}, true); p.flush();
    TEST_ASSERT_EQUAL_UINT(0, p.cr.txCalls);
    has("event=SCHEDULE_DROP,result=payload"); has("stale_or_incompatible_beacon");
    p.beacon(calendar.periodUs); p.data(0); p.feedback(0);
}

void stale_beacon_cannot_rearm_and_expired_launch_is_skipped() {
    Pair p;
    p.beacon(); const auto old = p.lastBeacon;
    p.c->update(p.anchor + calendar.dataStart(0) + kGuardUs, {}, true);
    p.feedback(0); p.expireFeedback(0);
    p.data(1); p.feedback(1);
    p.c->update(p.anchor + calendar.periodUs, p.cr.deliver(old), true);
    p.c->update(p.anchor + calendar.periodUs + calendar.dataStart(0), {}, true); p.flush();
    TEST_ASSERT_EQUAL_UINT(1, p.cr.txCalls);
    has("event=DATA_TX,result=slot_unavailable"); has("stale_or_incompatible_beacon");
}

void gateway_reboot_requires_expiry_and_retires_old_session() {
    Pair p;
    p.beacon(); const auto old = p.lastBeacon; p.data(0); p.feedback(0);
    ScheduledLink rebooted(p.gc, p.gr, p.gl, bootId); p.g = &rebooted;
    p.beacon(calendar.periodUs / 2); p.c->update(p.anchor + calendar.dataStart(0), {}, true);
    TEST_ASSERT_EQUAL_UINT(1, p.cr.txCalls);
    p.beacon(3 * calendar.periodUs / 2); p.data(0); p.feedback(0);
    auto stale = decoded(old, 2); stale.frameId = 300;
    p.c->update(p.anchor + calendar.periodUs + kGuardUs, p.cr.deliver(encoded(stale)), true);
    p.c->update(p.anchor + calendar.periodUs + kGuardUs + calendar.dataStart(0), {}, true);
    TEST_ASSERT_EQUAL_UINT(2, p.cr.txCalls);
}

void client_reboot_waits_for_beacon_and_new_identity() {
    Pair p;
    p.beacon(); const auto old = decoded(p.data(0), 1);
    ScheduledLink rebooted(p.cc, p.cr, p.cl, bootId); p.c = &rebooted;
    p.feedback(0); p.c->update(p.anchor + calendar.dataStart(1), {}, true);
    TEST_ASSERT_EQUAL_UINT(1, p.cr.txCalls);
    p.beacon(calendar.periodUs); const auto fresh = decoded(p.data(0), 1);
    TEST_ASSERT_NOT_EQUAL(old.bootId, fresh.bootId); p.feedback(0);
}

void wrong_client_identity_and_late_data_are_not_accepted() {
    Pair p;
    p.beacon(); const auto first = p.data(0, false);
    const auto end = p.anchor + calendar.dataStart(0) + airtimeUs(first.length);
    auto foreign = decoded(first, 1); foreign.sourceId = 3;
    p.g->update(end, p.gr.deliver(encoded(foreign)), true);
    foreign.sourceId = 2; put32(foreign.payload, 900);
    p.g->update(end, p.gr.deliver(encoded(foreign)), true);
    p.g->update(end + 2 * kGuardUs, p.gr.deliver(first), true);
    TEST_ASSERT_EQUAL_UINT8(0, decoded(p.feedback(0), 2).payload[2]);
    lacks("event=DATA_RX,result=accepted");
    p.data(1); p.feedback(1);
}

void wrong_feedback_client_epoch_is_unknown() {
    Pair p;
    p.beacon(); p.data(0);
    const auto correct = p.feedback(0, false);
    auto wrong = decoded(correct, 2); put32(wrong.payload + 3, 900);
    p.c->update(p.anchor + calendar.feedbackStart(0) + airtimeUs(correct.length), p.cr.deliver(encoded(wrong)), true);
    p.expireFeedback(0);
    has("event=BITMAP_RX,result=unknown"); lacks("event=BITMAP_RX,result=received");
}

void slot_offsets_survive_micros_wrap_and_reboot_inhibit() {
    Pair p;
    p.beacon(uint64_t(UINT32_MAX) - 90000);
    p.c->update(p.anchor + calendar.dataStart(0), {}, false);
    TEST_ASSERT_EQUAL_UINT(0, p.cr.txCalls);
    p.data(0); p.feedback(0); has("event=BITMAP_RX,result=received");
}
}

void setUp() { Serial.reset(); nextBoot = 100; }
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(timing_and_payload_bounds);
    RUN_TEST(research_role_and_period_requirements_preserve_nvs_schema);
    RUN_TEST(late_gateway_beacon_completion_suppresses_feedback);
    RUN_TEST(bitmap_bounds_and_padding_for_1_10_70);
    RUN_TEST(codec_bounds_and_v1_isolation);
    RUN_TEST(happy_path_stays_inside_slot_and_repeats);
    RUN_TEST(lost_data_receives_negative_and_retries_identically);
    RUN_TEST(lost_feedback_retries_without_delivering_duplicate);
    RUN_TEST(retry_exhaustion_is_four_attempts);
    RUN_TEST(invalid_beacon_and_missing_beacon_suppress_transmit);
    RUN_TEST(stale_beacon_cannot_rearm_and_expired_launch_is_skipped);
    RUN_TEST(gateway_reboot_requires_expiry_and_retires_old_session);
    RUN_TEST(client_reboot_waits_for_beacon_and_new_identity);
    RUN_TEST(wrong_client_identity_and_late_data_are_not_accepted);
    RUN_TEST(wrong_feedback_client_epoch_is_unknown);
    RUN_TEST(slot_offsets_survive_micros_wrap_and_reboot_inhibit);
    return UNITY_END();
}
