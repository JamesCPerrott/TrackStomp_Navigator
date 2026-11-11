#include "config.h"
#include "harness.h"
#include "input/buttons.h"
#include "link/link_state.h"
#include "sequencer/sequencer.h"
#include "storage/config_store.h"

#include <cstdint>

namespace {

uint32_t enter_setup() {
    const uint32_t start = harness_now_ms();
    harness_press(CHORD_BUTTON_A);
    harness_press(CHORD_BUTTON_B);
    harness_advance_to(start + CHORD_HOLD_MS);
    const uint32_t deadline = harness_now_ms() + SETUP_TIMEOUT_MS;
    harness_release(CHORD_BUTTON_A);
    harness_release(CHORD_BUTTON_B);
    harness_advance(DEBOUNCE_MS);
    harness_clear_captures();
    return deadline;
}

// Presses 2+4 and advances to the pair threshold. Caller checks the request.
uint32_t open_pairing_window() {
    const uint32_t pair_at = harness_now_ms();
    harness_press(PAIR_CHORD_BUTTON_A);
    harness_press(PAIR_CHORD_BUTTON_B);
    harness_advance_to(pair_at + PAIR_CHORD_HOLD_MS);
    return harness_now_ms();
}

void release_pair() {
    harness_release(PAIR_CHORD_BUTTON_A);
    harness_release(PAIR_CHORD_BUTTON_B);
    harness_advance(DEBOUNCE_MS);
}

void tap(uint8_t id) {
    harness_press(id);
    harness_advance(DEBOUNCE_MS);
    harness_release(id);
    harness_advance(DEBOUNCE_MS);
}

void exit_setup_gesture() {
    harness_press(CHORD_BUTTON_A);
    harness_press(CHORD_BUTTON_B);
    harness_advance(DEBOUNCE_MS);
    harness_release(CHORD_BUTTON_A);
    harness_release(CHORD_BUTTON_B);
    harness_advance(DEBOUNCE_MS);
}

} // namespace

