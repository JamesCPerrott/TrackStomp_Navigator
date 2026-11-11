#ifndef BLE_MIDI_PACKET_H
#define BLE_MIDI_PACKET_H

#include <cstdint>

// Pure §9.3.2 encoder. No BTstack types. Host-tested (criterion 99).

constexpr uint8_t BLE_MIDI_PACKET_LENGTH = 5;

struct BleMidiPacket {
    uint8_t bytes[BLE_MIDI_PACKET_LENGTH];
};

// channel is 1-based, matching midi_out. now_ms is milliseconds since boot;
// only the low 13 bits are written (BLE_MIDI_TIMESTAMP_MASK).
BleMidiPacket ble_midi_encode_note_on(uint8_t note, uint8_t channel, uint32_t now_ms);

// Same layout with Note Off status. midi_out calls this only when SEND_NOTE_OFF
// is true. Velocity is 0, matching the USB Note Off (QUESTIONS Q034).
BleMidiPacket ble_midi_encode_note_off(uint8_t note, uint8_t channel, uint32_t now_ms);

#endif
