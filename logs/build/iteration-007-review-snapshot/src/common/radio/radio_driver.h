#pragma once
#include <Arduino.h>
#include <RadioLib.h>
#include "radio_transport.h"

namespace adaptive_lora {
class RadioDriver final : public RadioTransport {
public:
    RadioDriver();
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
    uint32_t timeOnAir(size_t length) override { return radio_.getTimeOnAir(length); }
private:
    enum class State { OFF, IDLE, RX, TX, FAULT };
    static void IRAM_ATTR onDio1();
    static volatile bool irq_;
    Module module_;
    LLCC68 radio_;
    State state_ = State::OFF;
    bool initialized_ = false;
    uint32_t txDeadline_ = 0;
};
}  // namespace adaptive_lora
