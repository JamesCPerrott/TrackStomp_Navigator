#include "config.h"
#include "harness.h"

#include <cstdint>

namespace {

void expect_mask(uint16_t mask) {
    REQUIRE_SWITCH_LEDS(mask);
    REQUIRE(harness_switch_led_count() <= 2U);
}

void expect_trace_ceiling() {
    for (const uint16_t sample : harness_switch_trace()) {
        uint8_t count = 0;
        for (uint8_t bit = 0; bit < SWITCH_LED_COUNT; ++bit) {
            const uint16_t flag = static_cast<uint16_t>(uint16_t{1} << bit);
            if ((sample & flag) != 0U) {
                count = static_cast<uint8_t>(count + 1U);
            }
        }
        REQUIRE(count <= 2U);
    }
}

void tap(uint8_t id) {
    harness_press(id);
    harness_advance(DEBOUNCE_MS);
    harness_release(id);
    harness_advance(DEBOUNCE_MS);
}

void expect_only_these_bits(uint16_t allowed) {
    for (const uint16_t sample : harness_switch_trace()) {
        const uint16_t extra = static_cast<uint16_t>(sample & static_cast<uint16_t>(~allowed));
        REQUIRE(extra == 0U);
    }
}

} // namespace

// Per-switch indication engine. Both edges of every transition; ceiling on every check.
int main() {
    // 69: the hold envelope never truncates while the button stays down.
    harness_reset();
    harness_press(1);
    harness_advance_to(HOLD_MS - 1U);
    expect_mask(0U);
    harness_advance_to(HOLD_MS);
    expect_mask(SWITCH_LED(1));
    harness_advance_to(HOLD_MS + LED_CONFIRM_MS - 1U);
    expect_mask(SWITCH_LED(1));
    harness_advance_to(HOLD_MS + LED_CONFIRM_MS);
    expect_mask(SWITCH_LED(1));
    harness_advance_to(9000U);
    expect_mask(SWITCH_LED(1));
    harness_release(1);
    harness_advance(DEBOUNCE_MS - 1U);
    expect_mask(SWITCH_LED(1));
    harness_advance(1U);
    expect_mask(0U);
    expect_trace_ceiling();

    // 70: an accepted press cancels a confirmation; a lockout-discarded press does not.
    harness_reset();
    tap(1);
    tap(6);
    expect_mask(SWITCH_LED(1) | SWITCH_LED(6));
    harness_press(2);
    harness_advance(DEBOUNCE_MS);
    harness_release(2);
    harness_advance(DEBOUNCE_MS - 1U);
    expect_mask(SWITCH_LED(1) | SWITCH_LED(6));
    harness_advance(1U);
    expect_mask(SWITCH_LED(2));

    harness_reset();
    tap(1);
    tap(6);
    expect_mask(SWITCH_LED(1) | SWITCH_LED(6));
    tap(8);
    expect_mask(0U);
    expect_trace_ceiling();

    harness_reset();
    harness_press(1);
    harness_advance_to(HOLD_MS);
    expect_mask(SWITCH_LED(1));
    harness_press(6);
    harness_advance(DEBOUNCE_MS);
    expect_mask(SWITCH_LED(1));
    harness_release(6);
    harness_advance(DEBOUNCE_MS);
    expect_mask(SWITCH_LED(1));
    expect_trace_ceiling();

    // 68: releasing inside the envelope still leaves the full 5000 ms.
    harness_reset();
    harness_press(1);
    harness_advance_to(HOLD_MS - 1U);
    expect_mask(0U);
    harness_advance_to(HOLD_MS);
    expect_mask(SWITCH_LED(1));
    harness_advance_to(2100U);
    harness_release(1);
    harness_advance_to(2100U + DEBOUNCE_MS);
    expect_mask(SWITCH_LED(1));
    harness_advance_to(HOLD_MS + LED_CONFIRM_MS - 1U);
    expect_mask(SWITCH_LED(1));
    harness_advance_to(HOLD_MS + LED_CONFIRM_MS);
    expect_mask(0U);
    expect_trace_ceiling();

    // 61, 62: pending flashes at 250 ms and drops at the sequence timeout.
    harness_reset();
    tap(1);
    const uint32_t pending = harness_now_ms();
    expect_mask(SWITCH_LED(1));
    harness_advance_to(pending + LED_PENDING_FLASH_MS - 1U);
    expect_mask(SWITCH_LED(1));
    harness_advance_to(pending + LED_PENDING_FLASH_MS);
    expect_mask(0U);
    harness_advance_to(pending + (LED_PENDING_FLASH_MS * 2U) - 1U);
    expect_mask(0U);
    harness_advance_to(pending + (LED_PENDING_FLASH_MS * 2U));
    expect_mask(SWITCH_LED(1));
    harness_advance_to(pending + SEQUENCE_TIMEOUT_MS - (LED_PENDING_FLASH_MS * 2U));
    expect_mask(SWITCH_LED(1));
    harness_advance_to(pending + SEQUENCE_TIMEOUT_MS - LED_PENDING_FLASH_MS);
    expect_mask(0U);
    harness_advance_to(pending + SEQUENCE_TIMEOUT_MS);
    expect_mask(0U);
    harness_advance(LED_PENDING_FLASH_MS * 2U);
    expect_mask(0U);
    expect_only_these_bits(SWITCH_LED(1));
    expect_trace_ceiling();

    // 63: a new prefix replaces the flash on that tick and restarts the timeout.
    harness_reset();
    tap(2);
    expect_mask(SWITCH_LED(2));
    harness_press(4);
    harness_advance(DEBOUNCE_MS);
    harness_release(4);
    harness_advance(DEBOUNCE_MS - 1U);
    expect_mask(SWITCH_LED(2));
    harness_advance(1U);
    expect_mask(SWITCH_LED(4));
    expect_only_these_bits(SWITCH_LED(2) | SWITCH_LED(4));
    expect_trace_ceiling();

    harness_reset();
    tap(2);
    const uint32_t first = harness_now_ms();
    harness_advance(150U);
    tap(4);
    const uint32_t second         = harness_now_ms();
    const uint32_t elapsed_at_old = (first + SEQUENCE_TIMEOUT_MS) - second;
    const uint32_t phase_at_old   = elapsed_at_old / LED_PENDING_FLASH_MS;
    REQUIRE(elapsed_at_old < SEQUENCE_TIMEOUT_MS);
    REQUIRE((phase_at_old % 2U) == 0U);
    expect_mask(SWITCH_LED(4));
    harness_advance_to(first + SEQUENCE_TIMEOUT_MS);
    expect_mask(SWITCH_LED(4));
    harness_advance_to(second + SEQUENCE_TIMEOUT_MS - LED_PENDING_FLASH_MS);
    expect_mask(0U);
    harness_advance_to(second + SEQUENCE_TIMEOUT_MS);
    expect_mask(0U);
    expect_only_these_bits(SWITCH_LED(2) | SWITCH_LED(4));
    expect_trace_ceiling();

    // 64: a pair is both LEDs solid for the confirmation envelope.
    harness_reset();
    tap(1);
    harness_press(6);
    harness_advance(DEBOUNCE_MS);
    harness_release(6);
    harness_advance(DEBOUNCE_MS - 1U);
    expect_mask(SWITCH_LED(1));
    harness_advance(1U);
    const uint32_t pair = harness_now_ms();
    expect_mask(SWITCH_LED(1) | SWITCH_LED(6));
    harness_advance_to(pair + LED_CONFIRM_MS - 1U);
    expect_mask(SWITCH_LED(1) | SWITCH_LED(6));
    harness_advance_to(pair + LED_CONFIRM_MS);
    expect_mask(0U);
    expect_only_these_bits(SWITCH_LED(1) | SWITCH_LED(6));
    expect_trace_ceiling();

    // 65: a self-pair is one LED with a notch, not a solid pair.
    harness_reset();
    tap(3);
    harness_press(3);
    harness_advance(DEBOUNCE_MS);
    harness_release(3);
    harness_advance(DEBOUNCE_MS - 1U);
    expect_mask(SWITCH_LED(3));
    harness_advance(1U);
    const uint32_t self_pair = harness_now_ms();
    expect_mask(SWITCH_LED(3));
    harness_advance_to(self_pair + LED_SELF_PAIR_ON_MS - 1U);
    expect_mask(SWITCH_LED(3));
    harness_advance_to(self_pair + LED_SELF_PAIR_ON_MS);
    expect_mask(0U);
    harness_advance_to(self_pair + LED_SELF_PAIR_ON_MS + LED_SELF_PAIR_OFF_MS - 1U);
    expect_mask(0U);
    harness_advance_to(self_pair + LED_SELF_PAIR_ON_MS + LED_SELF_PAIR_OFF_MS);
    expect_mask(SWITCH_LED(3));
    harness_advance_to(self_pair + LED_CONFIRM_MS - 1U);
    expect_mask(SWITCH_LED(3));
    harness_advance_to(self_pair + LED_CONFIRM_MS);
    expect_mask(0U);
    expect_only_these_bits(SWITCH_LED(3));
    expect_trace_ceiling();

    // 66: standalone alternates five 1000 ms phases and ends lit, then off.
    harness_reset();
    harness_press(10);
    harness_advance(DEBOUNCE_MS);
    harness_release(10);
    harness_advance(DEBOUNCE_MS - 1U);
    expect_mask(0U);
    harness_advance(1U);
    const uint32_t alone = harness_now_ms();
    for (uint32_t phase = 0; phase < 5U; ++phase) {
        const uint32_t edge = alone + (phase * LED_STANDALONE_MS);
        harness_advance_to(edge);
        if ((phase % 2U) == 0U) {
            expect_mask(SWITCH_LED(10));
        } else {
            expect_mask(0U);
        }
        harness_advance_to(edge + LED_STANDALONE_MS - 1U);
        if ((phase % 2U) == 0U) {
            expect_mask(SWITCH_LED(10));
        } else {
            expect_mask(0U);
        }
    }
    harness_advance_to(alone + LED_CONFIRM_MS);
    expect_mask(0U);
    harness_advance(LED_PENDING_FLASH_MS);
    expect_mask(0U);
    expect_only_these_bits(SWITCH_LED(10));
    expect_trace_ceiling();

    // 67: abandoning a pending prefix for button 10 swaps LEDs on that tick.
    harness_reset();
    tap(2);
    expect_mask(SWITCH_LED(2));
    harness_press(10);
    harness_advance(DEBOUNCE_MS);
    harness_release(10);
    harness_advance(DEBOUNCE_MS - 1U);
    expect_mask(SWITCH_LED(2));
    harness_advance(1U);
    expect_mask(SWITCH_LED(10));
    expect_only_these_bits(SWITCH_LED(2) | SWITCH_LED(10));
    expect_trace_ceiling();

    // 79: holds that send nothing light nothing.
    for (uint8_t id = 6; id <= 9U; ++id) {
        if (id == 8U) {
            continue;
        }
        harness_reset();
        harness_press(id);
        harness_advance_to(HOLD_MS - 1U);
        expect_mask(0U);
        harness_advance_to(HOLD_MS);
        expect_mask(0U);
        harness_advance(1000U);
        expect_mask(0U);
        harness_release(id);
        harness_advance(DEBOUNCE_MS);
        expect_mask(0U);
        expect_trace_ceiling();
    }

    // 80: an orphan suffix from idle lights nothing.
    harness_reset();
    for (uint8_t id = 6; id <= 9U; ++id) {
        tap(id);
        expect_mask(0U);
        harness_advance(LED_PENDING_FLASH_MS);
        expect_mask(0U);
    }
    expect_trace_ceiling();
    return 0;
}
