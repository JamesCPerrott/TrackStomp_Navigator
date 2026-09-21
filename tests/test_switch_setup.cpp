#include "config.h"
#include "harness.h"
#include "ui/ui.h"

#include <cstdint>

namespace {

constexpr uint16_t kChordLeds = static_cast<uint16_t>(SWITCH_LED(6) | SWITCH_LED(9));

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

} // namespace

// Chord progress, setup blink, and boot. Both edges; two-LED ceiling on every check.
int main() {
    // 72: abort during the pending off-phase resumes that phase, it does not restart.
    harness_reset();
    tap(2);
    const uint32_t pending = harness_now_ms();
    expect_mask(SWITCH_LED(2));
    harness_press(6);
    harness_press(9);
    const uint32_t abort_at = pending + (LED_PENDING_FLASH_MS + 35U);
    REQUIRE(((abort_at - pending) / LED_PENDING_FLASH_MS) % 2U == 1U);
    REQUIRE(abort_at < pending + CHORD_LED_DELAY_MS);
    harness_advance_to(abort_at - DEBOUNCE_MS);
    harness_release(6);
    harness_release(9);
    harness_advance_to(abort_at - 1U);
    expect_mask(0U);
    harness_advance_to(abort_at);
    expect_mask(0U);
    harness_advance_to(pending + (LED_PENDING_FLASH_MS * 2U));
    expect_mask(SWITCH_LED(2));
    expect_trace_ceiling();

    // 72: abort during the pending on-phase lights that LED on the same tick.
    harness_reset();
    tap(2);
    const uint32_t pending_on = harness_now_ms();
    harness_press(6);
    harness_press(9);
    const uint32_t back_on = pending_on + 60U;
    harness_advance_to(back_on - DEBOUNCE_MS);
    harness_release(6);
    harness_release(9);
    harness_advance_to(back_on - 1U);
    expect_mask(0U);
    harness_advance_to(back_on);
    expect_mask(SWITCH_LED(2));
    expect_trace_ceiling();

    // A chord suspends a confirmation and restores it on abort.
    harness_reset();
    tap(1);
    tap(6);
    const uint32_t pair = harness_now_ms();
    expect_mask(SWITCH_LED(1) | SWITCH_LED(6));
    harness_press(6);
    harness_press(9);
    harness_advance(DEBOUNCE_MS);
    expect_mask(0U);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS - 1U);
    expect_mask(0U);
    harness_advance(1U);
    expect_mask(SWITCH_LED(1) | SWITCH_LED(6));
    REQUIRE(harness_now_ms() < pair + LED_CONFIRM_MS);
    expect_trace_ceiling();

    // 71: blackout, including a live pending flash, then 6 and 9 in phase.
    harness_reset();
    tap(2);
    expect_mask(SWITCH_LED(2));
    const uint32_t chord_at = harness_now_ms();
    harness_press(6);
    harness_press(9);
    harness_advance(DEBOUNCE_MS);
    expect_mask(0U);
    harness_advance_to(chord_at + CHORD_LED_DELAY_MS - 1U);
    expect_mask(0U);
    harness_advance_to(chord_at + CHORD_LED_DELAY_MS);
    expect_mask(kChordLeds);
    harness_advance(CHORD_LED_FLASH_MS - 1U);
    expect_mask(kChordLeds);
    harness_advance(1U);
    expect_mask(0U);
    harness_advance(CHORD_LED_FLASH_MS - 1U);
    expect_mask(0U);
    harness_advance(1U);
    expect_mask(kChordLeds);
    expect_trace_ceiling();

    // 73: re-forming the chord restarts the 500 ms blackout.
    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(2000U);
    expect_mask(kChordLeds);
    harness_release(9);
    harness_advance(DEBOUNCE_MS - 1U);
    expect_mask(kChordLeds);
    harness_advance(1U);
    expect_mask(0U);
    harness_advance_to(2500U);
    harness_press(9);
    harness_advance(DEBOUNCE_MS);
    expect_mask(0U);
    harness_advance_to(2500U + CHORD_LED_DELAY_MS - 1U);
    expect_mask(0U);
    harness_advance_to(2500U + CHORD_LED_DELAY_MS);
    expect_mask(kChordLeds);
    expect_trace_ceiling();

    // 74: at 5000 ms the stored channel blinks, including while 6 and 9 are held.
    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(CHORD_HOLD_MS - 1U);
    expect_mask(0U);
    harness_advance_to(CHORD_HOLD_MS);
    const uint32_t setup = harness_now_ms();
    expect_mask(SWITCH_LED(DEFAULT_MIDI_CHANNEL));
    harness_advance(LED_SETUP_BLINK_MS - 1U);
    expect_mask(SWITCH_LED(DEFAULT_MIDI_CHANNEL));
    harness_advance(1U);
    expect_mask(0U);
    harness_advance(LED_SETUP_BLINK_MS - 1U);
    expect_mask(0U);
    harness_advance(1U);
    expect_mask(SWITCH_LED(DEFAULT_MIDI_CHANNEL));
    REQUIRE(harness_button_pressed(6));
    REQUIRE(harness_button_pressed(9));
    expect_trace_ceiling();

    // 75: selecting channel 3 restarts the on phase on that tick.
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    expect_mask(SWITCH_LED(DEFAULT_MIDI_CHANNEL));
    harness_press(3);
    harness_advance(DEBOUNCE_MS);
    harness_release(3);
    harness_advance(DEBOUNCE_MS - 1U);
    expect_mask(SWITCH_LED(DEFAULT_MIDI_CHANNEL));
    harness_advance(1U);
    const uint32_t selected = harness_now_ms();
    expect_mask(SWITCH_LED(3));
    const uint32_t old_off = setup + (3U * LED_SETUP_BLINK_MS);
    REQUIRE(old_off > selected);
    REQUIRE(old_off < selected + LED_SETUP_BLINK_MS);
    harness_advance_to(old_off);
    expect_mask(SWITCH_LED(3));
    harness_advance_to(selected + LED_SETUP_BLINK_MS - 1U);
    expect_mask(SWITCH_LED(3));
    harness_advance_to(selected + LED_SETUP_BLINK_MS);
    expect_mask(0U);
    expect_trace_ceiling();

    // 76: chord exit and the 30 s timeout both leave every switch LED off.
    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(CHORD_HOLD_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    expect_mask(SWITCH_LED(DEFAULT_MIDI_CHANNEL));
    harness_press(6);
    harness_press(9);
    harness_advance(DEBOUNCE_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS - 1U);
    expect_mask(SWITCH_LED(DEFAULT_MIDI_CHANNEL));
    harness_advance(1U);
    expect_mask(0U);
    harness_advance(LED_SETUP_BLINK_MS);
    expect_mask(0U);
    expect_trace_ceiling();

    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(CHORD_HOLD_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    const uint32_t released = harness_now_ms();
    const uint32_t exit_at  = (released - 1U) + SETUP_TIMEOUT_MS;
    harness_advance_to(exit_at - 1U);
    expect_mask(SWITCH_LED(DEFAULT_MIDI_CHANNEL));
    harness_advance_to(exit_at);
    expect_mask(0U);
    harness_advance(LED_SETUP_BLINK_MS);
    expect_mask(0U);
    expect_trace_ceiling();

    // 77: boot is the stored channel, solid, then off. A press cancels it.
    harness_reset();
    ui_indicate_channel(DEFAULT_MIDI_CHANNEL);
    harness_advance(1U);
    expect_mask(SWITCH_LED(DEFAULT_MIDI_CHANNEL));
    harness_advance_to(LED_BOOT_CHANNEL_MS - 1U);
    expect_mask(SWITCH_LED(DEFAULT_MIDI_CHANNEL));
    harness_advance_to(LED_BOOT_CHANNEL_MS);
    expect_mask(0U);
    expect_trace_ceiling();

    harness_reset();
    ui_indicate_channel(8);
    harness_advance(1U);
    expect_mask(SWITCH_LED(8));
    harness_advance_to(LED_BOOT_CHANNEL_MS - 1U);
    expect_mask(SWITCH_LED(8));
    harness_advance_to(LED_BOOT_CHANNEL_MS);
    expect_mask(0U);
    expect_trace_ceiling();

    harness_reset();
    ui_indicate_channel(DEFAULT_MIDI_CHANNEL);
    harness_advance(1U);
    expect_mask(SWITCH_LED(1));
    harness_press(6);
    harness_advance(DEBOUNCE_MS);
    harness_release(6);
    harness_advance(DEBOUNCE_MS - 1U);
    expect_mask(SWITCH_LED(1));
    harness_advance(1U);
    expect_mask(0U);
    harness_advance_to(LED_BOOT_CHANNEL_MS);
    expect_mask(0U);
    expect_trace_ceiling();
    return 0;
}
