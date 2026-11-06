#include "board_led.h"

#ifndef HOST_TEST
#include "pico/stdlib.h"

// The cmake define follows the link: pico2_w is the only target that sets
// TRACKSTOMP_PICO2_W and links pico_cyw43_arch_poll. The board header must agree.
#if defined(TRACKSTOMP_PICO2_W) && !defined(RASPBERRYPI_PICO2_W)
#error T30: PICO_BOARD is pico2_w but boards/pico2_w.h was not included
#endif
#if !defined(TRACKSTOMP_PICO2_W) && defined(RASPBERRYPI_PICO2_W)
#error T30: boards/pico2_w.h is included but this is not the pico2_w target
#endif

#if defined(RASPBERRYPI_PICO2_W)
#ifdef PICO_DEFAULT_LED_PIN
#error PICO_DEFAULT_LED_PIN must not be defined on pico2_w
#endif
#include "pico/cyw43_arch.h"
static_assert(CYW43_WL_GPIO_LED_PIN == 0, "onboard LED is CYW43 WL_GPIO0");
#else
#include "hardware/gpio.h"
// Pico 2 header LED. Not PICO_DEFAULT_LED_PIN: pico2_w does not define that name.
constexpr uint kPico2OnboardLed = 25;
#endif

namespace {

bool g_enabled = false;
bool g_level   = false;

void write_level(bool on) {
#if defined(RASPBERRYPI_PICO2_W)
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
#else
    gpio_put(kPico2OnboardLed, on);
#endif
}

} // namespace

void board_led_init(bool enabled) {
    g_enabled = enabled;
    g_level   = false;
    if (!enabled) {
        return;
    }
#if !defined(RASPBERRYPI_PICO2_W)
    gpio_init(kPico2OnboardLed);
    gpio_set_dir(kPico2OnboardLed, GPIO_OUT);
#endif
    write_level(false);
}

void board_led_set(bool on) {
    if (!g_enabled || (on == g_level)) {
        return;
    }
    g_level = on;
    write_level(on);
}
#else
void board_led_init(bool enabled) {
    static_cast<void>(enabled);
}

void board_led_set(bool on) {
    static_cast<void>(on);
}
#endif
