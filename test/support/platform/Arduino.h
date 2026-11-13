#pragma once
#ifndef ADAPTIVE_LORA_NATIVE_TEST
#error "Native serial adapter must never be used in firmware"
#endif
#include <cstddef>
#include <cstdint>
#include <string>
#include <cstring>
#include <esp_timer.h>

// Captures the real SerialLogger's output. This models byte capacity/short
// writes only, not USB, interrupts, locks, a UART or elapsed write latency.
class HostSerial {
public:
    std::string output;
    int capacity = 64;
    size_t writeLimit = 128;
    size_t calls = 0;
    size_t largestWrite = 0;
    uint32_t baud = 0;
    uint64_t availableDelayUs = 0, writeDelayUs = 0;
    size_t beginCalls = 0, readCalls = 0;
    void begin(uint32_t value) { baud = value; ++beginCalls; }
    int available() const { return static_cast<int>(inputLength_ - inputHead_); }
    int read() { ++readCalls; return available() ? input_[inputHead_++] : -1; }
    bool inject(const char* bytes, size_t count) {
        if (inputHead_ == inputLength_) inputHead_ = inputLength_ = 0;
        if (!bytes || count > sizeof(input_) - inputLength_) return false;
        std::memcpy(input_ + inputLength_, bytes, count);
        inputLength_ += count;
        return true;
    }
    int availableForWrite() const { host_clock::advance(availableDelayUs); return capacity; }
    size_t write(const uint8_t* bytes, size_t length) {
        host_clock::advance(writeDelayUs);
        ++calls;
        if (length > largestWrite) largestWrite = length;
        size_t count = length < writeLimit ? length : writeLimit;
        if (capacity <= 0) count = 0;
        else if (count > static_cast<size_t>(capacity)) count = static_cast<size_t>(capacity);
        output.append(reinterpret_cast<const char*>(bytes), count);
        return count;
    }
    void reset() {
        output.clear(); capacity = 64; writeLimit = 128; calls = 0; largestWrite = 0;
        availableDelayUs = writeDelayUs = 0;
        baud = 0; beginCalls = readCalls = inputHead_ = inputLength_ = 0;
    }
private:
    uint8_t input_[4096]{};
    size_t inputHead_ = 0, inputLength_ = 0;
};
inline HostSerial& hostSerial() { static HostSerial serial; return serial; }
#define Serial (hostSerial())
inline uint32_t micros() { return static_cast<uint32_t>(host_clock::value()); }
