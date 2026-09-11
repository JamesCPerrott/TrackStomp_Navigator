#include "config.h"
#include "harness.h"
#include "sequencer/sequencer.h"
#include "ui/ui.h"

namespace {

void tap(uint8_t id) {
    harness_press(id);
    harness_advance(DEBOUNCE_MS);
    harness_release(id);
    harness_advance(DEBOUNCE_MS);
}

void enter_setup() {
    harness_reset();
    harness_press(6);
    harness_press(9);
    harness_advance_to(CHORD_HOLD_MS);
    REQUIRE_LAMP(true);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_LAMP(true);
}

void exit_via_chord() {
    harness_press(6);
    harness_press(9);
    harness_advance(DEBOUNCE_MS);
    harness_release(6);
    harness_release(9);
    harness_advance(DEBOUNCE_MS);
}

} // namespace

// 51 first: a new blink cancels one in progress (4 then 7 shows only 7).
int main() {
    enter_setup();
    tap(7);
    REQUIRE_LAMP(false);
    harness_advance(CHANNEL_BLINK_GAP_MS);
    REQUIRE_LAMP(true);
    for (uint8_t pulse = 0; pulse < 7U; ++pulse) {
        REQUIRE_LAMP(true);
        harness_advance(CHANNEL_BLINK_ON_MS);
        REQUIRE_LAMP(false);
        if (pulse + 1U < 7U) {
            harness_advance(CHANNEL_BLINK_OFF_MS);
        }
    }
    harness_advance(CHANNEL_BLINK_OFF_MS + CHANNEL_BLINK_GAP_MS);
    REQUIRE_LAMP(true);

    enter_setup();
    tap(4);
    tap(7);
    harness_advance(CHANNEL_BLINK_GAP_MS);
    REQUIRE_LAMP(true);
    harness_advance((7U * (CHANNEL_BLINK_ON_MS + CHANNEL_BLINK_OFF_MS)) - 1U);
    REQUIRE_LAMP(false);
    harness_advance(1U + CHANNEL_BLINK_GAP_MS);
    REQUIRE_LAMP(true);

    enter_setup();
    harness_clear_captures();
    exit_via_chord();
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupExit, 1});
    REQUIRE_LAMP(false);
    harness_advance(CHANNEL_BLINK_GAP_MS);
    REQUIRE_LAMP(true);
    harness_advance(CHANNEL_BLINK_ON_MS);
    REQUIRE_LAMP(false);
    harness_advance(CHANNEL_BLINK_OFF_MS + CHANNEL_BLINK_GAP_MS);
    REQUIRE_LAMP(false);

    enter_setup();
    harness_clear_captures();
    const uint32_t idle_from = harness_now_ms();
    harness_advance_to(idle_from + SETUP_TIMEOUT_MS);
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupExit, 1});
    REQUIRE_LAMP(false);
    harness_advance(CHANNEL_BLINK_GAP_MS);
    REQUIRE_LAMP(true);
    harness_advance(CHANNEL_BLINK_ON_MS);
    REQUIRE_LAMP(false);
    harness_advance(CHANNEL_BLINK_OFF_MS + CHANNEL_BLINK_GAP_MS);
    REQUIRE_LAMP(false);

    enter_setup();
    harness_clear_captures();
    exit_via_chord();
    tap(10);
    REQUIRE_COMMANDS(Command{20});
    REQUIRE_LAMP(true);

    enter_setup();
    tap(10);
    const uint32_t t0      = harness_now_ms();
    const uint32_t ch10_ms = CHANNEL_BLINK_GAP_MS +
                             (10U * (CHANNEL_BLINK_ON_MS + CHANNEL_BLINK_OFF_MS)) +
                             CHANNEL_BLINK_GAP_MS;
    REQUIRE(ch10_ms == 3800U);
    harness_clear_captures();
    tap(1);
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::SetupChannel, 1});
    REQUIRE(harness_now_ms() - t0 < ch10_ms);
    enter_setup();
    tap(10);
    harness_advance(ch10_ms);
    REQUIRE_LAMP(true);

    harness_reset();
    ui_indicate_channel(DEFAULT_MIDI_CHANNEL);
    REQUIRE_LAMP(false);
    harness_advance(CHANNEL_BLINK_GAP_MS);
    REQUIRE_LAMP(true);
    harness_advance(CHANNEL_BLINK_ON_MS);
    REQUIRE_LAMP(false);

    return 0;
}
