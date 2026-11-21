#pragma once
#include <Arduino.h>
#include <RadioLib.h>
#include "radio_hardware.h"

namespace adaptive_lora {
// Exactly one instance in Esp32Firmware composition. RadioLib/SPI/GPIO stays
// here; the ISR only latches a timestamp for the cooperative driver poll.
class Sx1262Hardware final : public RadioHardware {
public:
    Sx1262Hardware();
    int16_t begin() override;
    int16_t explicitHeader() override { return radio_.explicitHeader(); }
    int16_t setCRC(uint8_t length) override { return radio_.setCRC(length); }
    void enableDio1() override;
    void clearIrqSignal() override;
    bool irqSignalled() override;
    bool takeIrqTimestampUs(uint64_t& timestamp) override;
    int16_t standby() override { return radio_.standby(); }
    int16_t startReceive() override;
    int16_t startTransmit(const uint8_t* bytes, size_t length) override { return radio_.startTransmit(bytes, length); }
    int16_t finishTransmit() override { return radio_.finishTransmit(); }
    int16_t finishReceive() override { return radio_.finishReceive(); }
    uint32_t getIrqFlags() override { return radio_.getIrqFlags(); }
    size_t getPacketLength() override { return radio_.getPacketLength(); }
    int16_t readData(uint8_t* bytes, size_t length) override { return radio_.readData(bytes, length); }
    float getRSSI() override { return radio_.getRSSI(); }
    float getSNR() override { return radio_.getSNR(); }
    uint32_t getTimeOnAir(size_t length) override { return radio_.getTimeOnAir(length); }
private:
    static void IRAM_ATTR onDio1();
    static bool irq_;
    static uint64_t irqTimeUs_;
    Module module_;
    SX1262 radio_;
};
}  // namespace adaptive_lora
