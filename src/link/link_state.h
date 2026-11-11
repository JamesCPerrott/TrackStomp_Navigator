#ifndef LINK_STATE_H
#define LINK_STATE_H

#include <cstdint>

// Pure BLE session policy (PRD §7.5, §8.5, §12.1). No BTstack, no GPIO, no flash.

enum class LinkEventKind : uint8_t {
    Ready,            // encrypted with the bonded key and subscribed (§9.3.1)
    Lost,             // disconnected, or no longer ready
    PairingStarted,   // a central began pairing inside the window
    PairingSucceeded, // bond stored
    PairingFailed,
    NoRadio // the transport has no radio
};

struct LinkEvent {
    LinkEventKind kind;
};

enum class LinkRequestKind : uint8_t {
    BeginPairing, // clear bond, advertise for pairing (§7.5 steps 1–2)
    EndPairing    // stop pairing advertising
};

struct LinkRequest {
    LinkRequestKind kind;
};

// BondOk is the §11.5.8 long flash. It outlives the window: the window is
// already closed while it plays (§7.5).
enum class LinkPairingPhase : uint8_t { Closed, Advertising, Bonding, BondOk };

void link_tick(uint32_t now);

// loop_dispatch is the only caller of these three (§12.2). The sequencer and
// ui never call them.
void link_on_request(LinkRequest request); // from the sequencer's link-request drain
void link_on_event(LinkEvent event);       // from the transport
bool link_poll_request(LinkRequest* out);  // to the transport

// Read-only. The sequencer, ui, and midi_out poll these (QUESTIONS Q032).
bool link_pairing_window_open();
LinkPairingPhase link_pairing_phase();
bool link_ready();
// Latched when the host first becomes ready within BOOT_CONNECT_WINDOW_MS of
// power-up (§11.5.8). Never cleared; ui tracks whether the flash played.
bool link_boot_connected();

#ifdef HOST_TEST
void link_reset(); // power-up state: not ready, window closed, no requests queued
#endif

#endif
