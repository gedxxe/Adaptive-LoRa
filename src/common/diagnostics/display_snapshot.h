#pragma once
#include <cstdint>
#include "common/config/device_config.h"

namespace adaptive_lora {
enum class LinkFreshness { WAIT, ONLINE, STALE };

struct LastReception {
    bool seen = false;
    bool metrics = false;
    uint16_t peer = 0;
    uint64_t atUs = 0;
    float rssi = 0;
    float snr = 0;

    LinkFreshness freshness(uint64_t now, uint32_t intervalMs) const {
        // Allow two expected intervals and one ACK window. Gateway freshness
        // uses its configured expectation, so set the same interval on both.
        const uint64_t window = uint64_t(intervalMs) * 2000 + kAckTimeoutUs;
        return !seen ? LinkFreshness::WAIT :
               now >= atUs && now - atUs < window ? LinkFreshness::ONLINE : LinkFreshness::STALE;
    }
};

// Diagnostic totals wrap modulo 2^32 and reset at boot.
struct GatewayTelemetry {
    LastReception hello;
    uint32_t uniqueHellos = 0;
    uint32_t duplicates = 0;
    uint32_t ackStarts = 0;
    uint32_t ackCompletions = 0;
    uint32_t radioErrors = 0;
    bool sessionUnavailable = false;
};

struct ClientTelemetry {
    LastReception ack;
    uint32_t attempts = 0;
    uint32_t successes = 0;
    uint32_t timeouts = 0;
    uint32_t txFailures = 0;
    bool sessionUnavailable = false;
};

struct DisplaySnapshot {
    DeviceConfig config;
    const char* configError = nullptr;
    const char* radioState = "OFF";
    bool radioReady = false;
    GatewayTelemetry gateway;
    ClientTelemetry client;
    uint32_t logDropped = 0;
};
}  // namespace adaptive_lora
