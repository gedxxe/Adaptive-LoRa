#pragma once
#include <cstring>
#include "common/radio/radio_transport.h"

namespace adaptive_lora::test {
struct CapturedPacket {
    size_t length = 0;
    uint8_t bytes[kMaxFrameSize]{};
};

// One operation and one captured packet per endpoint. Delivery is explicit:
// packets cannot queue through half-duplex TX or reach a receiver that is off.
class FakeRadioTransport final : public RadioTransport {
public:
    enum class Mode { IDLE, RX, TX, FAULT };
    Mode mode = Mode::IDLE;
    bool ready = true;  // Apps are constructed only after successful real begin.
    unsigned rxFailuresLeft = 0;
    bool failNextTx = false;
    unsigned rxCalls = 0;
    unsigned txCalls = 0;
    unsigned successfulTxStarts = 0;
    unsigned dropped = 0;
    unsigned contractViolations = 0;
    CapturedPacket lastTx;

    int16_t receive() override {
        ++rxCalls;
        if (transmitting()) { ++contractViolations; return -901; }
        if (!ready || rxFailuresLeft) {
            if (rxFailuresLeft) --rxFailuresLeft;
            mode = Mode::FAULT;
            return -902;
        }
        mode = Mode::RX;
        return 0;
    }
    int16_t transmit(const uint8_t* bytes, size_t length, uint32_t) override {
        ++txCalls;
        if (!bytes || length < kHeaderSize || length > kMaxFrameSize || transmitting() || !healthy()) {
            ++contractViolations;
            return -903;
        }
        lastTx.length = length;
        std::memcpy(lastTx.bytes, bytes, length);
        if (failNextTx) { failNextTx = false; mode = Mode::FAULT; return -904; }
        ++successfulTxStarts;
        mode = Mode::TX;
        return 0;
    }
    bool transmitting() const override { return mode == Mode::TX; }
    bool receiving() const override { return mode == Mode::RX; }
    bool healthy() const override { return ready && mode != Mode::FAULT; }
    uint32_t timeOnAir(size_t) override { return 1000; }  // Synthetic, never RF evidence.

    RadioEvent completeTx() {
        if (!transmitting()) { ++contractViolations; return {}; }
        mode = Mode::IDLE;
        RadioEvent event;
        event.type = RadioEventType::TX_DONE;
        return event;
    }
    RadioEvent deliver(const CapturedPacket& packet) {
        if (!receiving() || !ready) { ++dropped; return {}; }
        if (packet.length > kMaxFrameSize) { ++contractViolations; return {}; }
        mode = Mode::IDLE;
        RadioEvent event;
        event.type = RadioEventType::RX_DONE;
        event.length = packet.length;
        std::memcpy(event.bytes, packet.bytes, packet.length);
        return event;
    }
    RadioEvent error(RadioEventType type, int16_t code) {
        mode = type == RadioEventType::ERROR ? Mode::FAULT : Mode::IDLE;
        RadioEvent event;
        event.type = type;
        event.code = code;
        return event;
    }
};
}  // namespace adaptive_lora::test
