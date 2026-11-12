#pragma once
#include <cstddef>
#include <cstdint>
#include "project_config.h"

namespace adaptive_lora {
enum class RadioEventType { NONE, TX_DONE, RX_DONE, RX_TIMEOUT, CRC_ERROR, ERROR };
struct RadioEvent {
    RadioEventType type = RadioEventType::NONE;
    int16_t code = 0;
    size_t length = 0;
    uint8_t bytes[kMaxFrameSize]{};
    float rssi = 0;
    float snr = 0;
    bool metrics = false;
};

// Application boundary only. RadioDriver owns hardware initialization, polling
// and IRQ handling; host tests supply packet delivery and error events.
class RadioTransport {
public:
    virtual ~RadioTransport() = default;
    virtual int16_t receive() = 0;
    virtual int16_t transmit(const uint8_t* bytes, size_t length, uint32_t now) = 0;
    virtual bool transmitting() const = 0;
    virtual bool receiving() const = 0;
    virtual bool healthy() const = 0;
    virtual uint32_t timeOnAir(size_t length) = 0;
};
}  // namespace adaptive_lora
