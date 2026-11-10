#include "config.h"
#include "harness.h"
#include "link/link_state.h"
#include "midi/transport.h"
#include "sequencer/sequencer.h"

#include <cstddef>
#include <cstdint>

// Link and transport exemplar (Phase 10). Imitate the assertions.
// harness_debug_link_request and harness_debug_ble_packet are T32-only
// seeding; do not copy those calls. From T34 midi_out produces the packets,
// and from T35 the pair chord produces the requests.
int main() {
    // Power-up: USB mounted, BLE not ready, window closed, nothing captured.
    harness_reset();
    REQUIRE(!link_ready());
    REQUIRE(!link_pairing_window_open());
    REQUIRE(link_pairing_phase() == LinkPairingPhase::Closed);
    REQUIRE(!link_boot_connected());
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_TRANSPORT_SENDS();
    REQUIRE(harness_ble_packets().empty());

    // One cue. REQUIRE_COMMANDS is what the sequencer emitted;
    // REQUIRE_TRANSPORT_SENDS is which wire it reached. Bracket the edge.
    harness_press(10);
    harness_advance(DEBOUNCE_MS);
    harness_release(10);
    harness_advance(DEBOUNCE_MS - 1U);
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    harness_advance(1);
    REQUIRE_COMMANDS(Command{20});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Usb, 20});
    REQUIRE(harness_ble_packets().empty());
    REQUIRE_NO_LINK_REQUESTS();

    // Sends are an ordered list: 2 then 7 is note 5, then 10 is note 20.
    harness_clear_captures();
    REQUIRE_NO_TRANSPORT_SENDS();
    harness_press(2);
    harness_advance(DEBOUNCE_MS);
    harness_release(2);
    harness_advance(DEBOUNCE_MS);
    harness_press(7);
    harness_advance(DEBOUNCE_MS);
    harness_release(7);
    harness_advance(DEBOUNCE_MS);
    harness_press(10);
    harness_advance(DEBOUNCE_MS);
    harness_release(10);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_COMMANDS(Command{5}, Command{20});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Usb, 5}, TransportSend{Transport::Usb, 20});

    // A transport event is queued in the fake transport and reaches
    // link_state through loop_dispatch, on the pass harness_link_event runs.
    // Ready and Lost are a latch. Neither moves the clock or issues a request.
    harness_clear_captures();
    const uint32_t event_time = harness_now_ms();
    harness_link_event(LinkEventKind::Ready);
    REQUIRE(link_ready());
    harness_advance(1);
    REQUIRE(link_ready());
    harness_link_event(LinkEventKind::Ready);
    REQUIRE(link_ready());
    harness_link_event(LinkEventKind::Lost);
    REQUIRE(!link_ready());
    harness_advance(1);
    REQUIRE(!link_ready());
    harness_link_event(LinkEventKind::Lost);
    REQUIRE(!link_ready());
    harness_link_event(LinkEventKind::Ready);
    REQUIRE(link_ready());
    REQUIRE(harness_now_ms() == event_time + 2U);
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_TRANSPORT_SENDS();

    // USB wins while BLE is ready (PRD §9.4).
    harness_press(10);
    harness_advance(DEBOUNCE_MS);
    harness_release(10);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Usb, 20});
    REQUIRE(harness_ble_packets().empty());

    // harness_reset is a power-up: the link is not ready again.
    harness_reset();
    REQUIRE(!link_ready());
    REQUIRE_NO_TRANSPORT_SENDS();

    // harness_set_usb_mounted and harness_link_event each run one pipeline
    // pass at the current timestamp, like harness_press.
    const uint32_t pass_time  = harness_now_ms();
    const std::size_t samples = harness_lamp_trace().size();
    harness_set_usb_mounted(false);
    harness_set_usb_mounted(true);
    harness_link_event(LinkEventKind::Lost);
    REQUIRE(harness_now_ms() == pass_time);
    REQUIRE(harness_lamp_trace().size() == samples + 3U);
    REQUIRE(harness_switch_trace().size() == harness_lamp_trace().size());

    // T32-only seeding from here on. Do not copy.

    // A BLE packet. The harness reads the note out of the packet: byte 3.
    harness_reset();
    harness_debug_ble_packet({0x80, 0x80, 0x90, 5, MIDI_VELOCITY});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Ble, 5});
    REQUIRE(harness_ble_packets().size() == 1U);
    REQUIRE(harness_ble_packets().back().size() == 5U);
    REQUIRE(harness_ble_packets().back().at(3) == 5U);

    // Both wires share one ordered list, so a cue that reached both would
    // appear twice.
    harness_clear_captures();
    REQUIRE(harness_ble_packets().empty());
    harness_press(10);
    harness_advance(DEBOUNCE_MS);
    harness_release(10);
    harness_advance(DEBOUNCE_MS);
    harness_debug_ble_packet({0x80, 0x80, 0x90, 5, MIDI_VELOCITY});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Usb, 20}, TransportSend{Transport::Ble, 5});

    // A link request goes in at link_on_request and comes out at the fake
    // transport through loop_dispatch. Begin then End, so this still holds
    // once T36 puts the window behind link_on_request.
    harness_clear_captures();
    const uint32_t request_time = harness_now_ms();
    harness_debug_link_request(LinkRequestKind::BeginPairing);
    REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::BeginPairing});
    harness_debug_link_request(LinkRequestKind::EndPairing);
    REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::BeginPairing},
                          LinkRequest{LinkRequestKind::EndPairing});
    REQUIRE(harness_now_ms() == request_time);
    REQUIRE_NO_TRANSPORT_SENDS();

    // harness_clear_captures empties all three buffers and leaves the clock.
    harness_debug_ble_packet({0x80, 0x80, 0x90, 5, MIDI_VELOCITY});
    harness_clear_captures();
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_TRANSPORT_SENDS();
    REQUIRE(harness_ble_packets().empty());
    REQUIRE(harness_now_ms() == request_time);

    // The §9.4 policy is pure and can be called directly. USB wins.
    REQUIRE(transport_select(true, true) == Transport::Usb);
    return 0;
}
