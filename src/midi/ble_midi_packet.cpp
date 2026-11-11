#include "ble_midi_packet.h"

#include "config.h"

#include <cstdint>

namespace {

uint8_t timestamp_header(uint32_t now_ms) {
    const uint16_t timestamp = static_cast<uint16_t>(now_ms & BLE_MIDI_TIMESTAMP_MASK);
    // Bits 5..0 are timestamp bits 12..7. Bit 7 is set and bit 6 stays clear.
    const uint8_t high = static_cast<uint8_t>(timestamp >> 7U);
    return static_cast<uint8_t>(0x80U | high);
}

uint8_t timestamp_low(uint32_t now_ms) {
    const uint16_t timestamp = static_cast<uint16_t>(now_ms & BLE_MIDI_TIMESTAMP_MASK);
    const uint8_t low        = static_cast<uint8_t>(timestamp & 0x7FU);
    return static_cast<uint8_t>(0x80U | low);
}

BleMidiPacket encode(uint8_t status_base, uint8_t note, uint8_t channel, uint32_t now_ms,
                     uint8_t velocity) {
    BleMidiPacket packet{};
    packet.bytes[0] = timestamp_header(now_ms);
    packet.bytes[1] = timestamp_low(now_ms);
    packet.bytes[2] = static_cast<uint8_t>(status_base | static_cast<uint8_t>(channel - 1U));
    packet.bytes[3] = note;
    packet.bytes[4] = velocity;
    return packet;
}

} // namespace

BleMidiPacket ble_midi_encode_note_on(uint8_t note, uint8_t channel, uint32_t now_ms) {
    return encode(0x90U, note, channel, now_ms, MIDI_VELOCITY);
}

BleMidiPacket ble_midi_encode_note_off(uint8_t note, uint8_t channel, uint32_t now_ms) {
    return encode(0x80U, note, channel, now_ms, 0U);
}
