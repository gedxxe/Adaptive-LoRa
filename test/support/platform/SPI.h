#pragma once
#ifndef ADAPTIVE_LORA_NATIVE_TEST
#error "Native SPI adapter must never be used in firmware"
#endif
#include <cstddef>
#include <cstdint>
#include "project_config.h"

constexpr int MSBFIRST = 1, SPI_MODE0 = 0;
constexpr int SS = 8, SCK = 9, MOSI = 10, MISO = 11;

namespace host_adapter {
enum Call { SPI_BEGIN, BEGIN, CURRENT_LIMIT, HEADER, CRC, IRQ, STANDBY, RX, TX,
            FINISH_TX, FINISH_RX, FLAGS, LENGTH, READ, RSSI, SNR, TOA, RETUNE, SET_TX, COUNT };
// Records arguments and scripted return values, never chip or driver state.
struct State {
    unsigned calls[COUNT]{};
    Call trace[64]{};
    size_t traceLength = 0;
    bool overflow = false;
    int16_t codes[COUNT]{};
    int cs = -1, dio = -1, reset = -1, busy = -1;
    int spiSck = -1, spiMiso = -1, spiMosi = -1, spiCs = -1;
    uint32_t spiHz = 0;
    int bitOrder = -1, mode = -1;
    float frequency = 0, bandwidth = 0, tcxo = 0, currentLimit = 0;
    uint8_t sf = 0, cr = 0, sync = 0, crc = 0;
    int8_t power = 0;
    uint16_t preamble = 0;
    bool ldo = true, dioHigh = false;
    void (*callback)() = nullptr;
    uint32_t rxTimeout = 0, rxFlags = 0, rxMask = 0, irqFlags = 0;
    size_t rxLength = 0, packetLength = 0, txLength = 0, readLength = 0, toaLength = 0;
    uint8_t txBytes[adaptive_lora::kRadioPacketLimit]{}, rxBytes[adaptive_lora::kRadioPacketLimit]{};
    uint32_t hardwareTxTicks = 0;
    uint16_t spiCommand = 0;
    uint8_t rfMode = 0;
    float rssi = -70, snr = 8;
    uint32_t toa = 12345;
    int16_t record(Call call) {
        ++calls[call];
        if (traceLength < 64) trace[traceLength++] = call;
        else overflow = true;
        return codes[call];
    }
};
inline State& state() { static State value; return value; }
}

struct SPISettings {
    uint32_t hz;
    int order, mode;
    SPISettings(uint32_t frequency, int bitOrder, int spiMode)
        : hz(frequency), order(bitOrder), mode(spiMode) {}
};
struct HostSPI {
    void begin(int sck, int miso, int mosi, int cs) {
        auto& s = host_adapter::state();
        s.record(host_adapter::SPI_BEGIN);
        s.spiSck = sck; s.spiMiso = miso; s.spiMosi = mosi; s.spiCs = cs;
    }
};
inline HostSPI& hostSPI() { static HostSPI value; return value; }
#define SPI (hostSPI())
inline int digitalRead(int pin) {
    return pin == host_adapter::state().dio && host_adapter::state().dioHigh;
}
