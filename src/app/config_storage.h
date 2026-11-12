#pragma once
#include <cstddef>

namespace adaptive_lora {
// Non-owning, serialized session boundary. The caller closes each successful
// begin before reopening; boolean/count results preserve Preferences semantics.
class ConfigStorage {
public:
    virtual ~ConfigStorage() = default;
    virtual bool begin(const char* name, bool readOnly) = 0;
    virtual size_t getBytesLength(const char* key) = 0;
    virtual size_t getBytes(const char* key, void* bytes, size_t capacity) = 0;
    virtual size_t putBytes(const char* key, const void* bytes, size_t length) = 0;
    virtual void end() = 0;
};
}  // namespace adaptive_lora
