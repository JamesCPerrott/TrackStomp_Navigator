#include "config.h"
#include "harness.h"
#include "input/buttons.h"

#include <cstdint>

// 15d first: 6, 7, 9 held alone must suppress. Button 8 must still HOLD.
int main() {
    const uint8_t suppressed[] = {6, 7, 9};
    for (const uint8_t id : suppressed) {
        harness_reset();
        harness_press(id);
        harness_advance_to(3000U);
        REQUIRE_NO_BUTTON_EVENTS();
        harness_release(id);
        harness_advance(DEBOUNCE_MS);
        REQUIRE_NO_BUTTON_EVENTS();
    }

    harness_reset();
    harness_press(8);
    harness_advance_to(HOLD_MS - 1U);
    REQUIRE_NO_BUTTON_EVENTS();
    harness_advance_to(HOLD_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{8, ButtonEventKind::Hold});
    harness_release(8);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{8, ButtonEventKind::Hold});

    const uint8_t capable[] = {1, 5, 10};
    for (const uint8_t id : capable) {
        harness_reset();
        harness_press(id);
        harness_advance_to(HOLD_MS);
        REQUIRE_BUTTON_EVENTS(ButtonEvent{id, ButtonEventKind::Hold});
        harness_release(id);
        harness_advance(DEBOUNCE_MS);
        REQUIRE_BUTTON_EVENTS(ButtonEvent{id, ButtonEventKind::Hold});
    }

    harness_reset();
    harness_press(1);
    harness_advance_to(HOLD_MS - 1U);
    harness_release(1);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{1, ButtonEventKind::Tap});

    harness_reset();
    harness_press(7);
    harness_advance(DEBOUNCE_MS);
    harness_release(7);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{7, ButtonEventKind::Tap});

    return 0;
}
