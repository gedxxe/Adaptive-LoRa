#pragma once
#ifndef ADAPTIVE_LORA_NATIVE_TEST
#error "Native GPIO adapter must never be used in firmware"
#endif
#include <cstdint>

using gpio_num_t = int;
constexpr int GPIO_MODE_OUTPUT = 1;
struct HostOledGpio {
    int calls = 0;
    bool fail = false;
    int pins[8]{};
    int values[8]{};
    int record(int pin, int value) {
        if (calls < 8) { pins[calls] = pin; values[calls] = value; }
        ++calls;
        return fail ? -1 : 0;
    }
};
inline HostOledGpio& hostOledGpio() { static HostOledGpio gpio; return gpio; }
inline int gpio_set_level(gpio_num_t pin, uint32_t level) { return hostOledGpio().record(pin, int(level)); }
inline int gpio_set_direction(gpio_num_t pin, int) { return hostOledGpio().record(pin, 2); }
