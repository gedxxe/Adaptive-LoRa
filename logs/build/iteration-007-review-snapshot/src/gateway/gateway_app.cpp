#include "gateway_app.h"

namespace adaptive_lora {
GatewayApp::GatewayApp(const DeviceConfig& config, RadioTransport& radio, SerialLogger& log, BootIdGenerator generate)
    : config_(config), radio_(radio), log_(log), generate_(generate) {}
void GatewayApp::begin(uint32_t now) {
    sequence_.begin(generate_(0));
    listen(now);
}
void GatewayApp::listen(uint32_t now) {
    const int16_t code = radio_.receive();
    if (code) {
        ++recoveryAttempts_;
        recoverAt_ = now + kRadioRecoveryUs;
        log_.event("RADIO_ERROR", "rx_start_failed", nullptr, nullptr, code);
    } else recoveryAttempts_ = 0;
}
void GatewayApp::update(uint32_t now, const RadioEvent& event, bool allowNew) {
    if (event.type == RadioEventType::TX_DONE) {
        log_.event("TX_DONE", "ack_sent", &ack_);
    } else if (event.type == RadioEventType::RX_DONE) {
        Frame hello;
        const auto decoded = decodeFrame(event.bytes, event.length, config_.networkId, config_.nodeId, hello);
        log_.event("RX_DONE", "packet", decoded == CodecResult::OK ? &hello : nullptr, &event);
        if (decoded != CodecResult::OK) log_.event("FRAME_REJECT", codecResultName(decoded), nullptr, &event);
        else if (hello.type != MessageType::HELLO) log_.event("FRAME_REJECT", "expected_hello", &hello, &event);
        else if (allowNew) {
            const auto observed = tracker_.observe({hello.networkId, hello.sourceId, hello.bootId, hello.sequence});
            if (observed == SequenceResult::STALE) log_.event("FRAME_REJECT", "stale_sequence", &hello, &event);
            else {
                log_.event(observed == SequenceResult::DUPLICATE ? "DUPLICATE" : "HELLO_RX",
                           observed == SequenceResult::DUPLICATE ? "reack" : "accepted", &hello, &event);
                ack_ = Frame{};
                ack_.type = MessageType::ACK;
                ack_.networkId = config_.networkId;
                ack_.sourceId = config_.nodeId;
                ack_.destinationId = hello.sourceId;
                if (!sequence_.take(ack_.sequence)) {
                    sequence_.begin(generate_(sequence_.bootId()));
                    if (!sequence_.take(ack_.sequence)) return;
                }
                ack_.bootId = sequence_.bootId();
                setAckPayload(ack_, hello);
                uint8_t bytes[kMaxFrameSize];
                size_t length = 0;
                const auto encoded = encodeFrame(ack_, bytes, sizeof(bytes), length);
                if (encoded != CodecResult::OK) log_.event("FRAME_REJECT", codecResultName(encoded), &ack_);
                else {
                    const int16_t code = radio_.transmit(bytes, length, now);
                    log_.event(code ? "RADIO_ERROR" : "TX_START", code ? "ack_start_failed" : "ack", &ack_, nullptr, code,
                               code ? 0 : radio_.timeOnAir(length));
                    if (!code) log_.event("ACK_TX", "started", &hello);
                }
            }
        }
    } else if (event.type != RadioEventType::NONE) {
        log_.event(event.type == RadioEventType::RX_TIMEOUT ? "RX_TIMEOUT" :
                   event.type == RadioEventType::CRC_ERROR ? "CRC_ERROR" : "RADIO_ERROR",
                   "receive_or_tx_error", nullptr, &event, event.code);
    }
    if (allowNew && !radio_.transmitting() && !radio_.receiving() &&
        (recoveryAttempts_ == 0 || (recoveryAttempts_ < 3 && deadlineReached(now, recoverAt_)))) listen(now);
}
}  // namespace adaptive_lora
