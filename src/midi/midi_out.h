#ifndef MIDI_OUT_H
#define MIDI_OUT_H

#include <cstdint>

void midi_out_set_channel(uint8_t channel);
uint8_t midi_out_channel();

// now_ms is milliseconds since boot when the cue was resolved (§9.3.2).
// The USB message does not carry it. The BLE packet does.
void midi_out_send(uint8_t note, uint32_t now_ms);

#ifdef HOST_TEST
void midi_out_reset(); // also sets USB mounted
bool midi_out_poll_byte(uint8_t* out);

// What tud_mounted() reports to the §9.4 policy on the host.
void midi_out_set_usb_mounted(bool mounted);
// The harness's own copy of every byte written to the USB wire, so draining
// it leaves midi_out_poll_byte intact. Tests do not call these two.
bool midi_out_poll_byte_for_harness(uint8_t* out);
bool midi_out_harness_bytes_lost(); // the copy overflowed and dropped bytes
#endif

#endif
