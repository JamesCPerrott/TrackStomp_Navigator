#include "config.h"
#include "harness.h"
#include "input/buttons.h"
#include "ui/ui.h"

#include <cstdint>

// harness_reset is a power-up even when the clock is already at 0. Each block
// leaves a module dirty at t=0 without moving the clock, resets, and asserts
// power-up behaviour. Not an exemplar: tests do not press before the first
// harness_reset or call buttons_set_setup_active.
int main() {
    // A button already down at the first scan is taken as accepted, so this is
    // the one press that needs no debounce time. Left alone, its release would
    // be classified as a tap DEBOUNCE_MS later.
    harness_press(10);
    REQUIRE(harness_now_ms() == 0U);
    REQUIRE(buttons_accepted_pressed(10));
    harness_reset();
    REQUIRE(harness_now_ms() == 0U);
    REQUIRE(!harness_button_pressed(10));
    REQUIRE(!buttons_accepted_pressed(10));
    harness_advance(DEBOUNCE_MS - 1U);
    REQUIRE_NO_BUTTON_EVENTS();
    harness_advance(1);
    REQUIRE_NO_BUTTON_EVENTS();
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_UI_EVENTS();
    REQUIRE_NO_TRANSPORT_SENDS();

    // input told that setup is active. Left set, a 1 s hold is swallowed.
    harness_reset();
    buttons_set_setup_active(true);
    REQUIRE(harness_now_ms() == 0U);
    harness_reset();
    REQUIRE(harness_now_ms() == 0U);
    harness_press(1);
    harness_advance_to(HOLD_MS - 1U);
    REQUIRE_NO_BUTTON_EVENTS();
    harness_advance_to(HOLD_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{1, ButtonEventKind::Hold});

    // Both LED engines showing the boot channel. Left running, LED 8 is lit
    // from the first pass and the panel lamp blinks after the lead gap.
    harness_reset();
    ui_indicate_channel(8);
    REQUIRE(harness_now_ms() == 0U);
    harness_reset();
    REQUIRE(harness_now_ms() == 0U);
    REQUIRE_LAMP(false);
    REQUIRE_NO_SWITCH_LEDS();
    harness_advance(1);
    REQUIRE_NO_SWITCH_LEDS();
    harness_advance_to(CHANNEL_BLINK_GAP_MS - 1U);
    REQUIRE_LAMP(false);
    harness_advance_to(CHANNEL_BLINK_GAP_MS);
    REQUIRE_LAMP(false);
    REQUIRE_NO_SWITCH_LEDS();
    for (const uint8_t sample : harness_lamp_trace()) {
        REQUIRE(sample == 0U);
    }
    for (const uint16_t mask : harness_switch_trace()) {
        REQUIRE(mask == 0U);
    }
    return 0;
}
