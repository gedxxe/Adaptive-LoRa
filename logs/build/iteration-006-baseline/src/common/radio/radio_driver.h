#pragma once
#include <Arduino.h>
#include <RadioLib.h>
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
class RadioDriver {
public:
    RadioDriver();
    int16_t begin();
    int16_t receive();
    int16_t transmit(const uint8_t* bytes, size_t length, uint32_t now);
    int16_t stop();
    RadioEvent poll(uint32_t now);
    bool transmitting() const { return state_ == State::TX; }
    bool receiving() const { return state_ == State::RX; }
    bool healthy() const { return initialized_ && state_ != State::FAULT; }
    bool initialized() const { return initialized_; }
    const char* chip() const { return "LLCC68_driver_id_not_exposed"; }
    uint32_t timeOnAir(size_t length) { return radio_.getTimeOnAir(length); }
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
