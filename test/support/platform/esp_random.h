#pragma once
#ifndef ADAPTIVE_LORA_NATIVE_TEST
#error "Native random adapter must never be used in firmware"
#endif
#include <cstddef>
#include <cstdint>
struct HostRandom {
    uint32_t values[16]{};
    size_t length = 0, at = 0, calls = 0;
    bool exhausted = false;
    uint32_t fallback = 100000;
    uint32_t read() {
        ++calls;
        if (at < length) return values[at++];
        exhausted = true;
        // Keep a failing test finite if its explicit script was too short.
        return ++fallback;
    }
};
inline HostRandom& hostRandom() { static HostRandom value; return value; }
inline uint32_t esp_random() { return hostRandom().read(); }
