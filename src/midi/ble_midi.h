#ifndef BLE_MIDI_H
#define BLE_MIDI_H

#include "link/link_state.h"

#include <cstdint>

// The BLE transport boundary. No BTstack type crosses it. One implementation
// is linked per build:
//   host     tests/harness/harness.cpp (the fake)
//   pico2    ble_midi_stub.cpp (no radio)
//   pico2_w  ble_midi.cpp. BeginPairing answers NoRadio until ble_midi_init.

void ble_midi_init();                                   // pico2_w, after cyw43_arch_init succeeds
void ble_midi_request(LinkRequest request);             // link → transport, via loop_dispatch
bool ble_midi_poll_event(LinkEvent* out);               // transport → link, via loop_dispatch
void ble_midi_send(const uint8_t* packet, uint8_t len); // one §9.3.2 packet, from midi_out

#endif
