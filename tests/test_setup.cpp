#include "config.h"
#include "harness.h"
#include "input/buttons.h"
#include "sequencer/sequencer.h"

// 28 first: a pending prefix is discarded when setup is entered.
int main() {
    harness_reset();
    harness_press(2);
    harness_advance(DEBOUNCE_MS);
    harness_release(2);
    harness_advance(DEBOUNCE_MS);
    harness_press(6);
    harness_press(9);
    harness_advance_to(harness_now_ms() + CHORD_HOLD_MS);
    REQUIRE_NO_COMMANDS();
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::Pending, 2}, UiEvent{UiEventKind::SetupEnter, 1});

    harness_clear_captures();
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_COMMANDS();

    harness_press(2);
    harness_advance(DEBOUNCE_MS);
    harness_release(2);
    harness_advance(DEBOUNCE_MS);
    harness_press(7);
    harness_advance(DEBOUNCE_MS);
    harness_release(7);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_COMMANDS();
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupChannel, 2}, UiEvent{UiEventKind::SetupChannel, 7});

    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(CHORD_HOLD_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    harness_clear_captures();
    harness_press(3);
    harness_advance_to(harness_now_ms() + 3000U);
    harness_release(3);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_UI_EVENTS();
    harness_press(8);
    harness_advance_to(harness_now_ms() + 3000U);
    harness_release(8);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_UI_EVENTS();

    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(CHORD_HOLD_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    harness_clear_captures();
    harness_press(8);
    harness_advance(DEBOUNCE_MS);
    harness_release(8);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupChannel, 8});
    harness_press(6);
    harness_press(9);
    harness_advance(DEBOUNCE_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_COMMANDS();
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupChannel, 8}, UiEvent{UiEventKind::SetupExit, 8});

    harness_clear_captures();
    harness_press(10);
    harness_advance(DEBOUNCE_MS);
    harness_release(10);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_COMMANDS(Command{20});

    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(CHORD_HOLD_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    harness_clear_captures();
    harness_press(6);
    harness_advance(DEBOUNCE_MS);
    harness_release(6);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupChannel, 6});
    harness_press(6);
    harness_press(9);
    harness_advance(DEBOUNCE_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupChannel, 6}, UiEvent{UiEventKind::SetupExit, 6});

    harness_press(6);
    harness_press(9);
    harness_advance_to(harness_now_ms() + CHORD_HOLD_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    harness_clear_captures();
    harness_press(9);
    harness_advance(DEBOUNCE_MS);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupChannel, 9});

    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(CHORD_HOLD_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    harness_clear_captures();
    harness_press(6);
    harness_press(9);
    harness_advance(DEBOUNCE_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_COMMANDS();
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupExit, 1});

    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(CHORD_HOLD_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    harness_clear_captures();
    harness_press(4);
    harness_advance(DEBOUNCE_MS);
    harness_release(4);
    harness_advance(DEBOUNCE_MS);
    const uint32_t idle_from = harness_now_ms();
    harness_advance_to(idle_from + SETUP_TIMEOUT_MS - 1U);
    REQUIRE_NO_COMMANDS();
    harness_advance_to(idle_from + SETUP_TIMEOUT_MS);
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupChannel, 4}, UiEvent{UiEventKind::SetupExit, 4});

    return 0;
}
