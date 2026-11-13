#include "link/link_state.h"
#include "midi/ble_midi.h"

#include <cstdint>
#include <iostream>

// The no-radio transport the pico2 firmware links (PRD §7.5). This test
// links ble_midi_stub.cpp directly and not the harness, which is the host
// implementation of the same header. pico2_w links ble_midi.cpp instead.

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << " FAIL: " #cond "\n";                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (false)

int main() {
    LinkEvent event{LinkEventKind::Ready};

    // Silent until asked to pair.
    CHECK(!ble_midi_poll_event(&event));
    ble_midi_request(LinkRequest{LinkRequestKind::EndPairing});
    CHECK(!ble_midi_poll_event(&event));

    // A pairing request is answered with exactly one NoRadio.
    ble_midi_request(LinkRequest{LinkRequestKind::BeginPairing});
    CHECK(!ble_midi_poll_event(nullptr));
    CHECK(ble_midi_poll_event(&event));
    CHECK(event.kind == LinkEventKind::NoRadio);
    CHECK(!ble_midi_poll_event(&event));

    // A packet is dropped and reports nothing.
    const uint8_t packet[5] = {0x80, 0x80, 0x90, 20, 1};
    ble_midi_send(packet, 5);
    CHECK(!ble_midi_poll_event(&event));
    return 0;
}
