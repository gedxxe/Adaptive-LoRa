#pragma once
#if defined(ADAPTIVE_LORA_MULTICLIENT) && !defined(ADAPTIVE_LORA_OLED)
#error "The v3 firmware requires the board display"
#endif
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
#include "common/diagnostics/oled_status_display.h"
#if defined(ADAPTIVE_LORA_SCHEDULED)
#include "scheduled_link.h"
#endif
#if defined(ADAPTIVE_LORA_MULTICLIENT)
#include "multiclient_link.h"
#endif

namespace adaptive_lora {
#if __has_include(<optional>)
namespace runtime_optional = std;
#else
// Legacy native libstdc++ has the same used operations in experimental/optional.
namespace runtime_optional = std::experimental;
#endif
class FirmwareRuntime {
public:
#if defined(ADAPTIVE_LORA_MULTICLIENT)
    FirmwareRuntime(ConfigStorage& storage, RadioHardware& hardware, bool example = false,
                    PayloadPort::Receiver receiver = nullptr, void* context = nullptr,
                    OledStatusDisplay* display = nullptr)
        : store_(storage), radio_(hardware), payload_(receiver ? receiver : receivePayload,
                                                     receiver ? context : this), example_(example), display_(display) {}
    SubmitResult submitPayload(const uint8_t* bytes, size_t length);
    PayloadResult payloadResult() const { return payload_.result(); }
#if defined(ADAPTIVE_LORA_OLED)
    bool safeToRefreshDisplay(uint64_t now) const;
#endif
#else
    FirmwareRuntime(ConfigStorage& storage, RadioHardware& hardware,
                    OledStatusDisplay* display = nullptr)
        : store_(storage), radio_(hardware), display_(display) {}
#endif
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
#if defined(ADAPTIVE_LORA_MULTICLIENT)
    static bool receivePayload(void* context, const PayloadDelivery& packet);
    PayloadPort payload_;
    runtime_optional::optional<MulticlientLink> multicli_;
    bool example_;
    uint64_t nextReport_ = 0;
#elif defined(ADAPTIVE_LORA_SCHEDULED)
    runtime_optional::optional<ScheduledLink> scheduled_;
#else
    runtime_optional::optional<GatewayApp> gateway_;
    runtime_optional::optional<ClientApp> client_;
#endif
    bool rebootPending_ = false;
    bool rebootStopped_ = false;
    uint32_t rebootAt_ = 0;
    OledStatusDisplay* display_;
#if defined(ADAPTIVE_LORA_OLED)
    void serviceDisplay(uint64_t now);
    bool displayReadyLogged_ = false;
    bool displayErrorLogged_ = false;
#if defined(ADAPTIVE_LORA_MULTICLIENT)
    uint32_t worstDisplayStepUs_ = 0;
    uint64_t clientDisplayUntil_ = 0;
#endif
#endif
};
}
