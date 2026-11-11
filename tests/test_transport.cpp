#include "config.h"
#include "harness.h"
#include "link/link_state.h"
#include "midi/ble_midi_packet.h"
#include "midi/midi_out.h"
#include "midi/transport.h"
#include "sequencer/sequencer.h"

#include <cstddef>
#include <cstdint>

namespace {

void tap(uint8_t button) {
    harness_press(button);
    harness_advance(DEBOUNCE_MS);
    harness_release(button);
    harness_advance(DEBOUNCE_MS);
}

void require_no_usb_bytes() {
    uint8_t byte = 0;
    REQUIRE(!midi_out_poll_byte(&byte));
}

void require_usb_note(uint8_t note) {
    uint8_t b0 = 0;
    uint8_t b1 = 0;
    uint8_t b2 = 0;
    REQUIRE(midi_out_poll_byte(&b0));
    REQUIRE(midi_out_poll_byte(&b1));
    REQUIRE(midi_out_poll_byte(&b2));
    const uint8_t status = static_cast<uint8_t>(0x90U | (midi_out_channel() - 1U));
    REQUIRE(b0 == status);
    REQUIRE(b1 == note);
    REQUIRE(b2 == MIDI_VELOCITY);
    REQUIRE(!midi_out_poll_byte(&b0));
    REQUIRE(harness_ble_packets().empty());
}

void require_ble_note(uint8_t note, uint32_t now_ms) {
    require_no_usb_bytes();
    const BleMidiPacket expected = ble_midi_encode_note_on(note, midi_out_channel(), now_ms);
    REQUIRE(harness_ble_packets().size() == 1U);
    const auto& actual = harness_ble_packets().back();
    REQUIRE(actual.size() == static_cast<std::size_t>(BLE_MIDI_PACKET_LENGTH));
    for (std::size_t i = 0; i < actual.size(); ++i) {
        REQUIRE(actual[i] == expected.bytes[i]);
    }
}

} // namespace

