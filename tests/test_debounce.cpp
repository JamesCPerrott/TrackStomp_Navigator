#include "config.h"
#include "harness.h"
#include "input/buttons.h"

#include <cstdint>

// Debounce timing, independence, rejected glitches, and criterion 57.
int main() {
    harness_reset();
    harness_press(1);
    harness_advance_to(DEBOUNCE_MS - 1U);
    REQUIRE(harness_now_ms() == DEBOUNCE_MS - 1U);
    REQUIRE_NO_BUTTON_EVENTS();
    harness_advance_to(DEBOUNCE_MS);
    REQUIRE(harness_now_ms() == DEBOUNCE_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{1, ButtonEventKind::Tap});

    harness_reset();
    harness_press(1);
    harness_advance(DEBOUNCE_MS - 1U);
    harness_release(1);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_BUTTON_EVENTS();

    harness_reset();
    harness_press(10);
    harness_inject_bounce(1, 20, 10);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{10, ButtonEventKind::Tap},
                          ButtonEvent{1, ButtonEventKind::Tap});

    harness_reset();
    for (uint32_t n = 0; n < 50U; ++n) {
        harness_clear_captures();
        harness_press(3);
        harness_advance(DEBOUNCE_MS);
        REQUIRE_BUTTON_EVENTS(ButtonEvent{3, ButtonEventKind::Tap});
        harness_release(3);
        harness_advance(DEBOUNCE_MS);
        REQUIRE_BUTTON_EVENTS(ButtonEvent{3, ButtonEventKind::Tap});
    }

    return 0;
}
