#include "harness.h"

#include <cstdint>

// Per-switch capture exemplar. Imitate the assertions.
// harness_debug_set_switch_leds is T03-only seeding; do not copy that call.
int main() {
    harness_reset();
    harness_debug_set_switch_leds(SWITCH_LED(1));
    REQUIRE_SWITCH_LEDS(SWITCH_LED(1));
    REQUIRE(harness_switch_led_count() == 1U);

    harness_debug_set_switch_leds(SWITCH_LED(1) | SWITCH_LED(6));
    REQUIRE_SWITCH_LEDS(SWITCH_LED(1) | SWITCH_LED(6));
    REQUIRE(harness_switch_led_count() == 2U);

    harness_debug_set_switch_leds(SWITCH_LED(1) | SWITCH_LED(10));
    REQUIRE_SWITCH_LEDS(SWITCH_LED(1) | SWITCH_LED(10));

    harness_debug_set_switch_leds(0U);
    REQUIRE_NO_SWITCH_LEDS();
    REQUIRE(harness_switch_led_count() == 0U);

    harness_reset();
    harness_advance(20);
    harness_debug_set_switch_leds(SWITCH_LED(1));
    REQUIRE(harness_now_ms() == 20U);
    REQUIRE(harness_switch_trace().size() == 21U);
    REQUIRE(harness_switch_trace().at(20) == SWITCH_LED(1));
    REQUIRE(harness_switch_trace().at(19) == 0U);

    harness_reset();
    REQUIRE(harness_switch_trace().size() == 1U);
    REQUIRE(harness_switch_trace().size() == harness_lamp_trace().size());

    harness_advance(20);
    REQUIRE(harness_now_ms() == 20U);
    REQUIRE(harness_switch_trace().size() == 21U);
    REQUIRE(harness_switch_trace().size() == harness_lamp_trace().size());

    harness_press(1);
    REQUIRE(harness_now_ms() == 20U);
    REQUIRE(harness_switch_trace().size() == 22U);
    REQUIRE(harness_switch_trace().size() == harness_lamp_trace().size());

    const uint32_t now = harness_now_ms();
    harness_clear_captures();
    REQUIRE(harness_switch_trace().empty());
    REQUIRE(harness_lamp_trace().empty());
    REQUIRE(harness_now_ms() == now);
    return 0;
}
