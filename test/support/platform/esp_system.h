#pragma once
#ifndef ADAPTIVE_LORA_NATIVE_TEST
#error "Native reset adapter must never be used in firmware"
#endif
struct HostReset {
    unsigned calls = 0;
    int reason = 7;
    void restart() { ++calls; }
};
inline HostReset& hostReset() { static HostReset value; return value; }
inline int esp_reset_reason() { return hostReset().reason; }
#define ESP (hostReset())
