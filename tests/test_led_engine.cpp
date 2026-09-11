#include "config.h"
#include "harness.h"
#include "ui/ui.h"

#include <chrono>

// Priority stack: idle off, setup solid on. Bounded work per tick (criterion 60).
int main() {
    harness_reset();
    REQUIRE_LAMP(false);

    const auto started = std::chrono::steady_clock::now();
    ui_tick(harness_now_ms());
    const auto elapsed = std::chrono::steady_clock::now() - started;
    REQUIRE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() < 10);

    harness_press(6);
    harness_press(9);
    harness_advance_to(CHORD_HOLD_MS - 1U);
    REQUIRE_LAMP(false);
    REQUIRE(harness_lamp_trace().back() == 0U);
    harness_advance_to(CHORD_HOLD_MS);
    REQUIRE_LAMP(true);
    REQUIRE(harness_lamp_trace().back() == 1U);

    harness_advance(200U);
    REQUIRE_LAMP(true);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_LAMP(true);

    harness_press(6);
    harness_press(9);
    harness_advance(DEBOUNCE_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_LAMP(false);

    return 0;
}
