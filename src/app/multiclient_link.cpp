#include "multiclient_link.h"
#include "radio_config.h"
#include <cinttypes>
#include <cmath>
#include <cstdio>

namespace adaptive_lora {
using namespace multiclient;
namespace {
constexpr uint8_t kAdrConfirmations = 3;
constexpr uint8_t kAdrRecentReportsMask = 0b111;
constexpr uint8_t kAdrFirstBeforeProbe = 6;
constexpr uint8_t kAdrProbeReceiptLimit = 6;
constexpr uint8_t kAdrRollbackHold = 8;
constexpr uint8_t kAdrIdleFrames = 4;
constexpr uint32_t kRetuneRetryUs = 5000;
// Provisional SNR needed to try SF7, SF8, or SF9, including 4 dB margin.
constexpr float kLowerSfProbeSnrDb[] = {-3.5f, -6.0f, -8.5f};
static_assert(kBaseSf + sizeof(kLowerSfProbeSnrDb) / sizeof(float) == phy::maxDataSf,
              "Provide a probe threshold for each lower DATA SF");
}

MulticlientLink::MulticlientLink(const DeviceConfig& c, RadioTransport& radio, SerialLogger& log,
                                 PayloadPort& payload, BootIdSource bootId, Roster roster, bool adaptDataSf)
    : config_(c), radio_(radio), log_(log), payload_(payload), bootIdSource_(bootId),
      adaptDataSf_(kAdaptive && adaptDataSf), roster_(roster) {
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
        (!gatewayBoot_ || now >= beaconAt_ + roster_.periodUs + kSlotGuardUs / 2);
}

bool MulticlientLink::safeToRefreshDisplay(uint64_t now, uint32_t stepBudgetUs) const {
    if (phase_ != Phase::WAIT || radio_.transmitting()) return false;
    // In search, RX stays armed and there are no local TX slots to protect.
    // FirmwareRuntime polls the radio between the bounded display steps.
    if (!boot_ || searchingForBeacon(now)) return true;
    // Local completion does not make peer slots available for display work.
    return now >= displayQuietAt_ && now < displayQuietUntil_ &&
        displayQuietUntil_ - now >= uint64_t(stepBudgetUs) + kSlotGuardUs / 2;
}

void MulticlientLink::observePeer(uint64_t now, uint16_t peer, const RadioEvent& event) {
    telemetry_.peer = {true, event.metrics, peer, now, event.rssi, event.snr};
    telemetry_.peerPeriodUs = roster_.periodUs;
}

void MulticlientLink::logLinkSamples(bool timingOk) {
    for (uint8_t i = 0; i < plan_.count; ++i) {
        if (!plan_.slots[i].boot) continue;
        const auto& rx = received_[i];
        const char* outcome = rx.result == Reception::NONE ? "missed" : rx.retried ? "retry" : "first";
        const char* application = rx.result == Reception::NONE ? "n/a" :
            rx.result == Reception::ACCEPTED ? "accepted" : "refused";
        char metrics[48] = "rssi_dbm=N/A,snr_db=N/A";
        if (rx.metrics) std::snprintf(metrics, sizeof(metrics), "rssi_dbm=%.1f,snr_db=%.1f", rx.rssi, rx.snr);
        char line[304];
        const bool sampleTimingOk = timingOk && !rx.rxWindowUnobserved;
        const auto* action = !timingOk ? "timing_invalid" : rx.rxWindowUnobserved ? "rx_window_unobserved" :
            (adaptDataSf_ ? reviewProfile(i) : "fixed_sf");
        const auto& adr = adr_[kAdaptive ? i : 0];
        std::snprintf(line, sizeof(line), "event=V3_LINK_SAMPLE,network_id=%u,gateway_id=%u,gateway_boot=%" PRIu32
            ",frame_id=%" PRIu32 ",source_id=%u,client_boot=%" PRIu32 ",sf=%u,outcome=%s,app=%s,%s",
            unsigned(config_.networkId), unsigned(config_.nodeId), boot_, frameId_, unsigned(roster_.nodes[i].id),
            plan_.slots[i].boot, unsigned(plan_.slots[i].sf), outcome, application, metrics);
        if (kAdaptive) {
            const auto prefix = std::strlen(line);
            std::snprintf(line + prefix, sizeof(line) - prefix, ",next_sf=%u,locked=%u,adr=%s",
                unsigned(adr.desired), unsigned(adr.locked), action);
        }
        const auto prefix = std::strlen(line);
        char rxTime[24] = "N/A";
        if (rx.result != Reception::NONE)
            std::snprintf(rxTime, sizeof(rxTime), "%" PRIu64, rx.completedAt);
        std::snprintf(line + prefix, sizeof(line) - prefix, ",timing=%s,payload_bytes=%u,rx_us=%s",
            sampleTimingOk ? "ok" : "invalid", unsigned(plan_.slots[i].bytes), rxTime);
        log_.text(line);
    }
}

bool MulticlientLink::prepareSf(uint8_t sf, uint64_t now) {
    if (radioSf_ == sf) return true;
    if (radio_.transmitting() || now < nextRetune_) return false;
    const int16_t code = radio_.setSpreadingFactor(sf);
    radioSf_ = code ? 0 : sf;
    phyReadinessPending_ = !code;
    phyRequestedAt_ = now;
    if (!code) ++telemetry_.retunes;
    if (code) {
        nextRetune_ = now + kRetuneRetryUs;
        frameTimingOk_ = false;
        ++telemetry_.radioErrors;
        log_.event("ADR_RADIO", "retune_error", nullptr, nullptr, code);
    }
    // Radio SPI and RX rearm take time. Launch decisions use the next loop clock.
    return false;
}

void MulticlientLink::admitProfiles() {
    // Reserve the full roster at its declared payload ceilings, including
    // inactive Clients and rollback profiles. A new probe cannot take the
    // budget reserved for a peer, even if that peer requests more bytes later.
    Plan reserved;
    reserved.count = roster_.count;
    for (uint8_t i = 0; i < plan_.count; ++i) {
        const auto& slot = plan_.slots[i];
        auto& state = adr_[i];
        if (slot.boot && state.boot != slot.boot) {
            state = {};
        } else if (state.lastRequest && frameId_ - state.lastRequest >= kAdrIdleFrames) {
            // Free airtime and discard old samples. Preserve the session and
            // last delivery so the next Request can still report its receipt.
            AdrState inactive;
            inactive.boot = state.boot;
            inactive.deliveredFrame = state.deliveredFrame;
            inactive.deliveredSf = state.deliveredSf;
            inactive.recoverySf = state.recoverySf;
            state = inactive;
        }
        if (slot.boot) {
            if (!state.lastRequest) state.desired = state.recoverySf;
            state.boot = slot.boot;
            state.lastRequest = frameId_;
        }
        reserved.slots[i] = {1, roster_.nodes[i].budget, state.reservedSf};
    }
    // Rotate only the first choice among unreserved upgrades.
    for (uint8_t step = 0; step < plan_.count; ++step) {
        const auto i = uint8_t((frameId_ % plan_.count + step) % plan_.count);
        auto& slot = plan_.slots[i];
        if (!slot.boot) continue;
        auto& state = adr_[i];
        while (state.reservedSf < state.desired) {
            ++reserved.slots[i].sf;
            if (!reserved.matches(roster_)) {
                --reserved.slots[i].sf;
                break;
            }
            ++state.reservedSf;
        }
        slot.sf = state.desired < state.reservedSf ? state.desired : state.reservedSf;
    }
}

const char* MulticlientLink::reviewProfile(uint8_t i) {
    auto& state = adr_[i];
    const auto& rx = received_[i];
    if (rx.result == Reception::REFUSED) return "app_refused";
    const auto observedSf = plan_.slots[i].sf;
    const bool clientConfirmed = state.acknowledgedSf == observedSf;
    if (state.observedSf != observedSf) {
        state.observedSf = observedSf;
        state.consecutiveFirst = state.degradedHistory = 0;
        state.hasSnr = false;
    }
    const bool first = rx.result == Reception::ACCEPTED && !rx.retried;
    if (observedSf != state.desired) {
        if (!first) state.consecutiveFirst = 0;
        else {
            if (state.consecutiveFirst < kAdrConfirmations) ++state.consecutiveFirst;
            if (state.consecutiveFirst >= kAdrConfirmations && clientConfirmed) {
                state.desired = observedSf;
                state.recoverySf = observedSf;
                state.locked = true;
                return "locked_within_budget";
            }
        }
        return "budget_limited";
    }
    if (state.probeReturn && (!first ||
        (!clientConfirmed && state.consecutiveFirst + 1 >= kAdrProbeReceiptLimit))) {
        state.desired = state.probeReturn;
        state.probeReturn = 0;
        state.consecutiveFirst = state.degradedHistory = 0;
        state.hold = kAdrRollbackHold;
        return first ? "probe_no_receipt" : "rollback";
    }
    state.degradedHistory = uint8_t(((state.degradedHistory << 1) | !first) & kAdrRecentReportsMask);
    const char* action = "observe";
    if (first) {
        if (rx.metrics && std::isfinite(rx.snr)) {
            state.filteredSnr = state.hasSnr ? 0.75f * state.filteredSnr + 0.25f * rx.snr : rx.snr;
            state.hasSnr = true;
        }
        if (state.consecutiveFirst < kAdrFirstBeforeProbe) ++state.consecutiveFirst;
        if (state.consecutiveFirst >= kAdrConfirmations && !state.locked && clientConfirmed) {
            state.probeReturn = 0;
            state.reservedSf = state.desired;
            state.recoverySf = observedSf;
            state.locked = true;
            action = "locked";
        }
        if (state.hold) --state.hold;
        else if (state.locked && clientConfirmed && state.desired > kBaseSf &&
                 state.consecutiveFirst >= kAdrFirstBeforeProbe &&
                 state.hasSnr && state.filteredSnr >= kLowerSfProbeSnrDb[state.desired - kBaseSf - 1]) {
            state.probeReturn = state.desired--;
            state.consecutiveFirst = 0;
            state.locked = false;
            action = "probe_lower";
        }
    } else {
        state.consecutiveFirst = 0;
        // A mask has at least two misses if clearing its lowest bit leaves another bit.
        if (state.degradedHistory & (state.degradedHistory - 1)) {
            state.locked = false;
            state.degradedHistory = 0;
            if (state.desired < phy::maxDataSf) {
                ++state.desired;
                action = "step_up";
            } else action = "ceiling";
        }
    }
    return action;
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
    log_.event(event, code ? "tx_error" : "started", &f, nullptr, code,
               kAdaptive ? airAtSf(length, radioSf_) : airtimeUs(length), radioSf_);
    return !code;
}

void MulticlientLink::update(uint64_t now, const RadioEvent& event, bool allowTx) {
    if (!validConfig_ || !allowTx) return;
    if (kAdaptive && phyReadinessPending_ && (radio_.receiving() || event.type == RadioEventType::RX_DONE)) {
        phyReadinessPending_ = false;
        const auto elapsed = now - phyRequestedAt_;
        if (elapsed > telemetry_.worstPhyReadyUs)
            telemetry_.worstPhyReadyUs = elapsed > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(elapsed);
    }
    const bool isGateway = config_.role == DeviceRole::GATEWAY;
    if (pendingBitmap_.seen && !radio_.transmitting()) {
        const auto bitmap = pendingBitmap_;
        pendingBitmap_ = {};
        const auto completedAt = event.completedAt(now);
        const bool sent = event.type == RadioEventType::TX_DONE;
        const bool timely = sent && bitmap.frame == frameId_ &&
            receiveWindow(completedAt, dataAt_ + plan_.feedbackStart(bitmap.round),
                          payloadAir(feedbackBytes(plan_.count)));
        if (sent) telemetry_.bitmap = bitmap;
        if (!timely) frameTimingOk_ = false;
        // A late completion cannot authorize a different round or a slot that has started.
        if (bitmap.retries) {
            if (timely && phase_ == Phase::DATA && bitmap.round + 1 == round_ &&
                now <= dataAt_ + plan_.roundStart(round_)) mask_ = bitmap.retries;
            else frameTimingOk_ = false;
        }
        if (bitmap.frame == frameId_ && bitmap.round == kAttempts - 1)
            logLinkSamples(frameTimingOk_ && completedAt < nextBeacon_);
    }
    const bool renew = isGateway && frameId_ == UINT32_MAX && now >= nextBeacon_ && !radio_.transmitting();
    if ((!boot_ || renew) && now >= nextBoot_) {
        nextBoot_ = now + kSessionRecoveryUs;
        const auto fresh = bootIdSource_(boot_);
        if (fresh && fresh != boot_) {
            if (renew) for (auto& peer : adr_) peer = {};
            boot_ = fresh;
            frameId_ = 0;
        }
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
        const bool control = phase_ != Phase::DATA ||
            (isGateway ? now >= nextBeacon_ : searchingForBeacon(now));
        if (!kAdaptive || !control || prepareSf(kControlSf, now)) {
            if (isGateway) gateway(now, event, incoming);
            else client(now, incoming, event);
        }
    }
    if ((!kAdaptive || radioSf_) && !radio_.transmitting() && !radio_.receiving() && now >= nextRecovery_) {
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
        if (phase_ == Phase::DATA) logLinkSamples(false);
        phase_ = Phase::WAIT;
        if (frameId_ == UINT32_MAX) return;
        ++frameId_;
        nextBeacon_ = now + roster_.periodUs;
        displayQuietUntil_ = nextBeacon_;
        // If beacon TX fails, reserve a request window in case it went on air.
        displayQuietAt_ = now + payloadAir(rosterBytes(roster_.count)) +
            grantStart(roster_.count) + kSlotGuardUs / 2;
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
        rxReadyMask_ = 0;
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
            if (kAdaptive) {
                auto& state = adr_[i];
                const auto receipt = get32(f.payload + 5);
                state.acknowledgedSf = state.boot == f.bootId && receipt && receipt == state.deliveredFrame ?
                    state.deliveredSf : 0;
                // One verified receipt can teach a more robust recovery SF.
                // Lowering the hint requires a confirmed lower-SF lock.
                if (state.acknowledgedSf > state.recoverySf)
                    state.recoverySf = state.acknowledgedSf;
            }
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
    if (adaptDataSf_) admitProfiles();
    if (kAdaptive && !plan_.matches(roster_)) {
        log_.event("V3_GRANT_TX", "adaptive_schedule_unavailable");
        return;
    }
    auto grant = frame(MessageType::SLOT_GRANT, 0);
    writePlan(grant, plan_);
    controlStart_ = now;
    // A reported grant TX failure can still leave peers with a usable grant.
    displayQuietAt_ = now + payloadAir(grantBytes(plan_.count)) + kSlotGuardUs / 2 +
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
            (kAdaptive && radioSf_ != plan_.slots[i].sf) ||
            !receiveWindow(receivedAt, dataAt_ + plan_.dataStart(round_, i, mask_),
                           dataAir(plan_.slots[i].bytes, plan_.slots[i].sf))) {
            log_.event("V3_DROP", "data_identity_size_or_slot", &f, &event);
        } else {
            const auto* bytes = f.payload + kPayloadPrefix;
            const auto size = plan_.slots[i].bytes;
            auto& received = received_[i];
            const char* result;
            if (received.result == Reception::NONE) {
                observePeer(receivedAt, f.sourceId, event);
                telemetry_.lastDataSf = radioSf_;
                rxReadyMask_ |= slotBit(i);
                ++telemetry_.data;
                if (round_) ++telemetry_.retries;
                std::memcpy(received.bytes, bytes, size);
                received.completedAt = receivedAt;
                received.rssi = event.rssi;
                received.snr = event.snr;
                received.metrics = event.metrics;
                received.retried = round_ != 0;
                const bool accepted = payload_.deliver({f.sourceId, f.bootId, boot_, f.frameId, f.sequence, bytes, size});
                received.result = accepted ? Reception::ACCEPTED : Reception::REFUSED;
                if (accepted) {
                    ++telemetry_.accepted;
                    if (kAdaptive) {
                        adr_[i].deliveredFrame = frameId_;
                        adr_[i].deliveredSf = plan_.slots[i].sf;
                    }
                } else ++telemetry_.refused;
                result = accepted ? "accepted" : "application_refused";
            } else {
                const bool same = std::memcmp(received.bytes, bytes, size) == 0;
                if (same) ++telemetry_.duplicates;
                result = same ? "duplicate" : "conflict";
            }
            log_.event("V3_DATA_RX", result, &f, &event, 0, 0, radioSf_);
        }
    }
    const auto start = dataAt_ + plan_.feedbackStart(round_);
    if (kAdaptive) {
        uint8_t nextSf = radioSf_;
        for (uint8_t i = 0; i < plan_.count; ++i) {
            if (!(mask_ & slotBit(i))) continue;
            const auto slot = dataAt_ + plan_.dataStart(round_, i, mask_);
            if (now + kRetuneLeadUs < slot) break;
            nextSf = plan_.slots[i].sf;
            if (now <= slot && radioSf_ == nextSf && radio_.receiving()) rxReadyMask_ |= slotBit(i);
        }
        if (now + kRetuneLeadUs >= start) nextSf = kControlSf;
        if (nextSf && !prepareSf(nextSf, now) && now < start + kLaunchUs) return;
    }
    if (now < start) return;
    auto ack = frame(MessageType::ACK_BITMAP, round_);
    ack.payloadLength = feedbackBytes(plan_.count);
    ack.payload[0] = plan_.count;
    uint8_t eligible = 0;
    for (uint8_t i = 0; i < plan_.count; ++i) {
        if (kAdaptive && (mask_ & slotBit(i)) && !(rxReadyMask_ & slotBit(i)))
            received_[i].rxWindowUnobserved = true;
        if (received_[i].result == Reception::ACCEPTED) {
            ack.payload[1] |= slotBit(i);
            put32(ack.payload + 3 + 4 * i, plan_.slots[i].boot);
        } else if (received_[i].result == Reception::NONE && plan_.slots[i].boot) {
            eligible |= slotBit(i);
        }
    }
    if (round_ + 1 < kAttempts) ack.payload[2] = retryMask(eligible, plan_.count, frameId_, round_);
    mask_ = 0;
    rxReadyMask_ = 0;
    if (launchWindow(now, start) && (!kAdaptive || radioSf_ == kControlSf)) {
        if (send(ack, now, "V3_BITMAP_TX")) {
            pendingBitmap_ = {true, frameId_, round_, ack.payload[1], ack.payload[2]};
        } else frameTimingOk_ = false;
    } else {
        frameTimingOk_ = false;
        log_.event("V3_BITMAP_TX", "deadline_miss", &ack);
    }
    if (++round_ == kAttempts) {
        phase_ = Phase::WAIT;
        if (!pendingBitmap_.seen) logLinkSamples(false);
    }
}

