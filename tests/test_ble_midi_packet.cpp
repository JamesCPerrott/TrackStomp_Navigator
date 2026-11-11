#include "config.h"
#include "harness.h"
#include "midi/ble_midi_packet.h"

#include <cstdint>

namespace {

void require_packet(const BleMidiPacket& packet, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
                    uint8_t b4) {
    REQUIRE(packet.bytes[0] == b0);
    REQUIRE(packet.bytes[1] == b1);
    REQUIRE(packet.bytes[2] == b2);
    REQUIRE(packet.bytes[3] == b3);
    REQUIRE(packet.bytes[4] == b4);
}

void require_timestamp_framing(uint8_t note, uint8_t channel, uint32_t now_ms) {
    const BleMidiPacket packet = ble_midi_encode_note_on(note, channel, now_ms);
    REQUIRE((packet.bytes[0] & 0xC0U) == 0x80U);
    REQUIRE((packet.bytes[1] & 0x80U) == 0x80U);
    const uint16_t recovered = static_cast<uint16_t>(
        (static_cast<uint16_t>(packet.bytes[0] & 0x3FU) << 7U) | (packet.bytes[1] & 0x7FU));
    REQUIRE(recovered == static_cast<uint16_t>(now_ms & BLE_MIDI_TIMESTAMP_MASK));
    REQUIRE(packet.bytes[4] == MIDI_VELOCITY);
}

} // namespace

int main() {
    // Criterion 99. Note 5, channel 3, t = 0x1ABC → B5 BC 92 05 01.
    const BleMidiPacket example = ble_midi_encode_note_on(5, 3, 0x1ABCU);
    require_packet(example, 0xB5, 0xBC, 0x92, 0x05, 0x01);
    REQUIRE(example.bytes[4] == MIDI_VELOCITY);

    // 13-bit wrap. 0x1FFF is the last timestamp before the boundary;
    // 0x2000 wraps to 0 (80 80 …); 0x2001 is the same framing as t = 1.
    const BleMidiPacket before_wrap = ble_midi_encode_note_on(5, 3, 0x1FFFU);
    require_packet(before_wrap, 0xBF, 0xFF, 0x92, 0x05, 0x01);
    const BleMidiPacket wrapped = ble_midi_encode_note_on(5, 3, 0x2000U);
    require_packet(wrapped, 0x80, 0x80, 0x92, 0x05, 0x01);
    const BleMidiPacket after_wrap = ble_midi_encode_note_on(5, 3, 0x2001U);
    require_packet(after_wrap, 0x80, 0x81, 0x92, 0x05, 0x01);

    // Channels 1 and 10 keep the same timestamp bytes and change only status.
    const BleMidiPacket channel_1 = ble_midi_encode_note_on(5, 1, 0x1ABCU);
    require_packet(channel_1, 0xB5, 0xBC, 0x90, 0x05, 0x01);
    const BleMidiPacket channel_10 = ble_midi_encode_note_on(5, 10, 0x1ABCU);
    require_packet(channel_10, 0xB5, 0xBC, 0x99, 0x05, 0x01);

    // Every note 0–32, which is every CUE_TABLE note.
    for (int note = 0; note <= 32; ++note) {
        const auto value           = static_cast<uint8_t>(note);
        const BleMidiPacket packet = ble_midi_encode_note_on(value, 1, 0);
        REQUIRE(packet.bytes[2] == 0x90U);
        REQUIRE(packet.bytes[3] == value);
        REQUIRE(packet.bytes[4] == MIDI_VELOCITY);
        REQUIRE((packet.bytes[0] & 0x80U) != 0U);
        REQUIRE((packet.bytes[1] & 0x80U) != 0U);
    }
    for (const Cue& cue : CUE_TABLE) {
        const BleMidiPacket packet = ble_midi_encode_note_on(cue.note, 10, 0x2000U);
        REQUIRE(cue.note <= 32U);
        REQUIRE(packet.bytes[2] == 0x99U);
        REQUIRE(packet.bytes[3] == cue.note);
        REQUIRE(packet.bytes[0] == 0x80U);
        REQUIRE(packet.bytes[1] == 0x80U);
        REQUIRE(packet.bytes[4] == MIDI_VELOCITY);
    }

    // Header and timestamp bit 7 across a sweep, including past the wrap.
    for (uint32_t now_ms = 0; now_ms <= 0x2000U; ++now_ms) {
        require_timestamp_framing(0, 1, now_ms);
    }
    constexpr uint32_t kFarTimestamps[] = {0x2001U,     0x3FFFU,     0x4000U,
                                           0x1FFF0000U, 0x80000000U, 0xFFFFFFFFU};
    for (const uint32_t now_ms : kFarTimestamps) {
        require_timestamp_framing(32, 10, now_ms);
    }

    // Note Off is a separate packet. Same timestamp, status 0x80 | (channel - 1),
    // velocity 0. SEND_NOTE_OFF stays false; this only locks the encoder.
    const BleMidiPacket note_off = ble_midi_encode_note_off(5, 3, 0x1ABCU);
    require_packet(note_off, 0xB5, 0xBC, 0x82, 0x05, 0x00);
    REQUIRE(!SEND_NOTE_OFF);
    const BleMidiPacket note_off_wrap = ble_midi_encode_note_off(0, 1, 0x2000U);
    require_packet(note_off_wrap, 0x80, 0x80, 0x80, 0x00, 0x00);
    const BleMidiPacket note_off_ch10 = ble_midi_encode_note_off(32, 10, 0x1FFFU);
    require_packet(note_off_ch10, 0xBF, 0xFF, 0x89, 0x20, 0x00);

    return 0;
}