// 88 first. Taps select nothing during the window, and 31 s of inactivity
// does not exit. 98's window half: nothing on either transport.
int main() {
    harness_reset();
    const uint32_t setup_deadline = enter_setup();
    const uint32_t window_start   = open_pairing_window();
    REQUIRE(link_pairing_window_open());
    REQUIRE(link_pairing_phase() == LinkPairingPhase::Advertising);
    REQUIRE(!link_boot_connected());
    REQUIRE_BUTTON_EVENTS(ButtonEvent{PAIR_CHORD_BUTTON_A, ButtonEventKind::PairChordHold});
    REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::BeginPairing});
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    REQUIRE_NO_UI_EVENTS();

    release_pair();
    REQUIRE_NO_UI_EVENTS();
    harness_clear_captures();
    for (uint8_t id = 1; id <= BUTTON_COUNT; ++id) {
        tap(id);
    }
    REQUIRE_NO_UI_EVENTS();
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    REQUIRE(link_pairing_window_open());

    // The setup deadline from entry would have fired by now if the window
    // did not suspend it. 31 s is one second past SETUP_TIMEOUT_MS.
    const uint32_t idle_at = harness_now_ms();
    REQUIRE(idle_at + SETUP_TIMEOUT_MS + 1000U < window_start + PAIRING_WINDOW_MS);
    harness_advance_to(setup_deadline - 1U);
    REQUIRE_NO_UI_EVENTS();
    REQUIRE(link_pairing_window_open());
    harness_advance(1);
    REQUIRE(harness_now_ms() == setup_deadline);
    REQUIRE_NO_UI_EVENTS();
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    harness_advance_to(idle_at + SETUP_TIMEOUT_MS + 1000U);
    REQUIRE_NO_UI_EVENTS();
    REQUIRE(link_pairing_window_open());
    REQUIRE(link_pairing_phase() == LinkPairingPhase::Advertising);
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();

    // 90. Bond success closes the window, leaves setup, and exits exactly
    // SETUP_TIMEOUT_MS later. No END_PAIRING: the transport reported success.
    harness_reset();
    enter_setup();
    open_pairing_window();
    release_pair();
    harness_clear_captures();
    const uint32_t writes_before_success = config_store_host_write_count();
    harness_link_event(LinkEventKind::PairingSucceeded);
    REQUIRE(!link_pairing_window_open());
    REQUIRE(link_pairing_phase() == LinkPairingPhase::BondOk);
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_UI_EVENTS();
    REQUIRE_NO_TRANSPORT_SENDS();
    harness_link_event(LinkEventKind::PairingFailed);
    REQUIRE(!link_pairing_window_open());
    REQUIRE(link_pairing_phase() == LinkPairingPhase::BondOk);
    const uint32_t closed_ok = harness_now_ms();
    harness_advance_to(closed_ok + SETUP_TIMEOUT_MS - 1U);
    REQUIRE_NO_UI_EVENTS();
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE(link_pairing_phase() == LinkPairingPhase::BondOk);
    harness_advance(1);
    REQUIRE(harness_now_ms() == closed_ok + SETUP_TIMEOUT_MS);
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupExit, DEFAULT_MIDI_CHANNEL});
    REQUIRE(config_store_host_write_count() == writes_before_success);
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_TRANSPORT_SENDS();

    // 91. Failure returns to advertising and does not move the deadline.
    harness_reset();
    enter_setup();
    const uint32_t fail_open = open_pairing_window();
    release_pair();
    harness_clear_captures();
    harness_advance(5000);
    harness_link_event(LinkEventKind::PairingStarted);
    REQUIRE(link_pairing_window_open());
    REQUIRE(link_pairing_phase() == LinkPairingPhase::Bonding);
    REQUIRE_NO_LINK_REQUESTS();
    harness_advance(5000);
    harness_link_event(LinkEventKind::PairingFailed);
    REQUIRE(link_pairing_window_open());
    REQUIRE(link_pairing_phase() == LinkPairingPhase::Advertising);
    REQUIRE_NO_LINK_REQUESTS();
    harness_advance_to(fail_open + PAIRING_WINDOW_MS - 1U);
    REQUIRE(link_pairing_window_open());
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_UI_EVENTS();
    harness_advance(1);
    REQUIRE(harness_now_ms() == fail_open + PAIRING_WINDOW_MS);
    REQUIRE(!link_pairing_window_open());
    REQUIRE(link_pairing_phase() == LinkPairingPhase::Closed);
    REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::EndPairing});
    REQUIRE_NO_UI_EVENTS();

    // 92. No bond by the deadline: END_PAIRING, setup remains, inactivity
    // restarts from the close.
    harness_reset();
    enter_setup();
    const uint32_t timeout_open = open_pairing_window();
    release_pair();
    harness_clear_captures();
    harness_advance_to(timeout_open + PAIRING_WINDOW_MS - 1U);
    REQUIRE(link_pairing_window_open());
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_UI_EVENTS();
    harness_advance(1);
    REQUIRE(harness_now_ms() == timeout_open + PAIRING_WINDOW_MS);
    REQUIRE(!link_pairing_window_open());
    REQUIRE(link_pairing_phase() == LinkPairingPhase::Closed);
    REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::EndPairing});
    REQUIRE_NO_UI_EVENTS();
    REQUIRE_NO_TRANSPORT_SENDS();
    const uint32_t closed_timeout = harness_now_ms();
    harness_clear_captures();
    harness_advance(1);
    REQUIRE_NO_LINK_REQUESTS();
    harness_advance_to(closed_timeout + SETUP_TIMEOUT_MS - 1U);
    REQUIRE_NO_UI_EVENTS();
    REQUIRE_NO_LINK_REQUESTS();
    harness_advance(1);
    REQUIRE(harness_now_ms() == closed_timeout + SETUP_TIMEOUT_MS);
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupExit, DEFAULT_MIDI_CHANNEL});
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_TRANSPORT_SENDS();

    // 93. 6+9 during the window sends END_PAIRING, then a normal exit.
    // No channel change: the flash is not written. A tap during the window
    // must not become the committed channel.
    harness_reset();
    config_store_host_erase();
    const uint32_t writes_unchanged = config_store_host_write_count();
    enter_setup();
    open_pairing_window();
    release_pair();
    harness_clear_captures();
    tap(7);
    REQUIRE_NO_UI_EVENTS();
    exit_setup_gesture();
    REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::EndPairing});
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupExit, DEFAULT_MIDI_CHANNEL});
    REQUIRE(!link_pairing_window_open());
    REQUIRE(link_pairing_phase() == LinkPairingPhase::Closed);
    REQUIRE(config_store_host_write_count() == writes_unchanged);
    REQUIRE(config_store_read_channel() == DEFAULT_MIDI_CHANNEL);
    REQUIRE_NO_TRANSPORT_SENDS();

    // Channel chosen before the window still commits, and still writes once.
    harness_reset();
    enter_setup();
    tap(3);
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupChannel, 3});
    open_pairing_window();
    release_pair();
    harness_clear_captures();
    exit_setup_gesture();
    REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::EndPairing});
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupExit, 3});
    REQUIRE(!link_pairing_window_open());
    REQUIRE(config_store_host_write_count() == writes_unchanged + 1U);
    REQUIRE(config_store_read_channel() == 3U);
    REQUIRE_NO_TRANSPORT_SENDS();

    // 94. A second 2+4 restarts the deadline. The original close stays open.
    harness_reset();
    enter_setup();
    const uint32_t first_open = open_pairing_window();
    REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::BeginPairing});
    release_pair();
    harness_advance(1000);
    const uint32_t second_press = harness_now_ms();
    harness_press(PAIR_CHORD_BUTTON_A);
    harness_press(PAIR_CHORD_BUTTON_B);
    harness_advance_to(second_press + PAIR_CHORD_HOLD_MS - 1U);
    REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::BeginPairing});
    REQUIRE(link_pairing_window_open());
    harness_advance(1);
    REQUIRE(harness_now_ms() == second_press + PAIR_CHORD_HOLD_MS);
    const uint32_t second_open = harness_now_ms();
    REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::BeginPairing},
                          LinkRequest{LinkRequestKind::BeginPairing});
    REQUIRE(link_pairing_phase() == LinkPairingPhase::Advertising);
    release_pair();
    harness_clear_captures();
    harness_advance_to(first_open + PAIRING_WINDOW_MS - 1U);
    REQUIRE(link_pairing_window_open());
    REQUIRE_NO_LINK_REQUESTS();
    harness_advance_to(first_open + PAIRING_WINDOW_MS);
    REQUIRE(link_pairing_window_open());
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE(second_open + PAIRING_WINDOW_MS > first_open + PAIRING_WINDOW_MS);
    harness_advance_to(second_open + PAIRING_WINDOW_MS - 1U);
    REQUIRE(link_pairing_window_open());
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_UI_EVENTS();
    harness_advance(1);
    REQUIRE(harness_now_ms() == second_open + PAIRING_WINDOW_MS);
    REQUIRE(!link_pairing_window_open());
    REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::EndPairing});
    REQUIRE_NO_UI_EVENTS();
    REQUIRE_NO_TRANSPORT_SENDS();

    // No radio: the window closes with no END_PAIRING, taps select again,
    // and setup inactivity is not granted another SETUP_TIMEOUT_MS (Q035).
    harness_reset();
    enter_setup();
    // The opening pass still sees 2 and 4 held, and the window opens only
    // after that tick, so inactivity sits at opened + SETUP_TIMEOUT_MS.
    // NoRadio must leave it there, not push it to the close.
    const uint32_t radio_opened = open_pairing_window();
    release_pair();
    harness_clear_captures();
    harness_link_event(LinkEventKind::NoRadio);
    const uint32_t radio_closed = harness_now_ms();
    REQUIRE(!link_pairing_window_open());
    REQUIRE(link_pairing_phase() == LinkPairingPhase::Closed);
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_UI_EVENTS();
    REQUIRE(radio_closed + SETUP_TIMEOUT_MS > radio_opened + SETUP_TIMEOUT_MS);
    harness_link_event(LinkEventKind::PairingStarted);
    REQUIRE(!link_pairing_window_open());
    REQUIRE(link_pairing_phase() == LinkPairingPhase::Closed);
    harness_advance_to(radio_opened + SETUP_TIMEOUT_MS - 1U);
    REQUIRE_NO_UI_EVENTS();
    harness_advance(1);
    REQUIRE(harness_now_ms() == radio_opened + SETUP_TIMEOUT_MS);
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupExit, DEFAULT_MIDI_CHANNEL});
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_TRANSPORT_SENDS();

    harness_reset();
    enter_setup();
    open_pairing_window();
    release_pair();
    harness_clear_captures();
    harness_link_event(LinkEventKind::NoRadio);
    tap(7);
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupChannel, 7});
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    return 0;
}
