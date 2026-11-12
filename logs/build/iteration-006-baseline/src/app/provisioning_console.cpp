#include "provisioning_console.h"
#include <cstring>

namespace adaptive_lora {
namespace {
bool number(const char* text, uint32_t maximum, uint32_t& value) {
    if (!text || !*text) return false;
    value = 0;
    for (const char* p = text; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        const uint32_t digit = static_cast<uint32_t>(*p - '0');
        if (value > (maximum - digit) / 10) return false;
        value = value * 10 + digit;
    }
    return value != 0 && value <= maximum;
}
}
ConsoleAction ProvisioningConsole::feed(char byte) {
    if (byte == '\r' || byte == '\n') {
        if (overflow_) {
            length_ = 0;
            overflow_ = false;
            error_ = "line_too_long";
            return ConsoleAction::ERROR;
        }
        if (!length_) return ConsoleAction::NONE;
        line_[length_] = '\0';
        length_ = 0;
        return execute();
    }
    if (overflow_) return ConsoleAction::NONE;
    if (static_cast<unsigned char>(byte) < 32 || static_cast<unsigned char>(byte) > 126) {
        overflow_ = true;
        return ConsoleAction::NONE;
    }
    if (length_ + 1 >= sizeof(line_)) overflow_ = true;
    else line_[length_++] = byte;
    return ConsoleAction::NONE;
}
ConsoleAction ProvisioningConsole::execute() {
    char* tokens[6]{};
    size_t count = 0;
    char* p = line_;
    while (*p) {
        while (*p == ' ') ++p;
        if (!*p) break;
        if (count == 6) { error_ = "too_many_tokens"; return ConsoleAction::ERROR; }
        tokens[count++] = p;
        while (*p && *p != ' ') ++p;
        if (*p) *p++ = '\0';
    }
    if (count == 0) return ConsoleAction::NONE;
    if (count == 1) {
        if (!std::strcmp(tokens[0], "help")) return ConsoleAction::HELP;
        if (!std::strcmp(tokens[0], "status")) return ConsoleAction::STATUS;
        if (!std::strcmp(tokens[0], "reboot")) return ConsoleAction::REBOOT;
    }
    if (count == 2 && !std::strcmp(tokens[0], "config")) {
        if (!std::strcmp(tokens[1], "show")) return ConsoleAction::SHOW;
        if (!std::strcmp(tokens[1], "save")) return ConsoleAction::SAVE;
    }
    error_ = "invalid_command";
    if (count != 4 || std::strcmp(tokens[0], "config") || std::strcmp(tokens[1], "set"))
        return ConsoleAction::ERROR;
    DeviceConfig candidate = staging_;
    const char* key = tokens[2];
    const char* value = tokens[3];
    if (!std::strcmp(key, "role")) {
        if (!std::strcmp(value, "gateway")) candidate.role = DeviceRole::GATEWAY;
        else if (!std::strcmp(value, "client")) candidate.role = DeviceRole::CLIENT;
        else { error_ = "invalid_role"; return ConsoleAction::ERROR; }
    } else {
        uint32_t n = 0;
        const uint32_t max = !std::strcmp(key, "hello-interval-ms") ? kMaxHelloIntervalMs :
                             (!std::strcmp(key, "network-id") ? 65535 : 65534);
        if (!number(value, max, n)) { error_ = "invalid_number"; return ConsoleAction::ERROR; }
        if (!std::strcmp(key, "network-id")) candidate.networkId = static_cast<uint16_t>(n);
        else if (!std::strcmp(key, "node-id")) candidate.nodeId = static_cast<uint16_t>(n);
        else if (!std::strcmp(key, "gateway-id")) candidate.gatewayId = static_cast<uint16_t>(n);
        else if (!std::strcmp(key, "hello-interval-ms") && n >= kMinHelloIntervalMs) candidate.helloIntervalMs = n;
        else { error_ = "invalid_field_or_range"; return ConsoleAction::ERROR; }
    }
    staging_ = candidate;
    return ConsoleAction::UPDATED;
}
}  // namespace adaptive_lora