// Criterion 97 first: USB wins, BLE is the fallback, neither drops the cue,
// and toggling USB moves the next cue. A cue is never on both wires.
int main() {
    REQUIRE(transport_select(true, true) == Transport::Usb);
    REQUIRE(transport_select(true, false) == Transport::Usb);
    REQUIRE(transport_select(false, true) == Transport::Ble);
    REQUIRE(transport_select(false, false) == Transport::None);

    // USB mounted, BLE not ready. Bracket the release edge. USB bytes are the
    // T16 Note On: status, note, MIDI_VELOCITY.
    harness_reset();
    REQUIRE(!link_ready());
    harness_press(10);
    harness_advance(DEBOUNCE_MS);
    harness_release(10);
    harness_advance(DEBOUNCE_MS - 1U);
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    harness_advance(1);
    REQUIRE_COMMANDS(Command{20});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Usb, 20});
    require_usb_note(20);
    REQUIRE(harness_now_ms() == (DEBOUNCE_MS + DEBOUNCE_MS));

    // USB mounted and BLE ready: still USB only.
    harness_reset();
    harness_link_event(LinkEventKind::Ready);
    REQUIRE(link_ready());
    tap(10);
    REQUIRE_COMMANDS(Command{20});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Usb, 20});
    require_usb_note(20);

    // USB not mounted, BLE ready: the encoder's packet, and nothing on USB.
    // One millisecond early is silent.
    harness_reset();
    harness_set_usb_mounted(false);
    harness_link_event(LinkEventKind::Ready);
    harness_press(10);
    harness_advance(DEBOUNCE_MS);
    harness_release(10);
    harness_advance(DEBOUNCE_MS - 1U);
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    REQUIRE(harness_ble_packets().empty());
    harness_advance(1);
    REQUIRE_COMMANDS(Command{20});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Ble, 20});
    require_ble_note(20, harness_now_ms());

    // Channel 10 changes the status byte only. The policy is unchanged.
    harness_reset();
    midi_out_set_channel(10);
    harness_set_usb_mounted(false);
    harness_link_event(LinkEventKind::Ready);
    tap(10);
    REQUIRE_COMMANDS(Command{20});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Ble, 20});
    require_ble_note(20, harness_now_ms());
    REQUIRE(harness_ble_packets().back()[2] == 0x99U);

    // Neither transport: the sequencer still emits the cue, and it is dropped.
    harness_reset();
    harness_set_usb_mounted(false);
    REQUIRE(!link_ready());
    tap(10);
    REQUIRE_COMMANDS(Command{20});
    REQUIRE_NO_TRANSPORT_SENDS();
    require_no_usb_bytes();
    REQUIRE(harness_ble_packets().empty());

    // Two cues, one after the other. Unplugging USB moves the second to BLE.
    harness_reset();
    harness_link_event(LinkEventKind::Ready);
    tap(2);
    tap(7);
    REQUIRE_COMMANDS(Command{5});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Usb, 5});
    require_usb_note(5);
    harness_clear_captures();
    harness_set_usb_mounted(false);
    tap(10);
    REQUIRE_COMMANDS(Command{20});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Ble, 20});
    require_ble_note(20, harness_now_ms());

    // The other edge: BLE first, then USB is plugged in before the next cue.
    harness_reset();
    harness_set_usb_mounted(false);
    harness_link_event(LinkEventKind::Ready);
    tap(10);
    REQUIRE_COMMANDS(Command{20});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Ble, 20});
    require_ble_note(20, harness_now_ms());
    harness_clear_captures();
    harness_set_usb_mounted(true);
    tap(10);
    REQUIRE_COMMANDS(Command{20});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Usb, 20});
    require_usb_note(20);

    // Repeated cues are not suppressed. Each carries its own timestamp.
    harness_reset();
    harness_set_usb_mounted(false);
    harness_link_event(LinkEventKind::Ready);
    tap(10);
    const uint32_t first_at   = harness_now_ms();
    const BleMidiPacket first = ble_midi_encode_note_on(20, midi_out_channel(), first_at);
    tap(10);
    const uint32_t second_at   = harness_now_ms();
    const BleMidiPacket second = ble_midi_encode_note_on(20, midi_out_channel(), second_at);
    REQUIRE(second_at == first_at + (DEBOUNCE_MS + DEBOUNCE_MS));
    REQUIRE_COMMANDS(Command{20}, Command{20});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Ble, 20}, TransportSend{Transport::Ble, 20});
    REQUIRE(harness_ble_packets().size() == 2U);
    for (std::size_t i = 0; i < BLE_MIDI_PACKET_LENGTH; ++i) {
        REQUIRE(harness_ble_packets()[0][i] == first.bytes[i]);
        REQUIRE(harness_ble_packets()[1][i] == second.bytes[i]);
    }
    REQUIRE(first.bytes[0] != second.bytes[0] || first.bytes[1] != second.bytes[1]);

    // Mid-sequence: the prefix is pending on USB, the cable goes, the suffix
    // cue is the first thing sent and it goes to BLE.
    harness_reset();
    harness_link_event(LinkEventKind::Ready);
    tap(2);
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    harness_set_usb_mounted(false);
    tap(7);
    REQUIRE_COMMANDS(Command{5});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Ble, 5});
    require_ble_note(5, harness_now_ms());

    // The opposite mid-sequence edge: pending on BLE, then USB is plugged in.
    harness_reset();
    harness_set_usb_mounted(false);
    harness_link_event(LinkEventKind::Ready);
    tap(2);
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    harness_set_usb_mounted(true);
    tap(7);
    REQUIRE_COMMANDS(Command{5});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Usb, 5});
    require_usb_note(5);

    // A hold is a cue and uses the same policy. Bracket HOLD_MS.
    harness_reset();
    harness_set_usb_mounted(false);
    harness_link_event(LinkEventKind::Ready);
    harness_press(1);
    harness_advance_to(HOLD_MS - 1U);
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    harness_advance(1);
    REQUIRE(harness_now_ms() == HOLD_MS);
    REQUIRE_COMMANDS(Command{27});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Ble, 27});
    require_ble_note(27, harness_now_ms());

    // LOCKED: after the hold, another press sends nothing on the ready wire.
    harness_clear_captures();
    harness_press(6);
    harness_advance(DEBOUNCE_MS);
    harness_release(6);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    REQUIRE(harness_ble_packets().empty());
    require_no_usb_bytes();

    // SETUP, and the chord lockout that precedes it. Then a cue after exit
    // still leaves, so the silence was the state.
    harness_reset();
    harness_press(6);
    harness_press(9);
    const uint32_t chord_start = harness_now_ms();
    harness_advance_to(chord_start + CHORD_HOLD_MS - 1U);
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    harness_advance(1);
    REQUIRE(harness_now_ms() == chord_start + CHORD_HOLD_MS);
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    harness_press(10);
    harness_advance(DEBOUNCE_MS);
    harness_release(10);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    tap(3);
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    harness_press(6);
    harness_press(9);
    harness_advance(DEBOUNCE_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    tap(10);
    REQUIRE_COMMANDS(Command{20});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Usb, 20});
    require_usb_note(20);

    return 0;
}
