#include "preferences_config_storage.h"

namespace adaptive_lora {
bool PreferencesConfigStorage::begin(const char* name, bool readOnly) {
    return preferences_.begin(name, readOnly);
}
size_t PreferencesConfigStorage::getBytesLength(const char* key) {
    return preferences_.getBytesLength(key);
}
size_t PreferencesConfigStorage::getBytes(const char* key, void* bytes, size_t capacity) {
    return preferences_.getBytes(key, bytes, capacity);
}
size_t PreferencesConfigStorage::putBytes(const char* key, const void* bytes, size_t length) {
    return preferences_.putBytes(key, bytes, length);
}
void PreferencesConfigStorage::end() { preferences_.end(); }
}  // namespace adaptive_lora
