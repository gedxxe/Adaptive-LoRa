#include <unity.h>
#include <Arduino.h>
#include <esp_timer.h>
#include <cstring>
#include <string>
#include <initializer_list>
#include "client/client_app.h"
#include "gateway/gateway_app.h"
#include "../support/fake_radio_transport.h"

using namespace adaptive_lora;
using namespace adaptive_lora::test;
namespace {
uint32_t clientBoot = 1000;
uint32_t gatewayBoot = 2000;
uint32_t nextClientBoot(uint32_t previous) { if (++clientBoot == previous) ++clientBoot; return clientBoot; }
uint32_t nextGatewayBoot(uint32_t previous) { if (++gatewayBoot == previous) ++gatewayBoot; return gatewayBoot; }
DeviceConfig configuration(DeviceRole role) {
    DeviceConfig c;
    c.role = role;
    c.nodeId = role == DeviceRole::CLIENT ? 2 : 1;
    c.helloIntervalMs = 1000;
    return c;
}
unsigned occurrences(const std::string& lines, const char* event) {
    const std::string pattern = std::string("event=") + event + ",";
    unsigned count = 0;
    for (size_t at = 0; (at = lines.find(pattern, at)) != std::string::npos; at += pattern.size()) ++count;
    return count;
}
std::string drain(SerialLogger& log) {
    Serial.output.clear();
    for (unsigned budget = 0; !log.empty() && budget < 512; ++budget) log.drain();
    TEST_ASSERT_TRUE(log.empty());
    TEST_ASSERT_EQUAL_UINT32(0, log.dropped());
    return Serial.output;
}
CapturedPacket encoded(const Frame& frame) {
    CapturedPacket packet;
    TEST_ASSERT_EQUAL(int(CodecResult::OK), int(encodeFrame(frame, packet.bytes, sizeof(packet.bytes), packet.length)));
    return packet;
}
Frame decoded(const CapturedPacket& packet, uint16_t destination) {
    Frame frame;
    TEST_ASSERT_EQUAL(int(CodecResult::OK), int(decodeFrame(packet.bytes, packet.length, 1, destination, frame)));
    return frame;
}
void samePacket(const CapturedPacket& a, const CapturedPacket& b) {
    TEST_ASSERT_EQUAL_UINT(a.length, b.length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(a.bytes, b.bytes, a.length);
}

struct Link {
    DeviceConfig clientConfig = configuration(DeviceRole::CLIENT);
    DeviceConfig gatewayConfig = configuration(DeviceRole::GATEWAY);
    FakeRadioTransport clientRadio;
    FakeRadioTransport gatewayRadio;
    SerialLogger clientLog;
    SerialLogger gatewayLog;
    ClientApp client{clientConfig, clientRadio, clientLog, nextClientBoot};
    GatewayApp gateway{gatewayConfig, gatewayRadio, gatewayLog, nextGatewayBoot};
    ClientApp* activeClient = &client;
    GatewayApp* activeGateway = &gateway;
    uint64_t now = 0;
    uint64_t helloDone = 0;
    unsigned accepted = 0, timeouts = 0, txFailed = 0;
    unsigned gatewayAccepted = 0, duplicates = 0, clientRejected = 0, gatewayRejected = 0;
    std::string lastClientLog, lastGatewayLog;

    explicit Link(unsigned clientRxFailures = 0, unsigned gatewayRxFailures = 0, uint64_t start = 0) : now(start) {
        host_clock::set(now);
        clientRadio.rxFailuresLeft = clientRxFailures;
        gatewayRadio.rxFailuresLeft = gatewayRxFailures;
        clientLog.begin(clientConfig);
        gatewayLog.begin(gatewayConfig);
        client.begin(static_cast<uint32_t>(now));
        gateway.begin(static_cast<uint32_t>(now));
        flush();
    }
    void flush() {
        lastClientLog = drain(clientLog);
        lastGatewayLog = drain(gatewayLog);
        accepted += occurrences(lastClientLog, "ACK_RX");
        timeouts += occurrences(lastClientLog, "ACK_TIMEOUT");
        txFailed += occurrences(lastClientLog, "HELLO_FAILED");
        clientRejected += occurrences(lastClientLog, "FRAME_REJECT");
        gatewayRejected += occurrences(lastGatewayLog, "FRAME_REJECT");
        gatewayAccepted += occurrences(lastGatewayLog, "HELLO_RX");
        duplicates += occurrences(lastGatewayLog, "DUPLICATE");
        TEST_ASSERT_EQUAL_UINT(0, clientRadio.contractViolations);
        TEST_ASSERT_EQUAL_UINT(0, gatewayRadio.contractViolations);
    }
    void clockOnly(uint64_t value) { now = value; host_clock::set(now); }
    void advanceTo(uint64_t value) {
        TEST_ASSERT_TRUE(value >= now);
        clockOnly(value);
        activeClient->update(static_cast<uint32_t>(now), {});
        activeGateway->update(static_cast<uint32_t>(now), {});
        flush();
    }
    void startHello() {
        advanceTo(now + uint64_t(clientConfig.helloIntervalMs) * 1000);
        TEST_ASSERT_TRUE(clientRadio.transmitting());
    }
    CapturedPacket completeHello(bool deliver = true) {
        TEST_ASSERT_TRUE(clientRadio.transmitting());
        const CapturedPacket hello = clientRadio.lastTx;
        clockOnly(now + 1000);
        helloDone = now;
        activeClient->update(static_cast<uint32_t>(now), clientRadio.completeTx());
        TEST_ASSERT_TRUE(clientRadio.receiving());
        if (deliver) activeGateway->update(static_cast<uint32_t>(now), gatewayRadio.deliver(hello));
        flush();
        return hello;
    }
    CapturedPacket completeAck(bool deliver = true) {
        TEST_ASSERT_TRUE(gatewayRadio.transmitting());
        const CapturedPacket ack = gatewayRadio.lastTx;
        clockOnly(now + 1000);
        activeGateway->update(static_cast<uint32_t>(now), gatewayRadio.completeTx());
        if (deliver) activeClient->update(static_cast<uint32_t>(now), clientRadio.deliver(ack));
        flush();
        return ack;
    }
    void deliverClient(const CapturedPacket& packet) {
        activeClient->update(static_cast<uint32_t>(now), clientRadio.deliver(packet));
        flush();
    }
    void deliverGateway(const CapturedPacket& packet) {
        activeGateway->update(static_cast<uint32_t>(now), gatewayRadio.deliver(packet));
        flush();
    }
    void transaction() { startHello(); completeHello(); completeAck(); }
};

void test_matching_transaction() {
    Link f;
    f.startHello();
    const auto hello = f.completeHello();
    const auto ack = f.completeAck();
    const Frame h = decoded(hello, 1), a = decoded(ack, 2);
    TEST_ASSERT_TRUE(ackMatches(a, h, 1));
    TEST_ASSERT_EQUAL_UINT(1, f.accepted);
    TEST_ASSERT_EQUAL_UINT(1, f.gatewayAccepted);
    TEST_ASSERT_EQUAL_UINT(0, f.timeouts);
    TEST_ASSERT_TRUE(f.gatewayRadio.receiving());
    TEST_ASSERT_TRUE(f.clientRadio.receiving());
}
void test_hello_loss_timeout_and_next_new_identity() {
    Link f; f.startHello();
    const Frame first = decoded(f.completeHello(false), 1);
    f.advanceTo(f.helloDone + kAckTimeoutUs);
    TEST_ASSERT_EQUAL_UINT(1, f.timeouts);
    TEST_ASSERT_EQUAL_UINT(0, f.gatewayAccepted);
    TEST_ASSERT_EQUAL_UINT(1, f.clientRadio.txCalls);
    f.advanceTo(f.now + 999999);
    TEST_ASSERT_EQUAL_UINT(1, f.clientRadio.txCalls);
    f.advanceTo(f.now + 1);
    const Frame second = decoded(f.clientRadio.lastTx, 1);
    TEST_ASSERT_EQUAL_UINT32(first.bootId, second.bootId);
    TEST_ASSERT_EQUAL_UINT16(first.sequence + 1, second.sequence);
    f.completeHello(); f.completeAck();
    TEST_ASSERT_EQUAL_UINT(1, f.accepted);
}
void test_ack_loss_has_no_automatic_retry() {
    Link f; f.startHello(); f.completeHello(); f.completeAck(false);
    f.advanceTo(f.helloDone + kAckTimeoutUs);
    TEST_ASSERT_EQUAL_UINT(1, f.timeouts);
    TEST_ASSERT_EQUAL_UINT(0, f.accepted);
    TEST_ASSERT_EQUAL_UINT(1, f.gatewayAccepted);
    TEST_ASSERT_EQUAL_UINT(1, f.clientRadio.txCalls);
    TEST_ASSERT_EQUAL_UINT(1, f.gatewayRadio.txCalls);
}
void test_same_packet_replay_preserves_identity_and_reacks_once() {
    Link f; f.startHello();
    const auto original = f.completeHello();
    const auto firstAck = f.completeAck(false);
    const CapturedPacket retransmission = original;
    samePacket(original, retransmission);  // Header identity and all payload bytes.
    f.deliverGateway(retransmission);
    const Frame secondAck = decoded(f.completeAck(), 2);
    TEST_ASSERT_TRUE(ackMatches(secondAck, decoded(original, 1), 1));
    TEST_ASSERT_NOT_EQUAL(decoded(firstAck, 2).sequence, secondAck.sequence);
    TEST_ASSERT_EQUAL_UINT(1, f.gatewayAccepted);
    TEST_ASSERT_EQUAL_UINT(1, f.duplicates);
    TEST_ASSERT_EQUAL_UINT(1, f.accepted);
    TEST_ASSERT_EQUAL_UINT(1, f.clientRadio.txCalls);  // Replay is test transport injection.
}
void test_delayed_ack_before_deadline() {
    Link f; f.startHello(); f.completeHello(); const auto ack = f.completeAck(false);
    f.advanceTo(f.helloDone + kAckTimeoutUs - 1);
    f.deliverClient(ack);
    TEST_ASSERT_EQUAL_UINT(1, f.accepted);
    TEST_ASSERT_EQUAL_UINT(0, f.timeouts);
}
void test_ack_at_deadline_is_rejected() {
    Link f; f.startHello(); f.completeHello(); const auto ack = f.completeAck(false);
    f.clockOnly(f.helloDone + kAckTimeoutUs);
    f.deliverClient(ack);  // RX event and timeout processed in the same production update.
    TEST_ASSERT_EQUAL_UINT(0, f.accepted);
    TEST_ASSERT_EQUAL_UINT(1, f.clientRejected);
    TEST_ASSERT_EQUAL_UINT(1, f.timeouts);
}
void test_ack_after_deadline_is_rejected() {
    Link f; f.startHello(); f.completeHello(); const auto ack = f.completeAck(false);
    f.advanceTo(f.helloDone + kAckTimeoutUs + 1); f.deliverClient(ack);
    TEST_ASSERT_EQUAL_UINT(0, f.accepted);
    TEST_ASSERT_EQUAL_UINT(1, f.clientRejected);
    TEST_ASSERT_EQUAL_UINT(1, f.timeouts);
}
void test_previous_ack_during_new_transaction_is_rejected() {
    Link f; f.startHello(); f.completeHello(); const auto old = f.completeAck(false);
    f.advanceTo(f.helloDone + kAckTimeoutUs);
    f.startHello(); f.completeHello();
    f.deliverClient(old);
    TEST_ASSERT_EQUAL_UINT(0, f.accepted);
    f.completeAck();
    TEST_ASSERT_EQUAL_UINT(1, f.accepted);
    TEST_ASSERT_EQUAL_UINT(1, f.clientRejected);
}
void test_duplicate_ack_does_not_complete_twice() {
    Link f; f.startHello(); f.completeHello(); const auto ack = f.completeAck();
    f.deliverClient(ack);
    TEST_ASSERT_EQUAL_UINT(1, f.accepted);
    TEST_ASSERT_EQUAL_UINT(1, f.clientRejected);
}
void wrongAck(unsigned offset, uint8_t value) {
    Link f; f.startHello(); f.completeHello(); auto ack = f.completeAck(false);
    auto wrong = ack; wrong.bytes[offset] = value;
    f.deliverClient(wrong);
    TEST_ASSERT_EQUAL_UINT(0, f.accepted);
    TEST_ASSERT_EQUAL_UINT(1, f.clientRejected);
    f.deliverClient(ack);
    TEST_ASSERT_EQUAL_UINT(1, f.accepted);
}
void test_wrong_echo_source() { wrongAck(28, 3); }
void test_wrong_echo_boot() { wrongAck(30, 42); }
void test_wrong_echo_sequence() { wrongAck(34, 42); }
void test_wrong_gateway_sender() { wrongAck(8, 3); }
void test_wrong_ack_network_or_destination() { wrongAck(6, 2); wrongAck(10, 3); }

void test_malformed_hellos_rejected_then_valid_recovers() {
    Link f; f.startHello(); const auto hello = f.completeHello(false);
    unsigned rejected = 0;
    for (size_t size = 0; size < kHeaderSize; ++size) {
        auto packet = hello; packet.length = size; f.deliverGateway(packet); ++rejected;
    }
    struct Mutation { unsigned offset; uint8_t value; };
    const Mutation mutations[] = {{0,0},{2,2},{3,0x10},{4,1},{5,27},{6,2},{8,0},{10,3},
        {16,1},{22,1},{24,1},{26,1},{26,69},{27,1}};
    for (const auto& mutation : mutations) {
        auto packet = hello; packet.bytes[mutation.offset] = mutation.value;
        f.deliverGateway(packet); ++rejected;
    }
    auto trailing = hello; trailing.bytes[trailing.length++] = 0;
    f.deliverGateway(trailing); ++rejected;
    auto noSession = hello; put32(noSession.bytes + 12, 0);
    f.deliverGateway(noSession); ++rejected;
    TEST_ASSERT_EQUAL_UINT(rejected, f.gatewayRejected);
    TEST_ASSERT_EQUAL_UINT(0, f.gatewayRadio.txCalls);
    f.deliverGateway(hello); f.completeAck();
    TEST_ASSERT_EQUAL_UINT(1, f.accepted);
    TEST_ASSERT_EQUAL_UINT(1, f.gatewayAccepted);
}
void test_malformed_acks_do_not_cancel_outstanding_identity() {
    Link f; f.startHello(); f.completeHello(); const auto ack = f.completeAck(false);
    unsigned rejected = 0;
    for (size_t size = 0; size < ack.length; ++size) {
        auto packet = ack; packet.length = size; f.deliverClient(packet); ++rejected;
    }
    for (unsigned offset : {0u,2u,3u,4u,5u,26u,27u,36u}) {
        auto packet = ack; packet.bytes[offset] ^= 0x80;
        f.deliverClient(packet); ++rejected;
    }
    TEST_ASSERT_EQUAL_UINT(rejected, f.clientRejected);
    TEST_ASSERT_EQUAL_UINT(0, f.accepted);
    f.deliverClient(ack);
    TEST_ASSERT_EQUAL_UINT(1, f.accepted);
    TEST_ASSERT_EQUAL_UINT(0, f.timeouts);
}
void test_wrong_message_for_each_role() {
    Link f;
    Frame h; h.networkId = 1; h.sourceId = 1; h.destinationId = 2; h.bootId = 9;
    Frame a; a.type = MessageType::ACK; a.networkId = 1; a.sourceId = 2; a.destinationId = 1; a.bootId = 10;
    setAckPayload(a, h);
    f.deliverGateway(encoded(a));
    TEST_ASSERT_EQUAL_UINT(1, f.gatewayRejected);
    TEST_ASSERT_EQUAL_UINT(0, f.gatewayRadio.txCalls);
    f.startHello(); f.completeHello(false);
    f.deliverClient(encoded(h));
    TEST_ASSERT_EQUAL_UINT(1, f.clientRejected);
    TEST_ASSERT_EQUAL_UINT(0, f.accepted);
}
void test_client_tx_start_failure_uses_new_identity_next_time() {
    Link f; f.clientRadio.failNextTx = true;
    f.advanceTo(1000000);
    const Frame first = decoded(f.clientRadio.lastTx, 1);
    TEST_ASSERT_EQUAL_UINT(1, f.txFailed);
    TEST_ASSERT_EQUAL_UINT(0, f.timeouts);
    f.startHello();
    const Frame next = decoded(f.clientRadio.lastTx, 1);
    TEST_ASSERT_EQUAL_UINT32(first.bootId, next.bootId);
    TEST_ASSERT_EQUAL_UINT16(first.sequence + 1, next.sequence);
    f.completeHello(); f.completeAck();
    TEST_ASSERT_EQUAL_UINT(1, f.accepted);
}
void test_client_tx_error_never_counts_as_ack_success() {
    Link f; f.startHello();
    f.client.update(static_cast<uint32_t>(f.now), f.clientRadio.error(RadioEventType::ERROR, -905)); f.flush();
    TEST_ASSERT_EQUAL_UINT(1, f.txFailed);
    TEST_ASSERT_EQUAL_UINT(0, f.timeouts);
    TEST_ASSERT_EQUAL_UINT(0, f.accepted);
    f.transaction(); TEST_ASSERT_EQUAL_UINT(1, f.accepted);
}
void test_gateway_ack_start_failure_then_same_hello_recovery() {
    Link f; f.gatewayRadio.failNextTx = true; f.startHello(); const auto hello = f.completeHello();
    TEST_ASSERT_EQUAL_UINT(1, f.gatewayAccepted);
    TEST_ASSERT_TRUE(f.gatewayRadio.receiving());
    f.deliverGateway(hello); f.completeAck();
    TEST_ASSERT_EQUAL_UINT(1, f.gatewayAccepted);
    TEST_ASSERT_EQUAL_UINT(1, f.duplicates);
    TEST_ASSERT_EQUAL_UINT(1, f.accepted);
}
void test_gateway_tx_error_then_same_hello_recovery() {
    Link f; f.startHello(); const auto hello = f.completeHello();
    f.gateway.update(static_cast<uint32_t>(f.now), f.gatewayRadio.error(RadioEventType::ERROR, -906)); f.flush();
    f.deliverGateway(hello); f.completeAck();
    TEST_ASSERT_EQUAL_UINT(1, f.gatewayAccepted);
    TEST_ASSERT_EQUAL_UINT(1, f.duplicates);
    TEST_ASSERT_EQUAL_UINT(1, f.accepted);
}
void test_client_rx_recovery_backoff_and_limit() {
    Link f(3, 0); f.advanceTo(0);
    TEST_ASSERT_EQUAL_UINT(1, f.clientRadio.rxCalls);
    f.advanceTo(999999); TEST_ASSERT_EQUAL_UINT(1, f.clientRadio.rxCalls);
    f.advanceTo(1000000); TEST_ASSERT_EQUAL_UINT(2, f.clientRadio.rxCalls);
    f.advanceTo(2000000); TEST_ASSERT_EQUAL_UINT(3, f.clientRadio.rxCalls);
    f.advanceTo(10000000); TEST_ASSERT_EQUAL_UINT(3, f.clientRadio.rxCalls);
    TEST_ASSERT_EQUAL_UINT(0, f.clientRadio.txCalls);
}
void test_gateway_rx_recovery_backoff_and_limit() {
    Link f(0, 3);
    TEST_ASSERT_EQUAL_UINT(1, f.gatewayRadio.rxCalls);
    f.advanceTo(999999); TEST_ASSERT_EQUAL_UINT(1, f.gatewayRadio.rxCalls);
    f.advanceTo(1000000); TEST_ASSERT_EQUAL_UINT(2, f.gatewayRadio.rxCalls);
    f.advanceTo(2000000); TEST_ASSERT_EQUAL_UINT(3, f.gatewayRadio.rxCalls);
    f.advanceTo(10000000); TEST_ASSERT_EQUAL_UINT(3, f.gatewayRadio.rxCalls);
    TEST_ASSERT_EQUAL_UINT(0, f.gatewayRadio.txCalls);
}
void test_receive_errors_preserve_ack_deadline() {
    Link f; f.startHello(); f.completeHello(); const auto ack = f.completeAck(false);
    for (auto type : {RadioEventType::CRC_ERROR,RadioEventType::RX_TIMEOUT,RadioEventType::ERROR}) {
        f.clockOnly(f.now + 1000);
        f.client.update(static_cast<uint32_t>(f.now), f.clientRadio.error(type, -907)); f.flush();
        TEST_ASSERT_TRUE(f.clientRadio.receiving());
    }
    f.advanceTo(f.helloDone + kAckTimeoutUs);
    TEST_ASSERT_EQUAL_UINT(1, f.timeouts); f.deliverClient(ack);
    TEST_ASSERT_EQUAL_UINT(0, f.accepted);
    TEST_ASSERT_EQUAL_UINT(1, f.clientRejected);
}
void test_repeated_transactions_200() {
    Link f;
    for (unsigned n = 0; n < 200; ++n) {
        f.transaction(); const Frame hello = decoded(f.clientRadio.lastTx, 1);
        TEST_ASSERT_EQUAL_UINT16(n, hello.sequence);
    }
    TEST_ASSERT_EQUAL_UINT(200, f.accepted);
    TEST_ASSERT_EQUAL_UINT(200, f.gatewayAccepted);
    TEST_ASSERT_EQUAL_UINT(0, f.timeouts);
    TEST_ASSERT_EQUAL_UINT(0, f.duplicates);
}
void test_client_reboot_rejects_prior_session_ack() {
    Link f; f.startHello(); const Frame oldHello = decoded(f.completeHello(), 1);
    const auto oldAck = f.completeAck(false);
    f.clientRadio.mode = FakeRadioTransport::Mode::IDLE;
    ClientApp rebooted(f.clientConfig, f.clientRadio, f.clientLog, nextClientBoot);
    f.activeClient = &rebooted; rebooted.begin(static_cast<uint32_t>(f.now));
    f.startHello(); const Frame next = decoded(f.completeHello(), 1);
    TEST_ASSERT_NOT_EQUAL(oldHello.bootId, next.bootId);
    TEST_ASSERT_EQUAL_UINT16(0, next.sequence);
    f.deliverClient(oldAck); TEST_ASSERT_EQUAL_UINT(0, f.accepted);
    f.completeAck(); TEST_ASSERT_EQUAL_UINT(1, f.accepted);
    TEST_ASSERT_EQUAL_UINT(2, f.gatewayAccepted);
}
void test_gateway_reboot_loses_volatile_history_but_echo_stays_correct() {
    Link f; f.startHello(); const auto hello = f.completeHello(); const auto oldAck = f.completeAck(false);
    f.gatewayRadio.mode = FakeRadioTransport::Mode::IDLE;
    GatewayApp rebooted(f.gatewayConfig, f.gatewayRadio, f.gatewayLog, nextGatewayBoot);
    f.activeGateway = &rebooted; rebooted.begin(static_cast<uint32_t>(f.now));
    f.deliverGateway(hello); const auto newAck = f.completeAck();
    TEST_ASSERT_NOT_EQUAL(decoded(oldAck, 2).bootId, decoded(newAck, 2).bootId);
    TEST_ASSERT_TRUE(ackMatches(decoded(newAck, 2), decoded(hello, 1), 1));
    TEST_ASSERT_EQUAL_UINT(2, f.gatewayAccepted);  // Documented reboot loss of replay history.
    TEST_ASSERT_EQUAL_UINT(0, f.duplicates);
    TEST_ASSERT_EQUAL_UINT(1, f.accepted);
}
void test_retained_old_session_duplicate_keeps_original_echo() {
    Link f; f.startHello(); const auto oldHello = f.completeHello(); f.completeAck();
    const Frame original = decoded(oldHello, 1);
    Frame newHello = original; ++newHello.bootId;
    f.deliverGateway(encoded(newHello)); f.completeAck(false);
    f.deliverGateway(oldHello); const auto ack = f.completeAck(false);
    TEST_ASSERT_TRUE(ackMatches(decoded(ack, 2), original, 1));
    TEST_ASSERT_EQUAL_UINT(2, f.gatewayAccepted);
    TEST_ASSERT_EQUAL_UINT(1, f.duplicates);
}
void test_32_bit_microsecond_wrap_during_ack_wait() {
    Link f(0, 0, uint64_t(0xFFFFFFFFu) - 1500000);
    f.startHello(); f.completeHello();
    const auto ack = f.completeAck(false);
    f.advanceTo(f.helloDone + kAckTimeoutUs - 1); f.deliverClient(ack);
    TEST_ASSERT_TRUE(f.now > uint64_t(0xFFFFFFFFu));
    TEST_ASSERT_EQUAL_UINT(1, f.accepted);
    TEST_ASSERT_EQUAL_UINT(0, f.timeouts);
}
void test_maximum_interval_uses_64_bit_time() {
    DeviceConfig c = configuration(DeviceRole::CLIENT); c.helloIntervalMs = kMaxHelloIntervalMs;
    FakeRadioTransport radio; SerialLogger log; log.begin(c);
    const uint64_t start = uint64_t(0xFFFFFFFFu) + 500000;
    host_clock::set(start); ClientApp client(c, radio, log, nextClientBoot); client.begin(static_cast<uint32_t>(start));
    const uint64_t sent = start + 1000000;
    host_clock::set(sent); client.update(static_cast<uint32_t>(sent), {});
    TEST_ASSERT_TRUE(radio.transmitting());
    client.update(static_cast<uint32_t>(sent), radio.error(RadioEventType::ERROR, -908)); drain(log);
    const uint64_t next = sent + uint64_t(kMaxHelloIntervalMs) * 1000;
    host_clock::set(next - 1); client.update(static_cast<uint32_t>(next - 1), {}); drain(log);
    TEST_ASSERT_EQUAL_UINT(1, radio.txCalls);
    host_clock::set(next); client.update(static_cast<uint32_t>(next), {}); drain(log);
    TEST_ASSERT_EQUAL_UINT(2, radio.txCalls);
}
void test_32_bit_microsecond_wrap_during_receive_recovery() {
    const uint64_t start = uint64_t(0xFFFFFFFFu) - 500000;
    Link f(3, 3, start); f.advanceTo(start);
    f.advanceTo(start + kRadioRecoveryUs - 1);
    TEST_ASSERT_EQUAL_UINT(1, f.clientRadio.rxCalls); TEST_ASSERT_EQUAL_UINT(1, f.gatewayRadio.rxCalls);
    f.advanceTo(start + kRadioRecoveryUs);
    TEST_ASSERT_EQUAL_UINT(2, f.clientRadio.rxCalls); TEST_ASSERT_EQUAL_UINT(2, f.gatewayRadio.rxCalls);
    f.advanceTo(start + 2 * kRadioRecoveryUs);
    TEST_ASSERT_EQUAL_UINT(3, f.clientRadio.rxCalls); TEST_ASSERT_EQUAL_UINT(3, f.gatewayRadio.rxCalls);
}
void test_stop_gate_prevents_new_client_tx_and_finishes_active_tx() {
    Link f; f.clockOnly(1000000);
    f.client.update(static_cast<uint32_t>(f.now), {}, false); f.flush();
    TEST_ASSERT_EQUAL_UINT(0, f.clientRadio.txCalls);
    f.client.update(static_cast<uint32_t>(f.now), {}); f.flush();
    TEST_ASSERT_TRUE(f.clientRadio.transmitting());
    f.client.update(static_cast<uint32_t>(f.now), f.clientRadio.completeTx(), false); f.flush();
    TEST_ASSERT_FALSE(f.clientRadio.transmitting());
    f.clockOnly(f.now + kAckTimeoutUs);
    f.client.update(static_cast<uint32_t>(f.now), {}, false); f.flush();
    TEST_ASSERT_EQUAL_UINT(1, f.timeouts);
    f.clockOnly(f.now + 2000000); f.client.update(static_cast<uint32_t>(f.now), {}, false); f.flush();
    TEST_ASSERT_EQUAL_UINT(1, f.clientRadio.txCalls);
}
void test_stop_gate_prevents_gateway_ack() {
    Link f; f.startHello(); const auto hello = f.completeHello(false);
    f.gateway.update(static_cast<uint32_t>(f.now), f.gatewayRadio.deliver(hello), false); f.flush();
    TEST_ASSERT_EQUAL_UINT(0, f.gatewayRadio.txCalls);
    TEST_ASSERT_EQUAL_UINT(0, f.gatewayAccepted);
}
void test_sequence_exhaustion_in_actual_apps_changes_both_sessions() {
    Link f; uint32_t initialClient = 0, initialGateway = 0;
    for (uint32_t n = 0; n <= 65536; ++n) {
        f.transaction();
        const Frame h = decoded(f.clientRadio.lastTx, 1), a = decoded(f.gatewayRadio.lastTx, 2);
        if (!n) { initialClient = h.bootId; initialGateway = a.bootId; }
        TEST_ASSERT_EQUAL_UINT16(n & 0xFFFFu, h.sequence);
        TEST_ASSERT_EQUAL_UINT16(n & 0xFFFFu, a.sequence);
        TEST_ASSERT_TRUE(ackMatches(a, h, 1));
        if (n < 65536) {
            TEST_ASSERT_EQUAL_UINT32(initialClient, h.bootId); TEST_ASSERT_EQUAL_UINT32(initialGateway, a.bootId);
        } else {
            TEST_ASSERT_NOT_EQUAL(initialClient, h.bootId); TEST_ASSERT_NOT_EQUAL(initialGateway, a.bootId);
        }
    }
    TEST_ASSERT_EQUAL_UINT(65537, f.accepted);
    TEST_ASSERT_EQUAL_UINT(65537, f.gatewayAccepted);
    TEST_ASSERT_EQUAL_UINT(0, f.duplicates);
}
void test_stale_sequence_is_not_accepted_or_reacked() {
    Link f; f.startHello(); Frame h = decoded(f.completeHello(false), 1);
    h.sequence = 100; f.deliverGateway(encoded(h)); f.completeAck(false);
    h.sequence = 0; f.deliverGateway(encoded(h));
    TEST_ASSERT_EQUAL_UINT(1, f.gatewayAccepted);
    TEST_ASSERT_EQUAL_UINT(1, f.gatewayRejected);
    TEST_ASSERT_EQUAL_UINT(1, f.gatewayRadio.txCalls);
}
void test_out_of_order_sequence_accepted_once() {
    Link f; f.startHello(); Frame h = decoded(f.completeHello(false), 1);
    h.sequence = 10; f.deliverGateway(encoded(h)); f.completeAck(false);
    h.sequence = 8; const auto packet = encoded(h);
    f.deliverGateway(packet); f.completeAck(false); f.deliverGateway(packet); f.completeAck(false);
    TEST_ASSERT_EQUAL_UINT(2, f.gatewayAccepted); TEST_ASSERT_EQUAL_UINT(1, f.duplicates);
}
void test_unavailable_transport_never_starts_hello() {
    Link f; f.clientRadio.ready = false;
    f.advanceTo(1000000); f.advanceTo(2000000); f.advanceTo(3000000); f.advanceTo(4000000);
    TEST_ASSERT_EQUAL_UINT(0, f.clientRadio.txCalls);
    TEST_ASSERT_EQUAL_UINT(3, f.clientRadio.rxCalls);
}
void test_max_payload_replay_is_byte_identical() {
    Link f; Frame h; h.networkId = 1; h.sourceId = 2; h.destinationId = 1; h.bootId = 123;
    h.payloadLength = kMaxPayloadSize;
    for (unsigned n = 0; n < kMaxPayloadSize; ++n) h.payload[n] = static_cast<uint8_t>(n ^ 0xA5);
    const auto first = encoded(h), replay = first;
    samePacket(first, replay); TEST_ASSERT_EQUAL_UINT(96, replay.length);
    f.deliverGateway(first); const Frame a = decoded(f.completeAck(false), 2);
    f.deliverGateway(replay); const Frame b = decoded(f.completeAck(false), 2);
    TEST_ASSERT_TRUE(ackMatches(a, h, 1)); TEST_ASSERT_TRUE(ackMatches(b, h, 1));
    TEST_ASSERT_EQUAL_UINT(1, f.gatewayAccepted); TEST_ASSERT_EQUAL_UINT(1, f.duplicates);
}
}
void setUp() { Serial.reset(); host_clock::set(0); clientBoot = 1000; gatewayBoot = 2000; }
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_matching_transaction);
    RUN_TEST(test_hello_loss_timeout_and_next_new_identity);
    RUN_TEST(test_ack_loss_has_no_automatic_retry);
    RUN_TEST(test_same_packet_replay_preserves_identity_and_reacks_once);
    RUN_TEST(test_delayed_ack_before_deadline);
    RUN_TEST(test_ack_at_deadline_is_rejected);
    RUN_TEST(test_ack_after_deadline_is_rejected);
    RUN_TEST(test_previous_ack_during_new_transaction_is_rejected);
    RUN_TEST(test_duplicate_ack_does_not_complete_twice);
    RUN_TEST(test_wrong_echo_source); RUN_TEST(test_wrong_echo_boot); RUN_TEST(test_wrong_echo_sequence);
    RUN_TEST(test_wrong_gateway_sender); RUN_TEST(test_wrong_ack_network_or_destination);
    RUN_TEST(test_malformed_hellos_rejected_then_valid_recovers);
    RUN_TEST(test_malformed_acks_do_not_cancel_outstanding_identity);
    RUN_TEST(test_wrong_message_for_each_role);
    RUN_TEST(test_client_tx_start_failure_uses_new_identity_next_time);
    RUN_TEST(test_client_tx_error_never_counts_as_ack_success);
    RUN_TEST(test_gateway_ack_start_failure_then_same_hello_recovery);
    RUN_TEST(test_gateway_tx_error_then_same_hello_recovery);
    RUN_TEST(test_client_rx_recovery_backoff_and_limit);
    RUN_TEST(test_gateway_rx_recovery_backoff_and_limit);
    RUN_TEST(test_receive_errors_preserve_ack_deadline);
    RUN_TEST(test_repeated_transactions_200);
    RUN_TEST(test_client_reboot_rejects_prior_session_ack);
    RUN_TEST(test_gateway_reboot_loses_volatile_history_but_echo_stays_correct);
    RUN_TEST(test_retained_old_session_duplicate_keeps_original_echo);
    RUN_TEST(test_32_bit_microsecond_wrap_during_ack_wait);
    RUN_TEST(test_maximum_interval_uses_64_bit_time);
    RUN_TEST(test_32_bit_microsecond_wrap_during_receive_recovery);
    RUN_TEST(test_stop_gate_prevents_new_client_tx_and_finishes_active_tx);
    RUN_TEST(test_stop_gate_prevents_gateway_ack);
    RUN_TEST(test_sequence_exhaustion_in_actual_apps_changes_both_sessions);
    RUN_TEST(test_stale_sequence_is_not_accepted_or_reacked);
    RUN_TEST(test_out_of_order_sequence_accepted_once);
    RUN_TEST(test_unavailable_transport_never_starts_hello);
    RUN_TEST(test_max_payload_replay_is_byte_identical);
    return UNITY_END();
}
