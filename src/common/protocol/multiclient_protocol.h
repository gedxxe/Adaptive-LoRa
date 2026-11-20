#pragma once
#include "scheduled_protocol.h"

namespace adaptive_lora::multiclient {
constexpr uint8_t kVersion = 3;
constexpr uint8_t kMaxClients = 8;
constexpr uint8_t kRetrySlots = 4;
constexpr uint8_t kAttempts = 4;
constexpr uint16_t kScheduleVersion = 1;
constexpr uint16_t kGroup = 0xFFFF;
constexpr uint8_t kAbsent = 0xFF;
constexpr uint8_t kPayloadPrefix = 4;
constexpr uint8_t kMaxData = kMaxPayloadSize - kPayloadPrefix;
constexpr uint8_t kRequestSize = kPayloadPrefix + 1;
constexpr uint32_t kTargetPeriodUs = 1000000;
constexpr uint32_t kMaxPeriodUs = 10000000;
constexpr uint32_t kGuardUs = 50000;
constexpr uint32_t kLaunchUs = kGuardUs / 4;

using scheduled::airtimeUs;
constexpr uint8_t slotBit(uint8_t index) { return uint8_t(1u << index); }
constexpr uint8_t poolLimit(uint8_t count) { return count < kRetrySlots ? count : kRetrySlots; }
constexpr uint8_t rosterBytes(uint8_t count) { return 5 + 3 * count; }
constexpr uint8_t grantBytes(uint8_t count) { return 1 + 5 * count; }
constexpr uint8_t feedbackBytes(uint8_t count) { return 3 + 4 * count; }
constexpr uint32_t payloadAir(uint8_t length) { return airtimeUs(kHeaderSize + length); }
constexpr uint32_t dataAir(uint8_t length) { return payloadAir(kPayloadPrefix + length); }

struct Candidate { uint16_t id = 0; uint8_t budget = 0; };
struct Roster {
    uint32_t periodUs = 0;  // Zero asks the gateway to derive its period from the budgets.
    uint8_t count = 0;
    Candidate nodes[kMaxClients]{};
    int indexOf(uint16_t id) const;
    bool valid(uint16_t gateway) const;
};

struct Assignment { uint32_t boot = 0; uint8_t bytes = 0; };
struct Plan {
    uint8_t count = 0;
    Assignment slots[kMaxClients]{};
    uint8_t activeMask() const;
    bool matches(const Roster& roster) const;
    uint32_t dataSpan() const;
    uint32_t retrySpan() const;
    uint32_t roundStart(uint8_t round) const;
    uint32_t feedbackStart(uint8_t round) const;
    uint32_t dataStart(uint8_t round, uint8_t index, uint8_t mask) const;
};

constexpr uint32_t requestStart(uint8_t index) {
    return kGuardUs + index * (payloadAir(kRequestSize) + kGuardUs);
}
constexpr uint32_t grantStart(uint8_t count) { return requestStart(count) + kGuardUs; }
uint32_t reservationUs(const Plan& plan);
uint32_t requiredPeriodUs(const Roster& roster);
uint8_t retryMask(uint8_t eligible, uint8_t count, uint32_t frame, uint8_t round);
bool launchWindow(uint64_t now, uint64_t start);
bool receiveWindow(uint64_t now, uint64_t start, uint32_t air);

void writeRoster(Frame& frame, const Roster& roster);
bool readRoster(const Frame& frame, Roster& roster);
void writePlan(Frame& frame, const Plan& plan);
bool readPlan(const Frame& frame, Plan& plan);
CodecResult encode(const Frame& frame, uint8_t* bytes, size_t capacity, size_t& length);
CodecResult decode(const uint8_t* bytes, size_t length, uint16_t network, uint16_t local, Frame& frame);
}
