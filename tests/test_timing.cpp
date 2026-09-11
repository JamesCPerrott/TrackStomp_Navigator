#include "config.h"
#include "harness.h"
#include "input/buttons.h"

// Timing/deadline: HOLD fires at HOLD_MS, not one millisecond earlier, not on release.
int main() {
    harness_reset();

    harness_press(1);
    harness_advance_to(HOLD_MS - 1U);
    REQUIRE(harness_now_ms() == HOLD_MS - 1U);
    REQUIRE_NO_BUTTON_EVENTS();

    harness_advance_to(HOLD_MS);
    REQUIRE(harness_now_ms() == HOLD_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{1, ButtonEventKind::Hold});

    harness_release(1);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{1, ButtonEventKind::Hold});
    return 0;
}
