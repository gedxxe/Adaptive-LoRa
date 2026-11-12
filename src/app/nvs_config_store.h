#pragma once
#include "common/config/device_config.h"
#include "config_storage.h"
namespace adaptive_lora {
class NvsConfigStore {
public:
    explicit NvsConfigStore(ConfigStorage& storage) : storage_(storage) {}
    const char* load(DeviceConfig& config);
    const char* save(const DeviceConfig& config);
private:
    ConfigStorage& storage_;
};
}
