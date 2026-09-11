#include "config.h"
#include "harness.h"
#include "input/buttons.h"

// 24 and 26 first: aborted overlap must not accumulate; re-form starts a fresh window.
int main() {
    harness_reset();
    harness_press(6);
    harness_advance_to(1000U);
    harness_press(9);
    harness_advance_to(1500U);
    harness_release(9);
    harness_advance_to(7000U);
    REQUIRE_NO_BUTTON_EVENTS();
    harness_release(6);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_BUTTON_EVENTS();

    harness_reset();
    harness_press(6);
    harness_advance_to(1000U);
    harness_press(9);
    harness_advance_to(1500U);
    harness_release(9);
    harness_advance_to(2000U);
    harness_press(9);
    harness_advance_to(2000U + CHORD_HOLD_MS - 1U);
    REQUIRE_NO_BUTTON_EVENTS();
    harness_advance_to(2000U + CHORD_HOLD_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{CHORD_BUTTON_A, ButtonEventKind::ChordHold});

    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(CHORD_HOLD_MS - 1U);
    REQUIRE_NO_BUTTON_EVENTS();
    harness_advance_to(CHORD_HOLD_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{CHORD_BUTTON_A, ButtonEventKind::ChordHold});

    harness_reset();
    harness_press(6);
    harness_advance_to(1000U);
    harness_press(9);
    harness_advance_to(CHORD_HOLD_MS);
    REQUIRE_NO_BUTTON_EVENTS();
    harness_advance_to(1000U + CHORD_HOLD_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{CHORD_BUTTON_A, ButtonEventKind::ChordHold});

    harness_reset();
    harness_press(6);
    harness_advance_to(10000U);
    REQUIRE_NO_BUTTON_EVENTS();
    harness_release(6);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_BUTTON_EVENTS();

    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(4000U);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_BUTTON_EVENTS();

    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(1000U);
    harness_inject_bounce(9, 20, 10);
    REQUIRE(harness_gpio_transition_count(9) >= 20U);
    harness_advance_to(CHORD_HOLD_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{CHORD_BUTTON_A, ButtonEventKind::ChordHold});

    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(1000U);
    harness_press(8);
    harness_advance_to(1000U + HOLD_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{8, ButtonEventKind::Hold});
    harness_advance_to(CHORD_HOLD_MS + 1000U);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{8, ButtonEventKind::Hold});

    return 0;
}
