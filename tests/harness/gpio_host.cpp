#include "config.h"
#include "harness.h"
#include "input/buttons.h"

#include <cstdint>

uint32_t buttons_gpio_levels() {
    uint32_t mask = 0xFFFFFFFFU;
    for (uint8_t button = 1; button <= BUTTON_COUNT; ++button) {
        if (harness_button_pressed(button)) {
            const uint8_t pin = static_cast<uint8_t>(BUTTON_GPIO_BASE + (button - 1U));
            mask &= ~(uint32_t{1} << pin);
        }
    }
    return mask;
}
