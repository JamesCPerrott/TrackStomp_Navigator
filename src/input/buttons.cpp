#include "buttons.h"

#include <cstdint>

void buttons_scan(uint32_t now) {
    static_cast<void>(now);
}

bool buttons_poll_event(ButtonEvent* out) {
    static_cast<void>(out);
    return false;
}

#ifndef HOST_TEST
uint32_t buttons_gpio_levels() {
    return 0xFFFFFFFFU;
}
#endif
