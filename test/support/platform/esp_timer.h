#pragma once
#ifndef ADAPTIVE_LORA_NATIVE_TEST
#error "Native clock adapter must never be used in firmware"
#endif
#include <cstdint>

namespace host_clock {
inline int64_t& value() { static int64_t now = 0; return now; }
inline uint64_t& pendingDelay() { static uint64_t delay = 0; return delay; }
inline void set(uint64_t now) { value() = static_cast<int64_t>(now); }
inline void advance(uint64_t elapsed) { value() += static_cast<int64_t>(elapsed); }
inline void delayAfterNextRead(uint64_t elapsed) { pendingDelay() = elapsed; }
}
inline int64_t esp_timer_get_time() {
    const auto now = host_clock::value();
    host_clock::advance(host_clock::pendingDelay());
    host_clock::pendingDelay() = 0;
    return now;
}
