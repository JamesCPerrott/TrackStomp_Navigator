#include "config.h"
#include "harness.h"
#include "input/buttons.h"
#include "link/link_state.h"
#include "midi/transport.h"
#include "sequencer/sequencer.h"

#include <cstdint>

namespace {

void enter_setup() {
    const uint32_t start = harness_now_ms();
    harness_press(CHORD_BUTTON_A);
    harness_press(CHORD_BUTTON_B);
    harness_advance_to(start + CHORD_HOLD_MS);
    harness_release(CHORD_BUTTON_A);
    harness_release(CHORD_BUTTON_B);
    harness_advance(DEBOUNCE_MS);
    harness_clear_captures();
}

} // namespace

// 86 first: outside setup, 2+4 is button 2's hold and lockout. No pairing.
int main() {
    harness_reset();
    harness_press(2);
    harness_advance_to(100U);
    harness_press(4);
    harness_advance_to(HOLD_MS - 1U);
    REQUIRE_NO_BUTTON_EVENTS();
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_TRANSPORT_SENDS();
    harness_advance(1);
    REQUIRE(harness_now_ms() == HOLD_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{2, ButtonEventKind::Hold});
    REQUIRE_COMMANDS(Command{28});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Usb, 28});
    REQUIRE_NO_LINK_REQUESTS();
    // Past the moment a leaked pair chord would fire, and past button 4's hold.
    harness_advance_to(100U + PAIR_CHORD_HOLD_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{2, ButtonEventKind::Hold});
    REQUIRE_COMMANDS(Command{28});
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Usb, 28});
    REQUIRE_NO_LINK_REQUESTS();
    harness_release(2);
    harness_advance(DEBOUNCE_MS);
    harness_release(4);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{2, ButtonEventKind::Hold});
    REQUIRE_COMMANDS(Command{28});
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Usb, 28});

    // 84. In setup, 2+4 held to the threshold issues one BEGIN_PAIRING. No MIDI.
    harness_reset();
    enter_setup();
    const uint32_t pair_at = harness_now_ms();
    harness_press(PAIR_CHORD_BUTTON_A);
    harness_press(PAIR_CHORD_BUTTON_B);
    harness_advance_to(pair_at + PAIR_CHORD_HOLD_MS - 1U);
    REQUIRE_NO_BUTTON_EVENTS();
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    harness_advance(1);
    REQUIRE(harness_now_ms() == pair_at + PAIR_CHORD_HOLD_MS);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{PAIR_CHORD_BUTTON_A, ButtonEventKind::PairChordHold});
    REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::BeginPairing});
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    harness_advance(1);
    REQUIRE_BUTTON_EVENTS(ButtonEvent{PAIR_CHORD_BUTTON_A, ButtonEventKind::PairChordHold});
    REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::BeginPairing});

    // 85. Release before the threshold: no request, and neither button selects.
    // A later clean tap of 2 still selects channel 2.
    harness_reset();
    enter_setup();
    const uint32_t abort_at = harness_now_ms();
    harness_press(PAIR_CHORD_BUTTON_A);
    harness_press(PAIR_CHORD_BUTTON_B);
    harness_advance_to(abort_at + 1500U);
    harness_release(PAIR_CHORD_BUTTON_B);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_UI_EVENTS();
    REQUIRE_NO_COMMANDS();
    harness_release(PAIR_CHORD_BUTTON_A);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_UI_EVENTS();
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();
    harness_press(PAIR_CHORD_BUTTON_A);
    harness_advance(DEBOUNCE_MS);
    harness_release(PAIR_CHORD_BUTTON_A);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupChannel, PAIR_CHORD_BUTTON_A});
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_COMMANDS();

    // 87. Release clears the timer. Re-forming starts a fresh PAIR_CHORD_HOLD_MS.
    // The old deadline and a paused remainder are both still silent.
    harness_reset();
    enter_setup();
    const uint32_t reform_at = harness_now_ms();
    harness_press(PAIR_CHORD_BUTTON_A);
    harness_press(PAIR_CHORD_BUTTON_B);
    harness_advance_to(reform_at + 1000U);
    harness_release(PAIR_CHORD_BUTTON_B);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_UI_EVENTS();
    harness_advance_to(reform_at + 1500U);
    harness_press(PAIR_CHORD_BUTTON_B);
    harness_advance_to(reform_at + 2000U);
    REQUIRE_NO_LINK_REQUESTS();
    harness_advance_to(reform_at + 1500U + PAIR_CHORD_HOLD_MS - 1U);
    REQUIRE_NO_LINK_REQUESTS();
    REQUIRE_NO_COMMANDS();
    harness_advance(1);
    REQUIRE(harness_now_ms() == reform_at + 1500U + PAIR_CHORD_HOLD_MS);
    REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::BeginPairing});
    REQUIRE_BUTTON_EVENTS(ButtonEvent{PAIR_CHORD_BUTTON_A, ButtonEventKind::PairChordHold});
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();

    // A contact bounce on 4 does not clear the timer. Same shape as the 6+9 test.
    harness_reset();
    enter_setup();
    const uint32_t bounce_at = harness_now_ms();
    harness_press(PAIR_CHORD_BUTTON_A);
    harness_press(PAIR_CHORD_BUTTON_B);
    harness_advance_to(bounce_at + 1000U);
    harness_inject_bounce(PAIR_CHORD_BUTTON_B, 20, 10);
    REQUIRE(harness_gpio_transition_count(PAIR_CHORD_BUTTON_B) >= 20U);
    harness_advance_to(bounce_at + PAIR_CHORD_HOLD_MS - 1U);
    REQUIRE_NO_LINK_REQUESTS();
    harness_advance_to(bounce_at + PAIR_CHORD_HOLD_MS);
    REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::BeginPairing});
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_TRANSPORT_SENDS();

    return 0;
}
