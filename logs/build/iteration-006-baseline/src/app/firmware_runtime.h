#pragma once
#include <optional>
#include "nvs_config_store.h"
#include "provisioning_console.h"
#include "client/client_app.h"
#include "gateway/gateway_app.h"

namespace adaptive_lora {
class FirmwareRuntime {
public:
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
    std::optional<GatewayApp> gateway_;
    std::optional<ClientApp> client_;
    bool rebootPending_ = false;
    bool rebootStopped_ = false;
    uint32_t rebootAt_ = 0;
};
}
