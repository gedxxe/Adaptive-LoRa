#pragma once
#ifndef ADAPTIVE_LORA_NATIVE_TEST
#error "Native clock adapter must never be used in firmware"
#endif
#include <cstdint>

namespace host_clock {
inline int64_t& value() { static int64_t now = 0; return now; }
inline void set(uint64_t now) { value() = static_cast<int64_t>(now); }
}
inline int64_t esp_timer_get_time() { return host_clock::value(); }
