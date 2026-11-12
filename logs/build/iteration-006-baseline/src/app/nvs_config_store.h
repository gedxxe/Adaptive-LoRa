#pragma once
#include "common/config/device_config.h"
namespace adaptive_lora {
class NvsConfigStore {
public:
    const char* load(DeviceConfig& config);
    const char* save(const DeviceConfig& config);
};
}
