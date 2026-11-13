#include "ble_midi.h"
#include "link/link_state.h"

#include <cstdint>

// The transport for a build with no radio (PRD §7.5). pico2 links this.
// pico2_w links ble_midi.cpp instead. A pairing request is answered with
// NoRadio so the link layer closes the window at once.

namespace {

bool g_no_radio_owed = false;

} // namespace

void ble_midi_request(LinkRequest request) {
    if (request.kind == LinkRequestKind::BeginPairing) {
        g_no_radio_owed = true;
    }
}

bool ble_midi_poll_event(LinkEvent* out) {
    if (out == nullptr || !g_no_radio_owed) {
        return false;
    }
    g_no_radio_owed = false;
    *out            = LinkEvent{LinkEventKind::NoRadio};
    return true;
}

void ble_midi_send(const uint8_t* packet, uint8_t len) {
    static_cast<void>(packet);
    static_cast<void>(len);
}