void MulticlientLink::acceptBeacon(uint64_t now, const Frame& beacon) {
    const bool sameBoot = gatewayBoot_ == beacon.bootId;
    if (beacon.bootId == retiredBoot_ || (gatewayBoot_ &&
        (sameBoot ? (beacon.frameId <= frameId_ || now + kSlotGuardUs < beaconAt_ + roster_.periodUs)
                  : now < beaconAt_ + roster_.periodUs + kSlotGuardUs))) {
        log_.event("V3_DROP", "stale_beacon", &beacon);
        return;
    }
    if (sentData_) clientFinish(PayloadResult::UNKNOWN);
    if (!sameBoot) {
        retiredBoot_ = gatewayBoot_;
        telemetry_.peer = {};
        telemetry_.bitmap = {};
        telemetry_.lastDataSf = 0;
        telemetry_.confirmedDataSf = 0;
        telemetry_.clientIssue = ClientScheduleIssue::NONE;
        acknowledgedFrame_ = 0;
    }
    gatewayBoot_ = beacon.bootId;
    frameId_ = beacon.frameId;
    beaconAt_ = now;
    phase_ = Phase::WAIT;
    readRoster(beacon, roster_);  // Already checked by the decoder.
    const auto beaconAir = payloadAir(rosterBytes(roster_.count));
    displayQuietUntil_ = now + roster_.periodUs - beaconAir;
    // With no observed grant, a grant to other nodes may have been lost.
    displayQuietAt_ = kAdaptive ? displayQuietUntil_ - kAdaptiveTailUs / 2 :
        now + requiredPeriodUs(roster_) - beaconAir - kSlotGuardUs / 2;
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
    if (result == PayloadResult::ACCEPTED) {
        ++telemetry_.accepted;
        telemetry_.confirmedDataSf = plan_.slots[index_].sf;
        acknowledgedFrame_ = frameId_;
    }
    payload_.finish(result);
    sentData_ = false;
    phase_ = Phase::WAIT;
    const char* text = result == PayloadResult::ACCEPTED ? "accepted" :
        (result == PayloadResult::MISSING ? "retry_exhausted_missing" : "retry_exhausted_unknown");
    log_.event("V3_PAYLOAD_RESULT", text, &data_, nullptr, 0, 0, plan_.slots[index_].sf);
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
    log_.event("V3_BITMAP_RX", "missing", &ack, &event, 0, 0, kControlSf);
    mask_ = p[2];
    launched_ = false;
    if (++round_ == kAttempts) {
        if (sentData_) clientFinish(PayloadResult::MISSING);
        else {
            telemetry_.clientIssue = ClientScheduleIssue::NO_DATA_TX;
            phase_ = Phase::WAIT;
        }
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
                payloadAir(feedbackBytes(grant.count)) + kSlotGuardUs / 2;
        }
    }
    if (phase_ == Phase::REQUEST) {
        const auto start = beaconAt_ + requestStart(index_);
        if (now < start) return;
        phase_ = Phase::WAIT;
        if (!launchWindow(now, start)) {
            telemetry_.clientIssue = ClientScheduleIssue::REQUEST_TX;
            log_.event("V3_REQUEST_TX", "deadline_miss");
            return;
        }
        auto req = frame(MessageType::PAYLOAD_REQUEST, uint16_t(frameId_));
        req.payloadLength = kRequestSize;
        put32(req.payload, gatewayBoot_);
        req.payload[4] = payload_.size_;
        if (kAdaptive) put32(req.payload + 5, acknowledgedFrame_);
        if (send(req, now, "V3_REQUEST_TX")) phase_ = Phase::GRANT_TX;
        else telemetry_.clientIssue = ClientScheduleIssue::REQUEST_TX;
        return;
    }
    if (phase_ == Phase::GRANT_TX) {
        const auto start = beaconAt_ + grantStart(roster_.count);
        const auto air = payloadAir(grantBytes(roster_.count));
        if (!control || control->type != MessageType::SLOT_GRANT || grant.slots[index_].boot != boot_ ||
            grant.slots[index_].bytes != payload_.size_) {
            if (now >= start + air + kSlotGuardUs / 2) {
                telemetry_.clientIssue = ClientScheduleIssue::NO_GRANT;
                log_.event("V3_GRANT_RX", "no_matching_grant");
                phase_ = Phase::WAIT;
            }
            return;
        }
        plan_ = grant;
        telemetry_.clientIssue = ClientScheduleIssue::NONE;
        if (kAdaptive) {
            char line[104];
            std::snprintf(line, sizeof(line), "event=ADR_APPLY,frame_id=%" PRIu32 ",node_id=%u,sf=%u",
                frameId_, unsigned(config_.nodeId), unsigned(plan_.slots[index_].sf));
            log_.text(line);
        }
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
    while (feedbackAt >= dataAt_ + plan_.feedbackStart(round_) + ackAir + kSlotGuardUs / 2) {
        log_.event("V3_BITMAP_RX", "unknown", &data_);
        mask_ = 0;  // Without this round's grant, the retry slot cannot be inferred.
        launched_ = false;
        if (++round_ == kAttempts) {
            if (sentData_) clientFinish(PayloadResult::UNKNOWN);
            else {
                telemetry_.clientIssue = ClientScheduleIssue::NO_DATA_TX;
                phase_ = Phase::WAIT;
            }
            return;
        }
    }
    if (control && control->type == MessageType::ACK_BITMAP && control->sequence == round_ &&
        receiveWindow(receivedAt, dataAt_ + plan_.feedbackStart(round_), ackAir)) {
        acceptFeedback(receivedAt, *control, event);
        return;
    }
    const auto start = dataAt_ + plan_.dataStart(round_, index_, mask_);
    const bool assigned = (mask_ & slotBit(index_)) && !launched_;
    if (kAdaptive) {
        const auto sf = assigned && now + kRetuneLeadUs >= start && now < start + kLaunchUs ?
            plan_.slots[index_].sf : kControlSf;
        if (!prepareSf(sf, now) && !(assigned && now >= start + kLaunchUs)) return;
    }
    if (assigned && now >= start) {
        launched_ = true;
        if (!launchWindow(now, start)) { log_.event("V3_DATA_TX", "deadline_miss", &data_); return; }
        if (send(data_, now, "V3_DATA_TX")) {
            sentData_ = true;
            telemetry_.lastDataSf = radioSf_;
            ++telemetry_.data;
            if (round_) ++telemetry_.retries;
        }
    }
}
}
