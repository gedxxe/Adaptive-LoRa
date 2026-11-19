#pragma once
#ifndef ADAPTIVE_LORA_NATIVE_TEST
#error "Native Wire adapter must never be used in firmware"
#endif
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <esp_timer.h>

struct HostWire {
    bool beginOk = true;
    bool shortWrite = false;
    uint8_t failure = 0;
    int sda = -1, scl = -1;
    uint32_t frequency = 0;
    uint16_t timeout = 0;
    uint8_t address = 0;
    size_t writes = 0, largest = 0, length = 0;
    uint8_t bytes[32]{};
    uint32_t elapsedUs = 0;

    bool begin(int data, int clock, uint32_t hz) {
        sda = data; scl = clock; frequency = hz;
        return beginOk;
    }
    void setTimeOut(uint16_t ms) { timeout = ms; }
    void beginTransmission(uint8_t target) { address = target; }
    size_t write(const uint8_t* data, size_t size) {
        length = size;
        if (size > largest) largest = size;
        if (size > sizeof(bytes)) return 0;
        std::memcpy(bytes, data, size);
        return shortWrite ? size - 1 : size;
    }
    uint8_t endTransmission() {
        ++writes;
        host_clock::advance(elapsedUs);
        return failure;
    }
};
inline HostWire& hostWire() { static HostWire wire; return wire; }
#define Wire (hostWire())
