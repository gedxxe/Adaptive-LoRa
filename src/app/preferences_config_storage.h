#pragma once
#include <Preferences.h>
#include "config_storage.h"

namespace adaptive_lora {
// Exactly one instance in FirmwareRuntime. No configuration policy belongs here.
class PreferencesConfigStorage final : public ConfigStorage {
public:
    bool begin(const char* name, bool readOnly) override;
    size_t getBytesLength(const char* key) override;
    size_t getBytes(const char* key, void* bytes, size_t capacity) override;
    size_t putBytes(const char* key, const void* bytes, size_t length) override;
    void end() override;
private:
    Preferences preferences_;
};
}  // namespace adaptive_lora
