#include "config.h"
#include "harness.h"
#include "midi/midi_out.h"
#include "storage/config_store.h"

int main() {
    config_store_host_erase();
    midi_out_reset();
    harness_reset();

    harness_press(10);
    harness_advance(DEBOUNCE_MS);
    harness_release(10);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_COMMANDS(Command{20});
    uint8_t b0 = 0;
    uint8_t b1 = 0;
    uint8_t b2 = 0;
    REQUIRE(midi_out_poll_byte(&b0));
    REQUIRE(midi_out_poll_byte(&b1));
    REQUIRE(midi_out_poll_byte(&b2));
    REQUIRE(b0 == 0x90U);
    REQUIRE(b1 == 20U);
    REQUIRE(b2 == MIDI_VELOCITY);
    REQUIRE(!midi_out_poll_byte(&b0));
    REQUIRE(config_store_host_write_count() == 0U);

    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(CHORD_HOLD_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    harness_press(8);
    harness_advance(DEBOUNCE_MS);
    harness_release(8);
    harness_advance(DEBOUNCE_MS);
    harness_press(6);
    harness_press(9);
    harness_advance(DEBOUNCE_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    REQUIRE(config_store_read_channel() == 8U);
    REQUIRE(config_store_host_write_count() == 1U);
    REQUIRE(midi_out_channel() == 8U);

    harness_clear_captures();
    while (midi_out_poll_byte(&b0)) {
    }
    harness_press(10);
    harness_advance(DEBOUNCE_MS);
    harness_release(10);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_COMMANDS(Command{20});
    REQUIRE(midi_out_poll_byte(&b0));
    REQUIRE(b0 == 0x97U);
    REQUIRE(midi_out_poll_byte(&b1));
    REQUIRE(b1 == 20U);
    REQUIRE(midi_out_poll_byte(&b2));
    REQUIRE(b2 == MIDI_VELOCITY);

    harness_press(6);
    harness_press(9);
    harness_advance_to(harness_now_ms() + CHORD_HOLD_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    harness_press(6);
    harness_press(9);
    harness_advance(DEBOUNCE_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    REQUIRE(config_store_host_write_count() == 1U);
    REQUIRE(midi_out_channel() == 8U);

    return 0;
}
