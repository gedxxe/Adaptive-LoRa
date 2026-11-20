#pragma once
#include "frame_codec.h"

namespace adaptive_lora::scheduled {
constexpr uint8_t kVersion = 2;
constexpr uint16_t kScheduleVersion = 1;
constexpr uint16_t kClientId = 2;
constexpr uint8_t kAttempts = 4;
constexpr uint8_t kDataPrefix = 4;
constexpr uint8_t kMaxData = kMaxPayloadSize - kDataPrefix;
constexpr uint8_t kDemoSize = 8;
constexpr uint8_t kBeaconSize = 6;
constexpr uint8_t kFeedbackSize = 9;
constexpr uint32_t kGuardUs = 50000;

// Fixed SF7/BW125/CR4/5, eight-symbol preamble, explicit header and CRC.
constexpr uint32_t airtimeUs(size_t wireBytes) {
    return (81 + 20 * ((8 * wireBytes + 16 + 27) / 28)) * 256;
}

struct Schedule {
    uint32_t periodUs;
    uint8_t maxData;
    uint32_t dataUs() const { return airtimeUs(kHeaderSize + kDataPrefix + maxData); }
    uint32_t dataStart(uint8_t round) const { return kGuardUs * 2 + round * stride(); }
    uint32_t feedbackStart(uint8_t round) const { return dataStart(round) + dataUs() + 4 * kGuardUs; }
    uint32_t feedbackEnd(uint8_t round) const {
        return feedbackStart(round) + airtimeUs(kHeaderSize + kFeedbackSize) + 2 * kGuardUs;
    }
    uint32_t stride() const { return dataUs() + airtimeUs(kHeaderSize + kFeedbackSize) + 8 * kGuardUs; }
    bool valid() const {
        return maxData <= kMaxData && periodUs <= 1000 * kMaxHelloIntervalMs && periodUs % 1000 == 0 &&
               feedbackEnd(kAttempts - 1) + 2 * kGuardUs +
               airtimeUs(kHeaderSize + kBeaconSize) < periodUs &&
               feedbackEnd(kAttempts - 1) < kAckTimeoutUs;
    }
};

bool validBitmap(uint8_t count, const uint8_t* bits, size_t length);
CodecResult encode(const Frame& frame, uint8_t* bytes, size_t capacity, size_t& length);
CodecResult decode(const uint8_t* bytes, size_t length, uint16_t network, uint16_t local, Frame& frame);
}
