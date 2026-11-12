#include "nvs_config_store.h"
#include <cstring>

namespace adaptive_lora {
namespace {
// Retain the schema-1 on-device namespace bytes for saved-config compatibility.
constexpr char kConfigNamespace[] = {0x6d, 0x69, 0x64, 0x65, 0x61, 0x2d, 0x70, 0x32, 0x70, 0};
}

const char* NvsConfigStore::load(DeviceConfig& config) {
    if (!storage_.begin(kConfigNamespace, true)) return "nvs_missing_or_unreadable";
    uint8_t bytes[kConfigBlobSize]{};
    const size_t length = storage_.getBytesLength("device");
    if (length != sizeof(bytes)) { storage_.end(); return "nvs_missing_or_length"; }
    const size_t read = storage_.getBytes("device", bytes, sizeof(bytes));
    storage_.end();
    if (read != sizeof(bytes)) return "nvs_read_failed";
    return decodeConfig(bytes, sizeof(bytes), config);
}
const char* NvsConfigStore::save(const DeviceConfig& config) {
    if (const char* error = validateConfig(config)) return error;
    uint8_t bytes[kConfigBlobSize]{};
    if (!encodeConfig(config, bytes, sizeof(bytes))) return "config_encode_failed";
    if (!storage_.begin(kConfigNamespace, false)) return "nvs_open_failed";
    // Keep the complete config in one blob; successful save also requires readback.
    const size_t written = storage_.putBytes("device", bytes, sizeof(bytes));
    storage_.end();
    if (written != sizeof(bytes)) return "nvs_write_failed";
    DeviceConfig verified;
    if (const char* error = load(verified)) return error;
    uint8_t readback[kConfigBlobSize]{};
    if (!encodeConfig(verified, readback, sizeof(readback)) || std::memcmp(bytes, readback, sizeof(bytes)))
        return "nvs_verify_failed";
    return nullptr;
}
}  // namespace adaptive_lora
