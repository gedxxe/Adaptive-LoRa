#include "multiclient_link.h"
#include "radio_config.h"
#include <cinttypes>
#include <cstdio>

namespace adaptive_lora {
using namespace multiclient;

MulticlientLink::MulticlientLink(const DeviceConfig& c, RadioTransport& radio, SerialLogger& log,
                                 PayloadPort& payload, BootIdSource bootId, Roster roster)
    : config_(c), radio_(radio), log_(log), payload_(payload), bootIdSource_(bootId), roster_(roster) {
    validConfig_ = !validateConfig(c) && (c.role == DeviceRole::CLIENT || roster_.valid(c.gatewayId));
    if (validConfig_ && c.role == DeviceRole::GATEWAY && !roster_.periodUs)
        roster_.periodUs = requiredPeriodUs(roster_);
}

MulticlientTelemetry MulticlientLink::telemetry() const {
    auto snapshot = telemetry_;
    snapshot.frame = frameId_;
    snapshot.periodUs = roster_.periodUs;
    return snapshot;
}

bool MulticlientLink::searchingForBeacon(uint64_t now) const {
    return config_.role == DeviceRole::CLIENT &&
        (!gatewayBoot_ || now >= beaconAt_ + roster_.periodUs + kGuardUs / 2);
}

bool MulticlientLink::safeToRefreshDisplay(uint64_t now, uint32_t stepBudgetUs) const {
    if (phase_ != Phase::WAIT || radio_.transmitting()) return false;
    // In search, RX stays armed and there are no local TX slots to protect.
    // FirmwareRuntime polls the radio between the bounded display steps.
    if (!boot_ || searchingForBeacon(now)) return true;
    // Local completion does not make peer slots available for display work.
    return now >= displayQuietAt_ && now < displayQuietUntil_ &&
        displayQuietUntil_ - now >= uint64_t(stepBudgetUs) + kGuardUs / 2;
}

void MulticlientLink::observePeer(uint64_t now, uint16_t peer, const RadioEvent& event) {
    telemetry_.peer = {true, event.metrics, peer, now, event.rssi, event.snr};
    telemetry_.peerPeriodUs = roster_.periodUs;
}

void MulticlientLink::logLinkSamples() {
    for (uint8_t i = 0; i < plan_.count; ++i) {
        if (!plan_.slots[i].boot) continue;
        const auto& rx = received_[i];
        const char* outcome = rx.result == Reception::NONE ? "missed" : rx.retried ? "retry" : "first";
        const char* application = rx.result == Reception::NONE ? "n/a" :
            rx.result == Reception::ACCEPTED ? "accepted" : "refused";
        char metrics[48] = "rssi_dbm=N/A,snr_db=N/A";
        if (rx.metrics) std::snprintf(metrics, sizeof(metrics), "rssi_dbm=%.1f,snr_db=%.1f", rx.rssi, rx.snr);
        char line[232];
        std::snprintf(line, sizeof(line), "event=V3_LINK_SAMPLE,network_id=%u,gateway_id=%u,gateway_boot=%" PRIu32
            ",frame_id=%" PRIu32 ",source_id=%u,client_boot=%" PRIu32 ",sf=%u,outcome=%s,app=%s,%s",
            unsigned(config_.networkId), unsigned(config_.nodeId), boot_, frameId_, unsigned(roster_.nodes[i].id),
            plan_.slots[i].boot, unsigned(phy::dataSf), outcome, application, metrics);
        log_.text(line);
    }
}

Frame MulticlientLink::frame(MessageType type, uint16_t sequence) const {
    Frame f;
    f.type = type;
    f.networkId = config_.networkId;
    f.sourceId = config_.nodeId;
    f.destinationId = config_.role == DeviceRole::GATEWAY ? kGroup : config_.gatewayId;
    f.bootId = boot_;
    f.frameId = frameId_;
    f.sequence = sequence;
    f.scheduleVersion = kScheduleVersion;
    return f;
}

bool MulticlientLink::send(const Frame& f, uint64_t now, const char* event) {
    uint8_t bytes[kMaxFrameSize];
    size_t length = 0;
    const auto result = encode(f, bytes, sizeof(bytes), length);
    if (result != CodecResult::OK) { log_.event(event, codecResultName(result), &f); return false; }
    if (!radio_.healthy() || radio_.transmitting()) {
        ++telemetry_.radioErrors;
        log_.event(event, "radio_unavailable", &f);
        return false;
    }
    const int16_t code = radio_.transmit(bytes, length, static_cast<uint32_t>(now));
    if (code) ++telemetry_.radioErrors;
    log_.event(event, code ? "tx_error" : "started", &f, nullptr, code, airtimeUs(length));
    return !code;
}

void MulticlientLink::update(uint64_t now, const RadioEvent& event, bool allowTx) {
    if (!validConfig_ || !allowTx) return;
    const bool isGateway = config_.role == DeviceRole::GATEWAY;
    if (pendingBitmap_.seen && !radio_.transmitting()) {
        if (event.type == RadioEventType::TX_DONE) {
            telemetry_.bitmap = pendingBitmap_;
            // A late completion must not authorize a different round.
            if (phase_ == Phase::DATA && pendingBitmap_.frame == frameId_ &&
                pendingBitmap_.round + 1 == round_ && now <= dataAt_ + plan_.roundStart(round_))
                mask_ = pendingBitmap_.retries;
            else if (pendingBitmap_.retries) frameTimingOk_ = false;
            if (pendingBitmap_.round == kAttempts - 1 && frameTimingOk_ &&
                event.completedAt(now) < nextBeacon_)
                logLinkSamples();
        } else {
            frameTimingOk_ = false;
        }
        pendingBitmap_ = {};
    }
    const bool renew = isGateway && frameId_ == UINT32_MAX && now >= nextBeacon_ && !radio_.transmitting();
    if ((!boot_ || renew) && now >= nextBoot_) {
        nextBoot_ = now + kSessionRecoveryUs;
        const auto fresh = bootIdSource_(boot_);
        if (fresh && fresh != boot_) { boot_ = fresh; frameId_ = 0; }
        else log_.event("V3_SESSION", "unavailable");
    }
    Frame f;
    const Frame* incoming = nullptr;
    if (event.type == RadioEventType::RX_DONE) {
        const auto result = decode(event.bytes, event.length, config_.networkId, config_.nodeId, f);
        if (result == CodecResult::OK) incoming = &f;
        else log_.event("V3_DROP", codecResultName(result), nullptr, &event);
    } else if (event.type != RadioEventType::NONE && event.type != RadioEventType::TX_DONE) {
        if (event.type == RadioEventType::ERROR) frameTimingOk_ = false;
        ++telemetry_.radioErrors;
        log_.event("V3_RADIO", "event_error", nullptr, &event, event.code);
    }
    if (boot_) {
        if (isGateway) gateway(now, event, incoming);
        else client(now, incoming, event);
    }
    if (!radio_.transmitting() && !radio_.receiving() && now >= nextRecovery_) {
        const int16_t code = radio_.receive();
        nextRecovery_ = code ? now + kRadioRecoveryUs : now;
        if (code) {
            frameTimingOk_ = false;
            ++telemetry_.radioErrors;
            log_.event("V3_RADIO", "rx_start_error", nullptr, nullptr, code);
        }
    }
}

void MulticlientLink::gateway(uint64_t now, const RadioEvent& event, const Frame* incoming) {
    const auto completedAt = event.completedAt(now);
    if (now >= nextBeacon_ && !radio_.transmitting()) {
        phase_ = Phase::WAIT;
        if (frameId_ == UINT32_MAX) return;
        ++frameId_;
        nextBeacon_ = now + roster_.periodUs;
        displayQuietUntil_ = nextBeacon_;
        // If beacon TX fails, reserve a request window in case it went on air.
        displayQuietAt_ = now + payloadAir(rosterBytes(roster_.count)) +
            grantStart(roster_.count) + kGuardUs / 2;
        plan_ = Plan{};
        plan_.count = roster_.count;
        for (uint8_t i = 0; i < roster_.count; ++i) received_[i] = Received{};
        auto beacon = frame(MessageType::BEACON, 0);
        writeRoster(beacon, roster_);
        controlStart_ = now;
        if (send(beacon, now, "V3_BEACON_TX")) phase_ = Phase::BEACON_TX;
        return;
    }
    if (phase_ == Phase::BEACON_TX || phase_ == Phase::GRANT_TX) {
        if (radio_.transmitting()) return;
        const bool beacon = phase_ == Phase::BEACON_TX;
        const auto air = payloadAir(beacon ? rosterBytes(roster_.count) : grantBytes(plan_.count));
        phase_ = Phase::WAIT;
        if (event.type != RadioEventType::TX_DONE || !receiveWindow(completedAt, controlStart_, air)) {
            log_.event("V3_CONTROL", "completion_missing_or_late");
            return;
        }
        if (beacon) { beaconAt_ = completedAt; phase_ = Phase::REQUEST; return; }
        const uint64_t lastTxEnd = completedAt + plan_.feedbackStart(kAttempts - 1) +
            payloadAir(feedbackBytes(plan_.count)) + kLaunchUs;
        if (lastTxEnd >= nextBeacon_) { log_.event("V3_GRANT_TX", "deadline_miss"); return; }
        dataAt_ = completedAt;
        frameTimingOk_ = true;
        displayQuietAt_ = lastTxEnd;
        round_ = 0;
        mask_ = plan_.activeMask();
        phase_ = Phase::DATA;
        return;
    }
    if (phase_ == Phase::REQUEST) gatewayRequest(now, completedAt, incoming);
    if (phase_ == Phase::DATA) gatewayData(now, incoming, event);
}

void MulticlientLink::gatewayRequest(uint64_t now, uint64_t receivedAt, const Frame* incoming) {
    if (incoming) {
        const auto& f = *incoming;
        const int i = roster_.indexOf(f.sourceId);
        if (f.type == MessageType::PAYLOAD_REQUEST && i >= 0 && f.frameId == frameId_ &&
            get32(f.payload) == boot_ && f.payload[4] <= roster_.nodes[i].budget &&
            receiveWindow(receivedAt, beaconAt_ + requestStart(i), payloadAir(kRequestSize)) && !plan_.slots[i].boot) {
            plan_.slots[i] = {f.bootId, f.payload[4]};
        } else log_.event("V3_DROP", "request_identity_size_or_slot", &f);
    }
    const auto start = beaconAt_ + grantStart(roster_.count);
    if (now < start) return;
    phase_ = Phase::WAIT;
    displayQuietAt_ = now;
    if (!plan_.activeMask()) return;
    if (!launchWindow(now, start) || !plan_.matches(roster_)) {
        log_.event("V3_GRANT_TX", "schedule_unavailable");
        return;
    }
    auto grant = frame(MessageType::SLOT_GRANT, 0);
    writePlan(grant, plan_);
    controlStart_ = now;
    // A reported grant TX failure can still leave peers with a usable grant.
    displayQuietAt_ = now + payloadAir(grantBytes(plan_.count)) + kGuardUs / 2 +
        plan_.feedbackStart(kAttempts - 1) + payloadAir(feedbackBytes(plan_.count)) + kLaunchUs;
    if (send(grant, now, "V3_GRANT_TX")) phase_ = Phase::GRANT_TX;
}

void MulticlientLink::gatewayData(uint64_t now, const Frame* incoming, const RadioEvent& event) {
    if (incoming) {
        const auto receivedAt = event.completedAt(now);
        const auto& f = *incoming;
        const int i = roster_.indexOf(f.sourceId);
        if (f.type != MessageType::DATA || i < 0 || f.frameId != frameId_ || get32(f.payload) != boot_ ||
            !(mask_ & slotBit(i)) || f.bootId != plan_.slots[i].boot ||
            f.payloadLength != kPayloadPrefix + plan_.slots[i].bytes ||
            !receiveWindow(receivedAt, dataAt_ + plan_.dataStart(round_, i, mask_), payloadAir(f.payloadLength))) {
            log_.event("V3_DROP", "data_identity_size_or_slot", &f, &event);
        } else {
            const auto* bytes = f.payload + kPayloadPrefix;
            const auto size = plan_.slots[i].bytes;
            auto& received = received_[i];
            const char* result;
            if (received.result == Reception::NONE) {
                observePeer(receivedAt, f.sourceId, event);
                ++telemetry_.data;
                if (round_) ++telemetry_.retries;
                std::memcpy(received.bytes, bytes, size);
                received.rssi = event.rssi;
                received.snr = event.snr;
                received.metrics = event.metrics;
                received.retried = round_ != 0;
                const bool accepted = payload_.deliver({f.sourceId, f.bootId, boot_, f.frameId, f.sequence, bytes, size});
                received.result = accepted ? Reception::ACCEPTED : Reception::REFUSED;
                if (accepted) ++telemetry_.accepted;
                else ++telemetry_.refused;
                result = accepted ? "accepted" : "application_refused";
            } else {
                const bool same = std::memcmp(received.bytes, bytes, size) == 0;
                if (same) ++telemetry_.duplicates;
                result = same ? "duplicate" : "conflict";
            }
            log_.event("V3_DATA_RX", result, &f, &event);
        }
    }
    const auto start = dataAt_ + plan_.feedbackStart(round_);
    if (now < start) return;
    auto ack = frame(MessageType::ACK_BITMAP, round_);
    ack.payloadLength = feedbackBytes(plan_.count);
    ack.payload[0] = plan_.count;
    uint8_t eligible = 0;
    for (uint8_t i = 0; i < plan_.count; ++i) {
        if (received_[i].result == Reception::ACCEPTED) {
            ack.payload[1] |= slotBit(i);
            put32(ack.payload + 3 + 4 * i, plan_.slots[i].boot);
        } else if (received_[i].result == Reception::NONE && plan_.slots[i].boot) {
            eligible |= slotBit(i);
        }
    }
    if (round_ + 1 < kAttempts) ack.payload[2] = retryMask(eligible, plan_.count, frameId_, round_);
    mask_ = 0;
    if (launchWindow(now, start)) {
        if (send(ack, now, "V3_BITMAP_TX")) {
            pendingBitmap_ = {true, frameId_, round_, ack.payload[1], ack.payload[2]};
        } else frameTimingOk_ = false;
    } else {
        frameTimingOk_ = false;
        log_.event("V3_BITMAP_TX", "deadline_miss", &ack);
    }
    if (++round_ == kAttempts) phase_ = Phase::WAIT;
}

void MulticlientLink::acceptBeacon(uint64_t now, const Frame& beacon) {
    const bool sameBoot = gatewayBoot_ == beacon.bootId;
    if (beacon.bootId == retiredBoot_ || (gatewayBoot_ &&
        (sameBoot ? (beacon.frameId <= frameId_ || now + kGuardUs < beaconAt_ + roster_.periodUs)
                  : now < beaconAt_ + roster_.periodUs + kGuardUs))) {
        log_.event("V3_DROP", "stale_beacon", &beacon);
        return;
    }
    if (sentData_) clientFinish(PayloadResult::UNKNOWN);
    if (!sameBoot) {
        retiredBoot_ = gatewayBoot_;
        telemetry_.peer = {};
        telemetry_.bitmap = {};
    }
    gatewayBoot_ = beacon.bootId;
    frameId_ = beacon.frameId;
    beaconAt_ = now;
    phase_ = Phase::WAIT;
    readRoster(beacon, roster_);  // Already checked by the decoder.
    const auto beaconAir = payloadAir(rosterBytes(roster_.count));
    displayQuietUntil_ = now + roster_.periodUs - beaconAir;
    // With no observed grant, a grant to other nodes may have been lost.
    displayQuietAt_ = now + requiredPeriodUs(roster_) - beaconAir - kGuardUs / 2;
    const int index = roster_.indexOf(config_.nodeId);
    if (!payload_.pending_) return;
    if (index < 0 || payload_.size_ > roster_.nodes[index].budget) {
        telemetry_.result = PayloadResult::NOT_ADMITTED;
        payload_.finish(PayloadResult::NOT_ADMITTED);
        log_.event("V3_REQUEST_TX", index < 0 ? "node_not_on_roster" : "payload_exceeds_roster_budget", &beacon);
        return;
    }
    index_ = uint8_t(index);
    phase_ = Phase::REQUEST;
}

void MulticlientLink::clientFinish(PayloadResult result) {
    telemetry_.result = result;
    if (result == PayloadResult::ACCEPTED) ++telemetry_.accepted;
    payload_.finish(result);
    sentData_ = false;
    phase_ = Phase::WAIT;
    const char* text = result == PayloadResult::ACCEPTED ? "accepted" :
        (result == PayloadResult::MISSING ? "retry_exhausted_missing" : "retry_exhausted_unknown");
    log_.event("V3_PAYLOAD_RESULT", text, &data_);
}

void MulticlientLink::acceptFeedback(uint64_t now, const Frame& ack, const RadioEvent& event) {
    const auto* p = ack.payload;
    if (p[0] != plan_.count || ((p[1] | p[2]) & ~plan_.activeMask())) return;
    uint8_t remaining = poolLimit(plan_.count);
    for (uint8_t i = 0; i < plan_.count; ++i) {
        if ((p[1] & slotBit(i)) && get32(p + 3 + 4 * i) != plan_.slots[i].boot) return;
        if (p[2] & slotBit(i)) { if (!remaining) return; --remaining; }
    }
    observePeer(now, ack.sourceId, event);
    telemetry_.bitmap = {true, frameId_, round_, p[1], p[2]};
    if (sentData_ && (p[1] & slotBit(index_))) { clientFinish(PayloadResult::ACCEPTED); return; }
    log_.event("V3_BITMAP_RX", "missing", &ack, &event);
    mask_ = p[2];
    launched_ = false;
    if (++round_ == kAttempts) {
        if (sentData_) clientFinish(PayloadResult::MISSING);
        else phase_ = Phase::WAIT;
    }
}

void MulticlientLink::client(uint64_t now, const Frame* incoming, const RadioEvent& event) {
    const auto receivedAt = event.completedAt(now);
    const Frame* control = incoming && incoming->sourceId == config_.gatewayId ? incoming : nullptr;
    if (control && control->type == MessageType::BEACON) { acceptBeacon(receivedAt, *control); return; }
    if (searchingForBeacon(control ? receivedAt : now)) {
        // Keep the old beacon identity for replay checks. Drop its TX calendar
        // but keep queued bytes which have not been transmitted.
        if (sentData_) clientFinish(PayloadResult::UNKNOWN);
        phase_ = Phase::WAIT;
        return;
    }
    if (control && (control->bootId != gatewayBoot_ || control->frameId != frameId_)) control = nullptr;
    Plan grant;
    if (control && control->type == MessageType::SLOT_GRANT && (phase_ == Phase::WAIT || phase_ == Phase::GRANT_TX)) {
        const auto start = beaconAt_ + grantStart(roster_.count);
        const auto air = payloadAir(grantBytes(roster_.count));
        if (!receiveWindow(receivedAt, start, air) || !readPlan(*control, grant) || !grant.matches(roster_)) {
            control = nullptr;
        } else {
            // Observe grants to peers as well, without granting ourselves a slot.
            displayQuietAt_ = receivedAt + grant.feedbackStart(kAttempts - 1) +
                payloadAir(feedbackBytes(grant.count)) + kGuardUs / 2;
        }
    }
    if (phase_ == Phase::REQUEST) {
        const auto start = beaconAt_ + requestStart(index_);
        if (now < start) return;
        phase_ = Phase::WAIT;
        if (!launchWindow(now, start)) { log_.event("V3_REQUEST_TX", "deadline_miss"); return; }
        auto req = frame(MessageType::PAYLOAD_REQUEST, uint16_t(frameId_));
        req.payloadLength = kRequestSize;
        put32(req.payload, gatewayBoot_);
        req.payload[4] = payload_.size_;
        if (send(req, now, "V3_REQUEST_TX")) phase_ = Phase::GRANT_TX;
        return;
    }
    if (phase_ == Phase::GRANT_TX) {
        const auto start = beaconAt_ + grantStart(roster_.count);
        const auto air = payloadAir(grantBytes(roster_.count));
        if (!control || control->type != MessageType::SLOT_GRANT || grant.slots[index_].boot != boot_ ||
            grant.slots[index_].bytes != payload_.size_) {
            if (now >= start + air + kGuardUs / 2) phase_ = Phase::WAIT;
            return;
        }
        plan_ = grant;
        observePeer(receivedAt, control->sourceId, event);
        dataAt_ = receivedAt;
        round_ = 0;
        mask_ = plan_.activeMask();
        launched_ = false;
        data_ = frame(MessageType::DATA, uint16_t(frameId_));
        data_.payloadLength = kPayloadPrefix + payload_.size_;
        put32(data_.payload, gatewayBoot_);
        std::memcpy(data_.payload + kPayloadPrefix, payload_.bytes_, payload_.size_);
        phase_ = Phase::DATA;
        return;
    }
    if (phase_ != Phase::DATA) return;
    const auto ackAir = payloadAir(feedbackBytes(plan_.count));
    const auto feedbackAt = control && control->type == MessageType::ACK_BITMAP ? receivedAt : now;
    while (feedbackAt >= dataAt_ + plan_.feedbackStart(round_) + ackAir + kGuardUs / 2) {
        log_.event("V3_BITMAP_RX", "unknown", &data_);
        mask_ = 0;  // Without this round's grant, the retry slot cannot be inferred.
        launched_ = false;
        if (++round_ == kAttempts) {
            if (sentData_) clientFinish(PayloadResult::UNKNOWN);
            else phase_ = Phase::WAIT;
            return;
        }
    }
    if (control && control->type == MessageType::ACK_BITMAP && control->sequence == round_ &&
        receiveWindow(receivedAt, dataAt_ + plan_.feedbackStart(round_), ackAir)) {
        acceptFeedback(receivedAt, *control, event);
        return;
    }
    const auto start = dataAt_ + plan_.dataStart(round_, index_, mask_);
    if ((mask_ & slotBit(index_)) && !launched_ && now >= start) {
        launched_ = true;
        if (!launchWindow(now, start)) { log_.event("V3_DATA_TX", "deadline_miss", &data_); return; }
        if (send(data_, now, "V3_DATA_TX")) {
            sentData_ = true;
            ++telemetry_.data;
            if (round_) ++telemetry_.retries;
        }
    }
}
}
