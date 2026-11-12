#pragma once
#include <cstdint>
namespace adaptive_lora {
enum class MessageType : uint8_t {
    HELLO = 0x01, ACK = 0x02, BEACON = 0x10, DATA = 0x11, ACK_BITMAP = 0x12,
    CONFIG_PROPOSE = 0x20, CONFIG_ACCEPT = 0x21, CONFIG_COMMIT = 0x22,
    REJOIN_REQUEST = 0x30, REJOIN_GRANT = 0x31, DIAGNOSTIC = 0x7F
};
}
