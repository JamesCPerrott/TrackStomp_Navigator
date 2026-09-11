#include "config.h"
#include "harness.h"
#include "midi/midi_out.h"

int main() {
    midi_out_reset();
    REQUIRE(midi_out_channel() == DEFAULT_MIDI_CHANNEL);
    midi_out_send(20);
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

    midi_out_set_channel(8);
    REQUIRE(midi_out_channel() == 8U);
    midi_out_send(5);
    midi_out_send(5);
    REQUIRE(midi_out_poll_byte(&b0));
    REQUIRE(b0 == 0x97U);
    REQUIRE(midi_out_poll_byte(&b1));
    REQUIRE(b1 == 5U);
    REQUIRE(midi_out_poll_byte(&b2));
    REQUIRE(b2 == MIDI_VELOCITY);
    REQUIRE(midi_out_poll_byte(&b0));
    REQUIRE(b0 == 0x97U);
    REQUIRE(midi_out_poll_byte(&b1));
    REQUIRE(b1 == 5U);
    REQUIRE(midi_out_poll_byte(&b2));
    REQUIRE(b2 == MIDI_VELOCITY);
    REQUIRE(!midi_out_poll_byte(&b0));

    midi_out_set_channel(0);
    REQUIRE(midi_out_channel() == 8U);
    midi_out_set_channel(11);
    REQUIRE(midi_out_channel() == 8U);

    return 0;
}
