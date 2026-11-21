#include "scheduled_link.h"
#include <cstring>

namespace adaptive_lora {
using namespace scheduled;

ScheduledLink::ScheduledLink(const DeviceConfig& config, RadioTransport& radio, SerialLogger& log,
                             BootIdSource bootId, uint8_t payloadBudget)
    : config_(config), radio_(radio), log_(log), bootIdSource_(bootId),
      schedule_{config.helloIntervalMs * 1000, payloadBudget} {}

bool ScheduledLink::supports(const DeviceConfig& c) {
    return !validateConfig(c) && c.gatewayId != kClientId &&
           (c.role == DeviceRole::GATEWAY || c.nodeId == kClientId) &&
           Schedule{c.helloIntervalMs * 1000, kDemoSize}.valid();
}

Frame ScheduledLink::frame(MessageType type, uint16_t sequence) const {
    Frame f;
    f.type = type;
    f.networkId = config_.networkId;
    f.sourceId = config_.nodeId;
    f.destinationId = config_.role == DeviceRole::GATEWAY ? kClientId : config_.gatewayId;
    f.bootId = bootId_;
    f.frameId = frameId_;
    f.sequence = sequence;
    f.scheduleVersion = kScheduleVersion;
    return f;
}

bool ScheduledLink::send(const Frame& outgoing, uint64_t now, const char* name) {
    uint8_t bytes[kMaxFrameSize];
    size_t length = 0;
    const auto result = encode(outgoing, bytes, sizeof(bytes), length);
    if (result != CodecResult::OK) {
        log_.event(name, codecResultName(result), &outgoing);
        return false;
    }
    const int16_t code = radio_.transmit(bytes, length, static_cast<uint32_t>(now));
    log_.event(name, code ? "tx_error" : "started", &outgoing, nullptr, code, airtimeUs(length));
    return !code;
}

void ScheduledLink::update(uint64_t now, const RadioEvent& event, bool allowTx) {
    if (!allowTx || !schedule_.valid()) return;
    if ((!bootId_ || (frameId_ == UINT32_MAX && config_.role == DeviceRole::GATEWAY && now >= nextBeacon_)) &&
        now >= nextBootAttempt_ && !radio_.transmitting()) {
        const uint32_t id = bootIdSource_(bootId_);
        nextBootAttempt_ = now + kSessionRecoveryUs;
        if (id) { bootId_ = id; frameId_ = 0; }
        else log_.event("SCHEDULE_SESSION", "unavailable");
    }
    if (event.type == RadioEventType::ERROR || event.type == RadioEventType::CRC_ERROR ||
        event.type == RadioEventType::RX_TIMEOUT)
        log_.event("SCHEDULE_RADIO", "event_error", nullptr, &event, event.code);

    Frame incoming;
    const Frame* received = nullptr;
    if (event.type == RadioEventType::RX_DONE) {
        const auto result = decode(event.bytes, event.length, config_.networkId, config_.nodeId, incoming);
        if (result == CodecResult::OK) received = &incoming;
        else log_.event("SCHEDULE_DROP", codecResultName(result), nullptr, &event);
    }
    if (bootId_) {
        if (config_.role == DeviceRole::GATEWAY) gateway(now, event, received);
        else client(now, event, received);
    }
    if (!radio_.transmitting() && !radio_.receiving() && now >= nextRxAttempt_) {
        const int16_t code = radio_.receive();
        nextRxAttempt_ = code ? now + kRadioRecoveryUs : now;
        if (code) log_.event("SCHEDULE_RADIO", "rx_start_error", nullptr, nullptr, code);
    }
}

void ScheduledLink::gateway(uint64_t now, const RadioEvent& event, const Frame* incoming) {
    if (beaconPending_ && (event.type == RadioEventType::TX_DONE || !radio_.transmitting())) {
        beaconPending_ = false;
        active_ = event.type == RadioEventType::TX_DONE && now < beaconDeadline_;
        anchor_ = now;
        if (!active_) log_.event("BEACON_TX", "completion_missing_or_late");
    }
    if (now >= nextBeacon_ && !radio_.transmitting() && frameId_ != UINT32_MAX) {
        active_ = false;
        haveData_ = false;
        round_ = 0;
        ++frameId_;
        nextBeacon_ = now + schedule_.periodUs;
        auto beacon = frame(MessageType::BEACON, 0);
        beacon.payloadLength = kBeaconSize;
        put32(beacon.payload, schedule_.periodUs);
        beacon.payload[4] = schedule_.maxData;
        beacon.payload[5] = kAttempts;
        beaconDeadline_ = now + airtimeUs(kHeaderSize + kBeaconSize) + kGuardUs;
        beaconPending_ = radio_.healthy() && send(beacon, now, "BEACON_TX");
        return;
    }
    if (!active_ || round_ >= kAttempts) return;
    if (incoming) {
        const auto& f = *incoming;
        const uint64_t start = anchor_ + schedule_.dataStart(round_);
        if (f.type == MessageType::DATA && f.sourceId == kClientId && f.frameId == frameId_ &&
            get32(f.payload) == bootId_ && f.payloadLength <= kDataPrefix + schedule_.maxData &&
            now >= start - kGuardUs + airtimeUs(kHeaderSize + f.payloadLength) &&
            now < start + 2 * kGuardUs + airtimeUs(kHeaderSize + f.payloadLength)) {
            if (!haveData_) {
                data_ = f;
                haveData_ = true;
                log_.event("DATA_RX", "accepted", &f, &event);
            } else {
                const bool same = f.bootId == data_.bootId && f.sequence == data_.sequence &&
                    f.payloadLength == data_.payloadLength && !std::memcmp(f.payload, data_.payload, f.payloadLength);
                log_.event("DATA_RX", same ? "duplicate" : "conflict", &f, &event);
            }
        } else log_.event("SCHEDULE_DROP", "data_identity_or_slot", &f, &event);
    }
    const uint64_t feedbackStart = anchor_ + schedule_.feedbackStart(round_);
    if (now < feedbackStart) return;
    if (now < feedbackStart + kGuardUs && !radio_.transmitting() && radio_.healthy()) {
        auto ack = frame(MessageType::ACK_BITMAP, round_);
        ack.payloadLength = kFeedbackSize;
        ack.payload[0] = 1;
        ack.payload[1] = 1;
        if (haveData_) {
            ack.payload[2] = 1;
            put32(ack.payload + 3, data_.bootId);
            put16(ack.payload + 7, data_.sequence);
        }
        send(ack, now, "BITMAP_TX");
    } else log_.event("BITMAP_TX", "slot_unavailable");
    ++round_;
}

void ScheduledLink::acceptBeacon(uint64_t now, const Frame& beacon, const RadioEvent& event) {
    const bool same = gatewayBoot_ == beacon.bootId;
    if (get32(beacon.payload) != schedule_.periodUs || beacon.bootId == retiredGateway_ ||
        (gatewayBoot_ && (same ? (beacon.frameId <= frameId_ || now + kGuardUs < anchor_ + schedule_.periodUs)
                              : now < anchor_ + schedule_.periodUs + kGuardUs))) {
        log_.event("SCHEDULE_DROP", "stale_or_incompatible_beacon", &beacon, &event);
        return;
    }
    if (!same) retiredGateway_ = gatewayBoot_;
    gatewayBoot_ = beacon.bootId;
    frameId_ = beacon.frameId;
    anchor_ = now;
    round_ = 0;
    active_ = true;
    attempted_ = false;
    dataSent_ = false;
    schedule_.maxData = beacon.payload[4];
    data_ = frame(MessageType::DATA, uint16_t(frameId_));
    const uint8_t length = schedule_.maxData < kDemoSize ? schedule_.maxData : kDemoSize;
    data_.payloadLength = kDataPrefix + length;
    put32(data_.payload, gatewayBoot_);
    // The research payload is the local beacon observation time in microseconds.
    for (uint8_t i = 0; i < length; ++i) data_.payload[kDataPrefix + i] = uint8_t(now >> (8 * i));
    log_.event("BEACON_RX", "synchronized", &beacon, &event);
}

void ScheduledLink::client(uint64_t now, const RadioEvent& event, const Frame* incoming) {
    if (incoming && incoming->sourceId == config_.gatewayId && incoming->type == MessageType::BEACON) {
        acceptBeacon(now, *incoming, event);
        return;
    }
    if (!active_) return;
    const uint64_t feedbackStart = anchor_ + schedule_.feedbackStart(round_);
    const uint64_t feedbackEnd = anchor_ + schedule_.feedbackEnd(round_);
    bool missing = false;
    if (incoming) {
        const auto& f = *incoming;
        if (dataSent_ && f.type == MessageType::ACK_BITMAP && f.sourceId == config_.gatewayId &&
            f.bootId == gatewayBoot_ && f.frameId == frameId_ && f.sequence == round_ &&
            now >= feedbackStart - kGuardUs + airtimeUs(kHeaderSize + kFeedbackSize) && now < feedbackEnd) {
            if (f.payload[2] && get32(f.payload + 3) == bootId_) {
                log_.event("BITMAP_RX", "received", &f, &event);
                active_ = false;
                return;
            }
            missing = !f.payload[2];
        }
        log_.event(missing ? "BITMAP_RX" : "SCHEDULE_DROP", missing ? "missing" : "feedback_identity_or_slot", &f, &event);
    }
    if (missing || now >= feedbackEnd) {
        if (!missing) log_.event("BITMAP_RX", "unknown", &data_);
        attempted_ = false;
        dataSent_ = false;
        if (++round_ == kAttempts) {
            active_ = false;
            log_.event("DATA_RESULT", "retry_exhausted", &data_);
        }
        return;
    }
    const uint64_t start = anchor_ + schedule_.dataStart(round_);
    if (!attempted_ && now >= start) {
        attempted_ = true;
        if (now < start + kGuardUs && radio_.healthy() && !radio_.transmitting())
            dataSent_ = send(data_, now, "DATA_TX");
        else log_.event("DATA_TX", "slot_unavailable", &data_);
    }
}
}
