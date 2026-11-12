#pragma once
#ifndef ADAPTIVE_LORA_NATIVE_TEST
#error "Fake configuration storage must never be used in firmware"
#endif
#include <cstdint>
#include <cstring>
#include <esp_timer.h>
#include "app/config_storage.h"

namespace adaptive_lora::test {
enum class StorageCall { BEGIN_READ, BEGIN_WRITE, LENGTH, READ, WRITE, END };
struct StorageTrace {
    StorageCall call{};
    char identifier[16]{};  // Namespace on begin, key on byte operations.
    size_t length = 0;
};
struct StorageSession {
    bool beginOk = true;
    bool overrideLength = false;
    size_t reportedLength = 0;
    bool overrideReadCount = false;
    size_t readCount = 0;
    bool replaceReadBytes = false;
    uint8_t readBytes[32]{};
};

// Storage responses and bytes only: no schema, hash, validator, codec or store
// policy. Failure may be scripted after backing bytes have changed; no rollback
// or flash power-loss model is implied by this in-memory backend.
class FakeConfigStorage final : public ConfigStorage {
public:
    StorageSession sessions[16]{};
    StorageTrace trace[64]{};
    uint64_t delayUs[6]{};
    size_t traceLength = 0, begins = 0, successfulBegins = 0, ends = 0;
    bool active = false, readOnly = false, violation = false, exists = true;
    uint8_t bytes[32]{}, lastWrite[32]{};
    size_t length = 0, lastWriteLength = 0;
    bool overrideWriteCount = false, mutateOnWriteFailure = false;
    size_t writeCount = 0;

    bool begin(const char* name, bool ro) override {
        record(ro ? StorageCall::BEGIN_READ : StorageCall::BEGIN_WRITE, name);
        if (active || begins >= 16) { violation = true; return false; }
        session_ = begins++;
        if (!sessions[session_].beginOk) return false;
        active = true; readOnly = ro; ++successfulBegins;
        return true;
    }
    size_t getBytesLength(const char* key) override {
        record(StorageCall::LENGTH, key);
        if (!active) { violation = true; return 0; }
        const auto& plan = sessions[session_];
        return plan.overrideLength ? plan.reportedLength : (exists ? length : 0);
    }
    size_t getBytes(const char* key, void* destination, size_t capacity) override {
        record(StorageCall::READ, key, capacity);
        if (!active || !destination || capacity > sizeof(bytes)) { violation = true; return 0; }
        const auto& plan = sessions[session_];
        const size_t result = plan.overrideReadCount ? plan.readCount : (exists ? length : 0);
        size_t copied = result < capacity ? result : capacity;
        if (copied > sizeof(bytes)) { violation = true; return 0; }
        std::memcpy(destination, plan.replaceReadBytes ? plan.readBytes : bytes, copied);
        return result;
    }
    size_t putBytes(const char* key, const void* source, size_t count) override {
        record(StorageCall::WRITE, key, count);
        if (!active || readOnly || !source || count > sizeof(bytes)) { violation = true; return 0; }
        lastWriteLength = count;
        std::memcpy(lastWrite, source, count);
        const size_t result = overrideWriteCount ? writeCount : count;
        if (result == count || mutateOnWriteFailure) {
            std::memcpy(bytes, source, count); length = count; exists = true;
        }
        return result;
    }
    void end() override {
        record(StorageCall::END);
        if (!active) { violation = true; return; }
        active = false; ++ends;
    }
private:
    size_t session_ = 0;
    void record(StorageCall call, const char* identifier = nullptr, size_t count = 0) {
        host_clock::advance(delayUs[static_cast<size_t>(call)]);
        if (traceLength == sizeof(trace) / sizeof(trace[0])) { violation = true; return; }
        auto& item = trace[traceLength++];
        item.call = call; item.length = count;
        if (!identifier) return;
        for (size_t i = 0; i < sizeof(item.identifier); ++i) {
            item.identifier[i] = identifier[i];
            if (!identifier[i]) return;
        }
        violation = true;
    }
};
}  // namespace adaptive_lora::test
