#pragma once
#include "scheduled_protocol.h"
#include "radio_config.h"

namespace adaptive_lora::multiclient {
constexpr uint8_t kVersion = 3;
#if defined(ADAPTIVE_LORA_LINK_ADAPTATION)
constexpr bool kAdaptive = true;
#else
constexpr bool kAdaptive = false;
#endif
constexpr uint8_t kWireVersion = kAdaptive ? 4 : kVersion;
constexpr uint8_t kBaseSf = phy::dataSf;
constexpr uint8_t kControlSf = kAdaptive ? phy::controlSf : kBaseSf;
constexpr uint8_t kMaxClients = 8;
constexpr uint8_t kRetrySlots = 4;
constexpr uint8_t kAttempts = 4;
constexpr uint16_t kScheduleVersion = 1;
constexpr uint16_t kGroup = 0xFFFF;
constexpr uint8_t kAbsent = 0xFF;
constexpr uint8_t kPayloadPrefix = 4;
constexpr uint8_t kMaxData = kMaxPayloadSize - kPayloadPrefix;
constexpr uint8_t kRequestSize = kPayloadPrefix + 1 + (kAdaptive ? 4 : 0);
constexpr uint32_t kTargetPeriodUs = 1000000;
constexpr uint32_t kMaxPeriodUs = 10000000;
constexpr uint32_t kGuardUs = 50000;
constexpr uint32_t kAdaptivePeriodUs = 15000000;
constexpr uint32_t kAdaptiveTailUs = 300000;
constexpr uint32_t kCalendarLimitUs = kAdaptive ? kAdaptivePeriodUs : kMaxPeriodUs;
constexpr uint32_t kSlotGuardUs = kAdaptive ? 80000 : kGuardUs;
constexpr uint32_t kLaunchUs = kSlotGuardUs / 4;
constexpr uint32_t kRetuneLeadUs = kSlotGuardUs / 4;

using scheduled::airtimeUs;
constexpr uint8_t slotBit(uint8_t index) { return uint8_t(1u << index); }
constexpr uint8_t poolLimit(uint8_t count) { return count < kRetrySlots ? count : kRetrySlots; }
constexpr uint8_t rosterBytes(uint8_t count) { return 5 + 3 * count; }
constexpr uint8_t kGrantEntryBytes = kAdaptive ? 6 : 5;
constexpr uint8_t grantBytes(uint8_t count) { return 1 + kGrantEntryBytes * count; }
constexpr uint8_t feedbackBytes(uint8_t count) { return 3 + 4 * count; }
// BW125, CR4/5, preamble 8, explicit header, CRC and SF7–9 without LDRO.
constexpr uint32_t airAtSf(size_t bytes, uint8_t sf) {
    const auto bits = 8 * bytes + 44 - 4 * sf;
    const auto symbolGroups = (bits + 4 * sf - 1) / (4 * sf);
    return (81 + 20 * symbolGroups) * (2u << sf);
}
constexpr uint32_t payloadAir(uint8_t length) {
    return kAdaptive ? airAtSf(kHeaderSize + length, kControlSf) : airtimeUs(kHeaderSize + length);
}
constexpr uint32_t dataAir(uint8_t length, uint8_t sf = kBaseSf) {
    return kAdaptive ? airAtSf(kHeaderSize + kPayloadPrefix + length, sf) :
        payloadAir(kPayloadPrefix + length);
}
constexpr bool validDataSf(uint8_t sf) {
    return kAdaptive ? sf >= 7 && sf <= 9 : sf == kBaseSf;
}

struct Candidate { uint16_t id = 0; uint8_t budget = 0; };
struct Roster {
    uint32_t periodUs = 0;  // Zero asks the gateway to derive its period from the budgets.
    uint8_t count = 0;
    Candidate nodes[kMaxClients]{};
    int indexOf(uint16_t id) const;
    bool valid(uint16_t gateway) const;
};

struct Assignment { uint32_t boot = 0; uint8_t bytes = 0; uint8_t sf = kBaseSf; };
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
    return kSlotGuardUs + index * (payloadAir(kRequestSize) + kSlotGuardUs);
}
constexpr uint32_t grantStart(uint8_t count) { return requestStart(count) + kSlotGuardUs; }
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
