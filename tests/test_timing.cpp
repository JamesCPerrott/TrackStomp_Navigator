#include "config.h"
#include "harness.h"

// Timing/deadline exemplar. Advance across a threshold and assert each side.
// T05 replaces the command assertions with hold-at-threshold behaviour.
int main() {
    harness_reset();

    harness_press(1);
    harness_advance_to(HOLD_MS - 1U);
    REQUIRE(harness_now_ms() == HOLD_MS - 1U);
    REQUIRE_NO_COMMANDS();

    harness_advance_to(HOLD_MS);
    REQUIRE(harness_now_ms() == HOLD_MS);
    REQUIRE_NO_COMMANDS();
    return 0;
}
