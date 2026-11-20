#pragma once
#include <cstring>
#include "common/protocol/multiclient_protocol.h"

namespace adaptive_lora {
enum class SubmitResult { QUEUED, BUSY, TOO_LARGE, INVALID_BYTES, INACTIVE };
enum class PayloadResult { NONE, ACCEPTED, MISSING, UNKNOWN, NOT_ADMITTED };

struct PayloadDelivery {
    uint16_t source;
    uint32_t clientBoot;
    uint32_t gatewayBoot;
    uint32_t frame;
    uint16_t sequence;
    const uint8_t* bytes;
    uint8_t size;
};

class PayloadPort {
public:
    // Called synchronously once per accepted wire identity. Copy bytes before
    // returning if the application needs them later. Return false to refuse.
    using Receiver = bool (*)(void*, const PayloadDelivery&);
    explicit PayloadPort(Receiver receiver = nullptr, void* context = nullptr)
        : receiver_(receiver), context_(context) {}

    SubmitResult submit(const uint8_t* bytes, size_t length) {
        if (length > sizeof(bytes_)) return SubmitResult::TOO_LARGE;
        if (!bytes && length) return SubmitResult::INVALID_BYTES;
        if (pending_) return SubmitResult::BUSY;
        size_ = static_cast<uint8_t>(length);
        if (size_) std::memcpy(bytes_, bytes, size_);
        pending_ = true;
        result_ = PayloadResult::NONE;
        return SubmitResult::QUEUED;
    }

    bool pending() const { return pending_; }
    PayloadResult result() const { return result_; }

private:
    friend class MulticlientLink;
    void finish(PayloadResult result) { result_ = result; pending_ = false; }
    bool deliver(const PayloadDelivery& packet) { return receiver_ && receiver_(context_, packet); }

    Receiver receiver_;
    void* context_;
    uint8_t bytes_[multiclient::kMaxData]{};
    uint8_t size_ = 0;
    bool pending_ = false;
    PayloadResult result_ = PayloadResult::NONE;
};
}
