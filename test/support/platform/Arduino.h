#pragma once
#ifndef ADAPTIVE_LORA_NATIVE_TEST
#error "Native serial adapter must never be used in firmware"
#endif
#include <cstddef>
#include <cstdint>
#include <string>

// Captures the real SerialLogger's output. This models byte capacity/short
// writes only, not USB, interrupts, locks, a UART or elapsed write latency.
class HostSerial {
public:
    std::string output;
    int capacity = 64;
    size_t writeLimit = 128;
    size_t calls = 0;
    size_t largestWrite = 0;
    int availableForWrite() const { return capacity; }
    size_t write(const uint8_t* bytes, size_t length) {
        ++calls;
        if (length > largestWrite) largestWrite = length;
        size_t count = length < writeLimit ? length : writeLimit;
        if (capacity <= 0) count = 0;
        else if (count > static_cast<size_t>(capacity)) count = static_cast<size_t>(capacity);
        output.append(reinterpret_cast<const char*>(bytes), count);
        return count;
    }
    void reset() { output.clear(); capacity = 64; writeLimit = 128; calls = 0; largestWrite = 0; }
};
inline HostSerial& hostSerial() { static HostSerial serial; return serial; }
#define Serial (hostSerial())
