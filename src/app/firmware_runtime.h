#pragma once
#if __has_include(<optional>)
#include <optional>
#else
#include <experimental/optional>
#endif
#include "nvs_config_store.h"
#include "provisioning_console.h"
#include "client/client_app.h"
#include "gateway/gateway_app.h"
#include "common/radio/radio_driver.h"

namespace adaptive_lora {
#if __has_include(<optional>)
namespace runtime_optional = std;
#else
// Legacy native libstdc++ has the same used operations in experimental/optional.
namespace runtime_optional = std::experimental;
#endif
class FirmwareRuntime {
public:
    FirmwareRuntime(ConfigStorage& storage, RadioHardware& hardware)
        : store_(storage), radio_(hardware) {}
    void setup();
    void loop();
private:
    void command(ConsoleAction action);
    void showConfig(const DeviceConfig& config, const char* scope, const char* validity);
    void status();
    DeviceConfig loaded_{};
    const char* loadError_ = "not_loaded";
    NvsConfigStore store_;
    ProvisioningConsole console_;
    RadioDriver radio_;
    SerialLogger log_;
    runtime_optional::optional<GatewayApp> gateway_;
    runtime_optional::optional<ClientApp> client_;
    bool rebootPending_ = false;
    bool rebootStopped_ = false;
    uint32_t rebootAt_ = 0;
};
}
