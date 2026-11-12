#pragma once
#ifndef ADAPTIVE_LORA_NATIVE_TEST
#error "Fake radio hardware must never be used in firmware"
#endif
#include <cstring>
#include <esp_timer.h>
#include "common/radio/radio_hardware.h"
#include "project_config.h"

namespace adaptive_lora::test {
enum class HardwareCall {
    BEGIN, HEADER, CRC, ENABLE_IRQ, CLEAR_SIGNAL, SIGNAL, STANDBY, START_RX,
    START_TX, FINISH_TX, FINISH_RX, FLAGS, LENGTH, READ, RSSI, SNR, AIRTIME, COUNT
};

// Injects API results/data only. No driver state, deadlines or event policy.
class FakeRadioHardware final : public RadioHardware {
public:
    int16_t beginCode = 0, headerCode = 0, crcCode = 0, standbyCode = 0;
    int16_t startRxCode = 0, startTxCode = 0, finishTxCode = 0, finishRxCode = 0, readCode = 0;
    bool irqFlag = false, dioHigh = false, violation = false;
    uint32_t flags = 0, airtime = 12345;
    size_t packetLength = kHeaderSize, txLength = 0, readLength = 0, airtimeLength = 0;
    uint8_t crcLength = 0;
    uint8_t rxBytes[kMaxFrameSize]{}, txBytes[kMaxFrameSize]{};
    float rssi = -87.5f, snr = 6.25f;
    HardwareCall trace[128]{};
    size_t traceLength = 0;
    unsigned calls[static_cast<size_t>(HardwareCall::COUNT)]{};
    uint64_t delayUs[static_cast<size_t>(HardwareCall::COUNT)]{};
    void (*onBegin)(void*) = nullptr;
    void* beginContext = nullptr;

    void resetTrace() { traceLength = 0; std::memset(calls, 0, sizeof(calls)); }
    unsigned count(HardwareCall call) const { return calls[static_cast<size_t>(call)]; }
    void signal(uint32_t value, bool level = true, bool latched = false) {
        flags = value; dioHigh = level; irqFlag = latched;
    }
    int16_t begin() override {
        record(HardwareCall::BEGIN);
        if (onBegin) onBegin(beginContext);
        return beginCode;
    }
    int16_t explicitHeader() override { record(HardwareCall::HEADER); return headerCode; }
    int16_t setCRC(uint8_t length) override { record(HardwareCall::CRC); crcLength = length; return crcCode; }
    void enableDio1() override { record(HardwareCall::ENABLE_IRQ); }
    void clearIrqSignal() override { record(HardwareCall::CLEAR_SIGNAL); irqFlag = false; }
    bool irqSignalled() override { record(HardwareCall::SIGNAL); return irqFlag || dioHigh; }
    int16_t standby() override { record(HardwareCall::STANDBY); return standbyCode; }
    int16_t startReceive() override { record(HardwareCall::START_RX); return startRxCode; }
    int16_t startTransmit(const uint8_t* bytes, size_t length) override {
        record(HardwareCall::START_TX);
        if (!bytes || length > sizeof(txBytes)) { violation = true; return radio_status::unknown; }
        txLength = length;
        std::memcpy(txBytes, bytes, length);
        return startTxCode;
    }
    int16_t finishTransmit() override { record(HardwareCall::FINISH_TX); return finishTxCode; }
    int16_t finishReceive() override { record(HardwareCall::FINISH_RX); return finishRxCode; }
    uint32_t getIrqFlags() override { record(HardwareCall::FLAGS); return flags; }
    size_t getPacketLength() override { record(HardwareCall::LENGTH); return packetLength; }
    int16_t readData(uint8_t* bytes, size_t length) override {
        record(HardwareCall::READ);
        if (!bytes || length > sizeof(rxBytes)) { violation = true; return radio_status::unknown; }
        readLength = length;
        if (!readCode) std::memcpy(bytes, rxBytes, length);
        return readCode;
    }
    float getRSSI() override { record(HardwareCall::RSSI); return rssi; }
    float getSNR() override { record(HardwareCall::SNR); return snr; }
    uint32_t getTimeOnAir(size_t length) override {
        record(HardwareCall::AIRTIME); airtimeLength = length; return airtime;
    }
private:
    void record(HardwareCall call) {
        ++calls[static_cast<size_t>(call)];
        host_clock::advance(delayUs[static_cast<size_t>(call)]);
        if (traceLength < sizeof(trace) / sizeof(trace[0])) trace[traceLength++] = call;
        else violation = true;
    }
};
}  // namespace adaptive_lora::test
