#pragma once
#include <Arduino.h>
#include <RadioLib.h>
#include "radio_hardware.h"

namespace adaptive_lora {
// Exactly one instance in FirmwareRuntime. All RadioLib/SPI/GPIO access stays
// here; the ISR only latches a signal for the cooperative driver poll.
class Llcc68Hardware final : public RadioHardware {
public:
    Llcc68Hardware();
    int16_t begin() override;
    int16_t explicitHeader() override { return radio_.explicitHeader(); }
    int16_t setCRC(uint8_t length) override { return radio_.setCRC(length); }
    void enableDio1() override;
    void clearIrqSignal() override { irq_ = false; }
    bool irqSignalled() override;
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
    static volatile bool irq_;
    Module module_;
    LLCC68 radio_;
};
}  // namespace adaptive_lora
