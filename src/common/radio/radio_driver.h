#pragma once
#include "radio_hardware.h"
#include "radio_transport.h"

namespace adaptive_lora {
class RadioDriver final : public RadioTransport {
public:
    explicit RadioDriver(RadioHardware& hardware) : hardware_(hardware) {}
    int16_t begin();
    int16_t receive() override;
    int16_t transmit(const uint8_t* bytes, size_t length, uint32_t now) override;
    int16_t stop();
    RadioEvent poll(uint32_t now);
    bool transmitting() const override { return state_ == State::TX; }
    bool receiving() const override { return state_ == State::RX; }
    bool healthy() const override { return initialized_ && state_ != State::FAULT; }
    bool initialized() const { return initialized_; }
    const char* chip() const { return "LLCC68_driver_id_not_exposed"; }
    uint32_t timeOnAir(size_t length) override { return hardware_.getTimeOnAir(length); }
private:
    enum class State { OFF, IDLE, RX, TX, FAULT };
    RadioHardware& hardware_;
    State state_ = State::OFF;
    bool initialized_ = false;
    uint32_t txDeadline_ = 0;
};
}  // namespace adaptive_lora
