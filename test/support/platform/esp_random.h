#pragma once
#ifndef ADAPTIVE_LORA_NATIVE_TEST
#error "Native random adapter must never be used in firmware"
#endif
#include <cstddef>
#include <cstdint>
struct HostRandomLimit {};  // Native-only finite instrumentation of a stuck source.
struct HostRandom {
    uint32_t values[16]{};
    size_t length = 0, at = 0, calls = 0;
    bool exhausted = false, cycle = false;
    size_t guardAfter = 0;
    uint32_t fallback = 100000;
    uint32_t read() {
        if (guardAfter && calls == guardAfter) throw HostRandomLimit{};
        ++calls;
        if (cycle && length && at == length) at = 0;
        if (at < length) return values[at++];
        exhausted = true;
        // Keep a failing test finite if its explicit script was too short.
        return ++fallback;
    }
};
inline HostRandom& hostRandom() { static HostRandom value; return value; }
inline uint32_t esp_random() { return hostRandom().read(); }
