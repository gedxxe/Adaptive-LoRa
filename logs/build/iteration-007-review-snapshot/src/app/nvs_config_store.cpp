#include "nvs_config_store.h"
#include <Preferences.h>
#include <cstring>

namespace adaptive_lora {
namespace {
// Retain the schema-1 on-device namespace bytes for saved-config compatibility.
constexpr char kConfigNamespace[] = {0x6d, 0x69, 0x64, 0x65, 0x61, 0x2d, 0x70, 0x32, 0x70, 0};
}

const char* NvsConfigStore::load(DeviceConfig& config) {
    Preferences prefs;
    if (!prefs.begin(kConfigNamespace, true)) return "nvs_missing_or_unreadable";
    uint8_t bytes[kConfigBlobSize]{};
    const size_t length = prefs.getBytesLength("device");
    if (length != sizeof(bytes)) { prefs.end(); return "nvs_missing_or_length"; }
    const size_t read = prefs.getBytes("device", bytes, sizeof(bytes));
    prefs.end();
    if (read != sizeof(bytes)) return "nvs_read_failed";
    return decodeConfig(bytes, sizeof(bytes), config);
}
const char* NvsConfigStore::save(const DeviceConfig& config) {
    if (const char* error = validateConfig(config)) return error;
    uint8_t bytes[kConfigBlobSize]{};
    if (!encodeConfig(config, bytes, sizeof(bytes))) return "config_encode_failed";
    Preferences prefs;
    if (!prefs.begin(kConfigNamespace, false)) return "nvs_open_failed";
    // One blob key avoids partially updated roles/IDs. NVS supplies atomic key updates.
    const size_t written = prefs.putBytes("device", bytes, sizeof(bytes));
    prefs.end();
    if (written != sizeof(bytes)) return "nvs_write_failed";
    DeviceConfig verified;
    if (const char* error = load(verified)) return error;
    uint8_t readback[kConfigBlobSize]{};
    if (!encodeConfig(verified, readback, sizeof(readback)) || std::memcmp(bytes, readback, sizeof(bytes)))
        return "nvs_verify_failed";
    return nullptr;
}
}  // namespace adaptive_lora
