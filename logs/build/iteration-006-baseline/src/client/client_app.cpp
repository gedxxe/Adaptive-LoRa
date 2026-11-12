#include "client_app.h"
#include <cstdio>
#include <inttypes.h>
#include <esp_timer.h>

namespace adaptive_lora {
ClientApp::ClientApp(const DeviceConfig& config, RadioDriver& radio, SerialLogger& log, BootIdGenerator generate)
    : config_(config), radio_(radio), log_(log), generate_(generate) {}
void ClientApp::begin(uint32_t now) {
    sequence_.begin(generate_(0));
    nextHelloUs_ = static_cast<uint64_t>(esp_timer_get_time()) + 1000000;
    recoverAt_ = now;
}
void ClientApp::finish(const char* result, const RadioEvent* event, bool txFailure) {
    if (event) ++successes_;
    else if (txFailure) ++txFailures_;
    else ++timeouts_;
    log_.event(event ? "ACK_RX" : (txFailure ? "HELLO_FAILED" : "ACK_TIMEOUT"), result, &outstanding_, event);
    char line[256];
    std::snprintf(line, sizeof(line), "event=LINK_COUNTS,attempts=%" PRIu32 ",ack_success=%" PRIu32 ",timeouts=%" PRIu32
                  ",tx_failures=%" PRIu32 ",transaction_us=%" PRIu64,
                  attempts_, successes_, timeouts_, txFailures_, static_cast<uint64_t>(esp_timer_get_time()) - sentAtUs_);
    log_.text(line);
    state_ = State::IDLE;
    nextHelloUs_ = static_cast<uint64_t>(esp_timer_get_time()) + uint64_t(config_.helloIntervalMs) * 1000;
}
void ClientApp::update(uint32_t now, const RadioEvent& event, bool allowNew) {
    if (event.type == RadioEventType::TX_DONE && state_ == State::TRANSMITTING) {
        log_.event("TX_DONE", "hello_sent", &outstanding_);
        state_ = State::WAIT_ACK;
        ackDeadline_ = now + kAckTimeoutUs;
    } else if (event.type == RadioEventType::RX_DONE) {
        Frame ack;
        const auto decoded = decodeFrame(event.bytes, event.length, config_.networkId, config_.nodeId, ack);
        log_.event("RX_DONE", "packet", decoded == CodecResult::OK ? &ack : nullptr, &event);
        if (decoded != CodecResult::OK) log_.event("FRAME_REJECT", codecResultName(decoded), nullptr, &event);
        else if (state_ != State::WAIT_ACK || deadlineReached(now, ackDeadline_) ||
                 !ackMatches(ack, outstanding_, config_.gatewayId))
            log_.event("FRAME_REJECT", "ack_identity_or_state_mismatch", &ack, &event);
        else finish("accepted", &event);
    } else if (event.type != RadioEventType::NONE) {
        log_.event(event.type == RadioEventType::CRC_ERROR ? "CRC_ERROR" :
                   event.type == RadioEventType::RX_TIMEOUT ? "RX_TIMEOUT" : "RADIO_ERROR",
                   "receive_or_tx_error", &outstanding_, &event, event.code);
        if (state_ == State::TRANSMITTING) finish("tx_failed", nullptr, true);
    }
    if (state_ == State::WAIT_ACK && deadlineReached(now, ackDeadline_)) finish("timeout");
    if (!allowNew) return;
    if (!radio_.receiving() && !radio_.transmitting() &&
        (recoveryAttempts_ == 0 || (recoveryAttempts_ < 3 && deadlineReached(now, recoverAt_)))) {
        const int16_t code = radio_.receive();
        if (code) {
            ++recoveryAttempts_;
            recoverAt_ = now + kRadioRecoveryUs;
            log_.event("RADIO_ERROR", "rx_start_failed", nullptr, nullptr, code);
        } else recoveryAttempts_ = 0;
    }
    if (state_ != State::IDLE || !radio_.healthy() ||
        static_cast<uint64_t>(esp_timer_get_time()) < nextHelloUs_) return;
    outstanding_ = Frame{};
    outstanding_.networkId = config_.networkId;
    outstanding_.sourceId = config_.nodeId;
    outstanding_.destinationId = config_.gatewayId;
    if (!sequence_.take(outstanding_.sequence)) {
        sequence_.begin(generate_(sequence_.bootId()));
        log_.event("SESSION_NEW", "sequence_exhausted");
        if (!sequence_.take(outstanding_.sequence)) return;
    }
    outstanding_.bootId = sequence_.bootId();
    uint8_t bytes[kMaxFrameSize];
    size_t length = 0;
    const auto encoded = encodeFrame(outstanding_, bytes, sizeof(bytes), length);
    if (encoded != CodecResult::OK) {
        log_.event("FRAME_REJECT", codecResultName(encoded), &outstanding_);
        nextHelloUs_ = static_cast<uint64_t>(esp_timer_get_time()) + uint64_t(config_.helloIntervalMs) * 1000;
        return;
    }
    const int16_t code = radio_.transmit(bytes, length, now);
    sentAtUs_ = static_cast<uint64_t>(esp_timer_get_time());
    ++attempts_;
    if (code) { log_.event("RADIO_ERROR", "hello_start_failed", &outstanding_, nullptr, code); finish("tx_start_failed", nullptr, true); }
    else {
        state_ = State::TRANSMITTING;
        log_.event("TX_START", "hello", &outstanding_, nullptr, 0, radio_.timeOnAir(length));
    }
}
}  // namespace adaptive_lora
