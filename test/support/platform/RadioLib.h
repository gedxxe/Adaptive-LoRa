#pragma once
#ifndef ADAPTIVE_LORA_NATIVE_TEST
#error "Native RadioLib call recorder must never be used in firmware"
#endif
#include <SPI.h>
#include <cstring>
#define IRAM_ATTR

constexpr int16_t RADIOLIB_ERR_NONE = 0, RADIOLIB_ERR_UNKNOWN = -1;
constexpr int16_t RADIOLIB_ERR_PACKET_TOO_LONG = -4, RADIOLIB_ERR_TX_TIMEOUT = -5;
constexpr int16_t RADIOLIB_ERR_RX_TIMEOUT = -6, RADIOLIB_ERR_CRC_MISMATCH = -7;
constexpr uint32_t RADIOLIB_SX126X_IRQ_TX_DONE = 1, RADIOLIB_SX126X_IRQ_RX_DONE = 2;
constexpr uint32_t RADIOLIB_SX126X_IRQ_HEADER_VALID = 0x10, RADIOLIB_SX126X_IRQ_HEADER_ERR = 0x20;
constexpr uint32_t RADIOLIB_SX126X_IRQ_CRC_ERR = 0x40, RADIOLIB_SX126X_IRQ_TIMEOUT = 0x200;
constexpr uint32_t RADIOLIB_SX126X_RX_TIMEOUT_INF = 0xFFFFFF;
// Generic IRQ positions in pinned 7.8.1: RX_DONE, HEADER_VALID/ERR, CRC_ERR, TIMEOUT.
constexpr uint32_t RADIOLIB_IRQ_RX_DEFAULT_FLAGS = (1u << 1) | (1u << 4) | (1u << 5) | (1u << 6) | (1u << 9);

class Module {
public:
    Module(int cs, int dio, int reset, int busy, HostSPI&, SPISettings settings) {
        auto& s = host_adapter::state();
        s.cs = cs; s.dio = dio; s.reset = reset; s.busy = busy;
        s.spiHz = settings.hz; s.bitOrder = settings.order; s.mode = settings.mode;
    }
};
class SX1262 {
public:
    explicit SX1262(Module*) {}
    int16_t begin(float frequency, float bandwidth, uint8_t sf, uint8_t cr,
                  uint8_t sync, int8_t power, uint16_t preamble, float tcxo, bool ldo) {
        auto& s = host_adapter::state();
        s.frequency = frequency; s.bandwidth = bandwidth; s.sf = sf; s.cr = cr;
        s.sync = sync; s.power = power; s.preamble = preamble; s.tcxo = tcxo; s.ldo = ldo;
        return s.record(host_adapter::BEGIN);
    }
    int16_t setCurrentLimit(float limit) {
        auto& s = host_adapter::state(); s.currentLimit = limit;
        return s.record(host_adapter::CURRENT_LIMIT);
    }
    int16_t explicitHeader() { return host_adapter::state().record(host_adapter::HEADER); }
    int16_t setCRC(uint8_t length) {
        host_adapter::state().crc = length; return host_adapter::state().record(host_adapter::CRC);
    }
    void setDio1Action(void (*callback)()) {
        host_adapter::state().callback = callback; host_adapter::state().record(host_adapter::IRQ);
    }
    int16_t standby() { return host_adapter::state().record(host_adapter::STANDBY); }
    int16_t setSpreadingFactor(uint8_t sf) {
        auto& s = host_adapter::state();
        const auto code = s.record(host_adapter::RETUNE);
        if (!code) s.sf = sf;
        return code;
    }
    int16_t startReceive(uint32_t timeout, uint32_t flags, uint32_t mask, size_t length) {
        auto& s = host_adapter::state(); s.rxTimeout = timeout; s.rxFlags = flags; s.rxMask = mask; s.rxLength = length;
        return s.record(host_adapter::RX);
    }
    int16_t startTransmit(const uint8_t* bytes, size_t length) {
        auto& s = host_adapter::state(); s.txLength = length;
        if (length <= sizeof(s.txBytes)) std::memcpy(s.txBytes, bytes, length);
        else s.overflow = true;
        return s.record(host_adapter::TX);
    }
    int16_t finishTransmit() { return host_adapter::state().record(host_adapter::FINISH_TX); }
    int16_t finishReceive() { return host_adapter::state().record(host_adapter::FINISH_RX); }
    uint32_t getIrqFlags() { auto& s = host_adapter::state(); s.record(host_adapter::FLAGS); return s.irqFlags; }
    size_t getPacketLength() { auto& s = host_adapter::state(); s.record(host_adapter::LENGTH); return s.packetLength; }
    int16_t readData(uint8_t* bytes, size_t length) {
        auto& s = host_adapter::state(); s.readLength = length;
        if (length <= sizeof(s.rxBytes)) std::memcpy(bytes, s.rxBytes, length);
        else s.overflow = true;
        return s.record(host_adapter::READ);
    }
    float getRSSI() { auto& s = host_adapter::state(); s.record(host_adapter::RSSI); return s.rssi; }
    float getSNR() { auto& s = host_adapter::state(); s.record(host_adapter::SNR); return s.snr; }
    uint32_t getTimeOnAir(size_t length) {
        auto& s = host_adapter::state(); s.toaLength = length; s.record(host_adapter::TOA); return s.toa;
    }
};
