#include "config.h"
#include "harness.h"

namespace {

void tap(uint8_t id) {
    harness_press(id);
    harness_advance(DEBOUNCE_MS);
    harness_release(id);
    harness_advance(DEBOUNCE_MS);
}

} // namespace

// 42 first: chord blackout must override PENDING, not jump to the faster flash.
int main() {
    harness_reset();
    tap(2);
    REQUIRE_LAMP(true);
    harness_press(6);
    harness_press(9);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_LAMP(false);
    harness_advance(CHORD_LED_DELAY_MS - DEBOUNCE_MS - 1U);
    REQUIRE_LAMP(false);

    harness_reset();
    tap(2);
    REQUIRE_LAMP(true);
    harness_press(6);
    harness_press(9);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_LAMP(false);
    harness_advance_to(harness_now_ms() + 300U - DEBOUNCE_MS);
    REQUIRE_LAMP(false);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_LAMP(true);

    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(2000U);
    REQUIRE_LAMP(true);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_LAMP(false);
    harness_advance_to(2500U);
    harness_press(9);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_LAMP(false);
    harness_advance_to(2500U + CHORD_LED_DELAY_MS - 1U);
    REQUIRE_LAMP(false);
    harness_advance_to(2500U + CHORD_LED_DELAY_MS);
    REQUIRE_LAMP(true);

    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(CHORD_LED_DELAY_MS - 1U);
    REQUIRE_LAMP(false);
    harness_advance_to(CHORD_LED_DELAY_MS);
    REQUIRE_LAMP(true);
    harness_advance(CHORD_LED_FLASH_MS - 1U);
    REQUIRE_LAMP(true);
    harness_advance(1);
    REQUIRE_LAMP(false);
    harness_advance(CHORD_LED_FLASH_MS - 1U);
    REQUIRE_LAMP(false);
    harness_advance(1);
    REQUIRE_LAMP(true);
    harness_advance_to(CHORD_HOLD_MS);
    REQUIRE_LAMP(true);
    harness_advance(200U);
    REQUIRE_LAMP(true);

    return 0;
}
