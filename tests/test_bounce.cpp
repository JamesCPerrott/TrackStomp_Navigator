#include "config.h"
#include "harness.h"
#include "input/buttons.h"

#include <cstdint>

// Bounce-injection: chatter of 5–20 edges over 10 ms must settle to one tap.
int main() {
    const uint8_t trains[] = {5, 8, 12, 20};
    for (const uint8_t transitions : trains) {
        harness_reset();

        harness_inject_bounce(1, transitions, 10);
        REQUIRE(harness_button_pressed(1));
        REQUIRE(harness_gpio_transition_count(1) >= transitions);
        REQUIRE_NO_BUTTON_EVENTS();

        harness_advance(DEBOUNCE_MS);
        REQUIRE_NO_BUTTON_EVENTS();
        harness_release(1);
        harness_advance(DEBOUNCE_MS);
        REQUIRE_BUTTON_EVENTS(ButtonEvent{1, ButtonEventKind::Tap});
    }
    return 0;
}
