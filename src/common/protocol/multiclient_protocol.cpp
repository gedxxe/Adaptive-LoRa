#include "multiclient_protocol.h"
#include <cstring>

namespace adaptive_lora::multiclient {
namespace {
bool unicast(uint16_t id) { return id && id != kGroup; }
bool groupType(MessageType type) {
    return type == MessageType::BEACON || type == MessageType::SLOT_GRANT || type == MessageType::ACK_BITMAP;
}
bool validCount(uint8_t count) { return count && count <= kMaxClients; }
uint32_t slotUs(const Assignment& slot) { return dataAir(slot.bytes, slot.sf) + kSlotGuardUs; }
}

int Roster::indexOf(uint16_t id) const {
    for (uint8_t i = 0; i < count; ++i) if (nodes[i].id == id) return i;
    return -1;
}

bool Roster::valid(uint16_t gateway) const {
    if (!validCount(count)) return false;
    for (uint8_t i = 0; i < count; ++i) {
        const auto& node = nodes[i];
        if (!unicast(node.id) || node.id == gateway || node.budget > kMaxData || indexOf(node.id) != i)
            return false;
    }
    const auto required = requiredPeriodUs(*this);
    return required <= kCalendarLimitUs &&
        (!periodUs || (periodUs <= kCalendarLimitUs && periodUs % 1000 == 0 && periodUs >= required));
}

uint8_t Plan::activeMask() const {
    uint8_t mask = 0;
    for (uint8_t i = 0; i < count; ++i) if (slots[i].boot) mask |= slotBit(i);
    return mask;
}

bool Plan::matches(const Roster& roster) const {
    if (count != roster.count) return false;
    for (uint8_t i = 0; i < count; ++i)
        if (slots[i].bytes > roster.nodes[i].budget || !validDataSf(slots[i].sf)) return false;
    return activeMask() && reservationUs(*this) + (kAdaptive ? kAdaptiveTailUs : 0) <= roster.periodUs;
}

uint32_t Plan::dataSpan() const {
    uint32_t total = 0;
    for (uint8_t i = 0; i < count; ++i) if (slots[i].boot) total += slotUs(slots[i]);
    return total;
}

uint32_t Plan::retrySpan() const {
    // Reserve a shared pool large enough for the most expensive eligible set.
    uint32_t total = 0;
    uint8_t remaining = activeMask();
    for (uint8_t rank = 0; rank < kRetrySlots && remaining; ++rank) {
        int largest = -1;
        for (uint8_t i = 0; i < count; ++i) {
            if ((remaining & slotBit(i)) && (largest < 0 || slotUs(slots[i]) > slotUs(slots[largest]))) largest = i;
        }
        total += slotUs(slots[largest]);
        remaining &= ~slotBit(static_cast<uint8_t>(largest));
    }
    return total;
}

uint32_t Plan::roundStart(uint8_t round) const {
    if (!round) return kSlotGuardUs;
    const uint32_t control = feedbackAir(count) + 2 * kSlotGuardUs;
    return kSlotGuardUs + dataSpan() + control + (round - 1) * (retrySpan() + control);
}

uint32_t Plan::feedbackStart(uint8_t round) const {
    return roundStart(round) + (round ? retrySpan() : dataSpan()) + kSlotGuardUs;
}

uint32_t Plan::dataStart(uint8_t round, uint8_t index, uint8_t mask) const {
    uint32_t start = roundStart(round);
    for (uint8_t i = 0; i < index; ++i) if (mask & slotBit(i)) start += slotUs(slots[i]);
    return start;
}

uint32_t reservationUs(const Plan& plan) {
    return beaconAir(plan.count) + grantStart(plan.count) + grantAir(plan.count) +
           plan.feedbackStart(kAttempts - 1) + feedbackAir(plan.count) + kSlotGuardUs;
}

uint32_t requiredPeriodUs(const Roster& roster) {
    if (!validCount(roster.count)) return 0;
    Plan max;
    max.count = roster.count;
    for (uint8_t i = 0; i < max.count; ++i) max.slots[i] = {1, roster.nodes[i].budget};
    const uint32_t rounded = (reservationUs(max) + (kAdaptive ? kAdaptiveTailUs : 0) + 999) / 1000 * 1000;
    return rounded < kTargetPeriodUs ? kTargetPeriodUs : rounded;
}

#if defined(ADAPTIVE_LORA_ROTATING)
Roster Rotation::select(uint16_t gateway, uint8_t budget, const uint8_t* profiles) {
    Roster roster{kAdaptivePeriodUs};
    if (count == 0 || count > kPopulationLimit || next >= count || !profiles ||
        !unicast(gateway) || budget > kMaxData)
        return {};

    Plan ceiling;
    // Finish the sweep before wrapping, so each registry position appears
    // exactly once even if SF changes alter the next group's size.
    for (uint16_t position = next; position < count && roster.count < kMaxClients; ++position) {
        const auto index = roster.count;
        const auto sf = profiles[position];
        if (!validDataSf(sf)) return {};
        ceiling.slots[index] = {1, budget, sf};
        ceiling.count = index + 1;
        if (reservationUs(ceiling) + kAdaptiveTailUs > roster.periodUs) break;
        roster.nodes[index] = {registeredId(position, gateway), budget};
        ++roster.count;
    }
    if (!roster.count) return {};
    next += roster.count;
    if (next == count) next = 0;
    return roster;
}
#endif

uint8_t retryMask(uint8_t eligible, uint8_t count, uint32_t frame, uint8_t round) {
    uint8_t assigned = 0;
    uint8_t available = kRetrySlots;
    for (uint8_t step = 0; step < count && available; ++step) {
        const auto i = uint8_t((frame % count + round + step) % count);
        if (eligible & slotBit(i)) { assigned |= slotBit(i); --available; }
    }
    return assigned;
}

bool launchWindow(uint64_t now, uint64_t start) { return now >= start && now - start < kLaunchUs; }
bool receiveWindow(uint64_t now, uint64_t start, uint32_t air) {
    const uint64_t end = start + air;
    return now >= end - kSlotGuardUs / 2 && now < end + kSlotGuardUs / 2;
}

void writeRoster(Frame& f, const Roster& roster) {
    f.payloadLength = rosterBytes(roster.count);
    put32(f.payload, roster.periodUs);
    f.payload[4] = roster.count;
    for (uint8_t i = 0; i < roster.count; ++i) {
        put16(f.payload + 5 + i * 3, roster.nodes[i].id);
        f.payload[7 + i * 3] = roster.nodes[i].budget;
    }
}

bool readRoster(const Frame& f, Roster& roster) {
    if (f.payloadLength < 5) return false;
    const uint8_t count = f.payload[4];
    if (!validCount(count) || f.payloadLength != rosterBytes(count)) return false;
    Roster result{get32(f.payload), count, {}};
    for (uint8_t i = 0; i < count; ++i) result.nodes[i] = {get16(f.payload + 5 + i * 3), f.payload[7 + i * 3]};
    if (!result.periodUs || !result.valid(f.sourceId)) return false;
    roster = result;
    return true;
}

void writePlan(Frame& f, const Plan& plan) {
    f.payloadLength = grantBytes(plan.count);
    f.payload[0] = plan.count;
    for (uint8_t i = 0; i < plan.count; ++i) {
        auto* entry = f.payload + 1 + i * kGrantEntryBytes;
        entry[0] = plan.slots[i].boot ? plan.slots[i].bytes : kAbsent;
        put32(entry + 1, plan.slots[i].boot);
        if (kAdaptive) entry[5] = plan.slots[i].boot ? plan.slots[i].sf : 0;
    }
}

bool readPlan(const Frame& f, Plan& plan) {
    if (!f.payloadLength) return false;
    const uint8_t count = f.payload[0];
    if (!validCount(count) || f.payloadLength != grantBytes(count)) return false;
    Plan result;
    result.count = count;
    for (uint8_t i = 0; i < count; ++i) {
        const auto* entry = f.payload + 1 + i * kGrantEntryBytes;
        const uint8_t length = entry[0];
        const uint32_t boot = get32(entry + 1);
        const uint8_t sf = kAdaptive ? entry[5] : kBaseSf;
        if (boot ? length > kMaxData : length != kAbsent) return false;
        if (boot) {
            if (!validDataSf(sf)) return false;
            result.slots[i] = {boot, length, sf};
        } else if (kAdaptive && sf) return false;
    }
    if (!result.activeMask()) return false;
    plan = result;
    return true;
}

namespace {
CodecResult validate(const Frame& f) {
    if (!f.networkId) return CodecResult::NETWORK;
    if (!unicast(f.sourceId)) return CodecResult::SOURCE;
    if (groupType(f.type) ? f.destinationId != kGroup : (!unicast(f.destinationId) || f.destinationId == f.sourceId))
        return CodecResult::DESTINATION;
    if (!f.bootId || !f.frameId) return CodecResult::SESSION;
    if (f.configVersion || f.scheduleVersion != kScheduleVersion) return CodecResult::RESERVED;
    if (f.payloadLength > kMaxPayloadSize) return CodecResult::LENGTH;
    const auto* p = f.payload;
    switch (f.type) {
        case MessageType::BEACON: {
            Roster roster;
            return !f.sequence && readRoster(f, roster) ? CodecResult::OK : CodecResult::PAYLOAD;
        }
        case MessageType::SLOT_GRANT: {
            Plan plan;
            return !f.sequence && readPlan(f, plan) ? CodecResult::OK : CodecResult::PAYLOAD;
        }
        case MessageType::PAYLOAD_REQUEST:
        case MessageType::DATA:
            if (f.type == MessageType::PAYLOAD_REQUEST && (f.payloadLength != kRequestSize ||
                p[4] > kMaxData || (kAdaptive && get32(p + 5) >= f.frameId)))
                return CodecResult::PAYLOAD;
            return f.payloadLength >= kPayloadPrefix && get32(p) && f.sequence == uint16_t(f.frameId)
                ? CodecResult::OK : CodecResult::PAYLOAD;
        case MessageType::ACK_BITMAP: {
            if (f.payloadLength < 3 || !validCount(p[0]) || f.payloadLength != feedbackBytes(p[0]) ||
                f.sequence >= kAttempts || (f.sequence == kAttempts - 1 && p[2]) || (p[1] & p[2]) ||
                !scheduled::validBitmap(p[0], p + 1, 1) || !scheduled::validBitmap(p[0], p + 2, 1))
                return CodecResult::PAYLOAD;
            for (uint8_t i = 0; i < p[0]; ++i) {
                if (bool(p[1] & slotBit(i)) != bool(get32(p + 3 + 4 * i))) return CodecResult::PAYLOAD;
            }
            return CodecResult::OK;
        }
        default: return CodecResult::TYPE;
    }
}
}

CodecResult encode(const Frame& f, uint8_t* b, size_t capacity, size_t& length) {
    length = 0;
    const auto result = validate(f);
    if (result != CodecResult::OK) return result;
    if (!b || capacity < kHeaderSize + f.payloadLength) return CodecResult::BUFFER;
    std::memset(b, 0, kHeaderSize);
    put16(b, 0x4D4C);
    b[2] = kWireVersion;
    b[3] = uint8_t(f.type);
    b[5] = kHeaderSize;
    put16(b + 6, f.networkId);
    put16(b + 8, f.sourceId);
    put16(b + 10, f.destinationId);
    put32(b + 12, f.bootId);
    put32(b + 16, f.frameId);
    put16(b + 20, f.sequence);
    put16(b + 24, f.scheduleVersion);
    b[26] = f.payloadLength;
    std::memcpy(b + kHeaderSize, f.payload, f.payloadLength);
    length = kHeaderSize + f.payloadLength;
    return CodecResult::OK;
}

CodecResult decode(const uint8_t* b, size_t length, uint16_t network, uint16_t local, Frame& out) {
    if (!b) return CodecResult::BUFFER;
    if (length < kHeaderSize || length > kMaxFrameSize || length != kHeaderSize + b[26]) return CodecResult::LENGTH;
    if (get16(b) != 0x4D4C) return CodecResult::MAGIC;
    if (b[2] != kWireVersion) return CodecResult::VERSION;
    if (b[5] != kHeaderSize) return CodecResult::HEADER;
    if (b[4] || b[27]) return CodecResult::RESERVED;
    if (get16(b + 6) != network) return CodecResult::NETWORK;
    const auto type = static_cast<MessageType>(b[3]);
    const uint16_t destination = get16(b + 10);
    if (destination != (groupType(type) ? kGroup : local)) return CodecResult::DESTINATION;
    Frame f;
    f.type = type;
    f.networkId = network;
    f.sourceId = get16(b + 8);
    f.destinationId = destination;
    f.bootId = get32(b + 12);
    f.frameId = get32(b + 16);
    f.sequence = get16(b + 20);
    f.configVersion = get16(b + 22);
    f.scheduleVersion = get16(b + 24);
    f.payloadLength = b[26];
    std::memcpy(f.payload, b + kHeaderSize, f.payloadLength);
    const auto result = validate(f);
    if (result == CodecResult::OK) out = f;
    return result;
}
}
