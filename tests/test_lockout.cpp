#include "config.h"
#include "harness.h"
#include "input/buttons.h"

// 19, 20, 21: hold lockout discards other buttons; phantom taps are suppressed;
// input works again after the locked button is released.
int main() {
    harness_reset();
    harness_press(1);
    harness_advance_to(HOLD_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{1, ButtonEventKind::Hold});
    harness_clear_captures();
    harness_press(6);
    harness_advance(DEBOUNCE_MS);
    harness_release(6);
    harness_advance(DEBOUNCE_MS);
    harness_press(7);
    harness_advance(DEBOUNCE_MS);
    harness_release(7);
    harness_advance(DEBOUNCE_MS);
    harness_press(3);
    harness_advance(DEBOUNCE_MS);
    harness_release(3);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_BUTTON_EVENTS();
    harness_release(1);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_BUTTON_EVENTS();

    harness_reset();
    harness_press(1);
    harness_advance_to(HOLD_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{1, ButtonEventKind::Hold});
    harness_clear_captures();
    harness_press(3);
    harness_advance(DEBOUNCE_MS);
    harness_release(1);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_BUTTON_EVENTS();
    harness_release(3);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_BUTTON_EVENTS();

    harness_reset();
    harness_press(1);
    harness_advance_to(HOLD_MS);
    harness_release(1);
    harness_advance(DEBOUNCE_MS);
    harness_clear_captures();
    harness_press(2);
    harness_advance(DEBOUNCE_MS);
    harness_release(2);
    harness_advance(DEBOUNCE_MS);
    harness_press(7);
    harness_advance(DEBOUNCE_MS);
    harness_release(7);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{2, ButtonEventKind::Tap},
                          ButtonEvent{7, ButtonEventKind::Tap});

    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(CHORD_HOLD_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{CHORD_BUTTON_A, ButtonEventKind::ChordHold});
    harness_clear_captures();
    harness_press(3);
    harness_advance(DEBOUNCE_MS);
    harness_release(3);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_BUTTON_EVENTS();
    harness_release(6);
    harness_advance(DEBOUNCE_MS);
    harness_press(3);
    harness_advance(DEBOUNCE_MS);
    harness_release(3);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_BUTTON_EVENTS();
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_BUTTON_EVENTS();
    harness_press(3);
    harness_advance(DEBOUNCE_MS);
    harness_release(3);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{3, ButtonEventKind::Tap});

    return 0;
}
