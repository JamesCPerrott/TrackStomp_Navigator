#ifndef MIDI_OUT_H
#define MIDI_OUT_H

#include <cstdint>

void midi_out_set_channel(uint8_t channel);
uint8_t midi_out_channel();
void midi_out_send(uint8_t note);

#ifdef HOST_TEST
void midi_out_reset();
bool midi_out_poll_byte(uint8_t* out);
#endif

#endif
