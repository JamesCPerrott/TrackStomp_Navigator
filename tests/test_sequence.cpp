#include "config.h"
#include "harness.h"
#include "sequencer/sequencer.h"

#include <cstdint>

namespace {

void tap(uint8_t id) {
    harness_press(id);
    harness_advance(DEBOUNCE_MS);
    harness_release(id);
    harness_advance(DEBOUNCE_MS);
}

} // namespace

// Test 6 first: self-pair must beat prefix reassignment (3, 3 → note 24).
int main() {
    harness_reset();
    tap(3);
    tap(3);
    REQUIRE_COMMANDS(Command{24});

    harness_reset();
    tap(1);
    tap(6);
    REQUIRE_COMMANDS(Command{0});

    harness_reset();
    tap(5);
    tap(9);
    REQUIRE_COMMANDS(Command{19});

    harness_reset();
    tap(2);
    tap(4);
    tap(8);
    REQUIRE_COMMANDS(Command{14});

    harness_reset();
    tap(2);
    harness_advance(SEQUENCE_TIMEOUT_MS);
    REQUIRE_NO_COMMANDS();

    harness_reset();
    tap(2);
    harness_advance(SEQUENCE_TIMEOUT_MS);
    tap(6);
    REQUIRE_NO_COMMANDS();

    harness_reset();
    tap(6);
    REQUIRE_NO_COMMANDS();

    harness_reset();
    tap(10);
    REQUIRE_COMMANDS(Command{20});

    harness_reset();
    tap(2);
    tap(10);
    REQUIRE_COMMANDS(Command{20});

    harness_reset();
    tap(2);
    harness_press(5);
    harness_advance_to(harness_now_ms() + HOLD_MS);
    REQUIRE_COMMANDS(Command{31});

    harness_reset();
    tap(2);
    harness_press(2);
    harness_advance_to(harness_now_ms() + HOLD_MS);
    REQUIRE_COMMANDS(Command{28});

    return 0;
}
