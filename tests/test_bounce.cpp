#include "config.h"
#include "harness.h"

// Bounce-injection exemplar. T04 replaces REQUIRE_NO_BUTTON_EVENTS with
// "exactly one press/tap after the train settles".
int main() {
    harness_reset();

    harness_inject_bounce(1, 20, 10);
    harness_advance(DEBOUNCE_MS);

    REQUIRE(harness_button_pressed(1));
    REQUIRE(harness_gpio_transition_count(1) >= 20U);
    REQUIRE_NO_BUTTON_EVENTS();
    return 0;
}
