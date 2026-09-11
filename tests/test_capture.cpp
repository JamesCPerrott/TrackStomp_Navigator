#include "config.h"
#include "harness.h"
#include "input/buttons.h"
#include "sequencer/sequencer.h"

// Capture-buffer exemplar. Shows ordered-list and empty assertions.
// T08 replaces the bodies with real sequencer output, e.g.:
//   REQUIRE_COMMANDS(Command{5}, Command{20});
int main() {
    harness_reset();

    // T03 only — do not copy. Sequencer is a stub, so seed the buffers to prove
    // the matcher can fail (not only pass on empty).
    harness_debug_push_command(Command{0});
    harness_debug_push_command(Command{20});
    REQUIRE_COMMANDS(Command{0}, Command{20});
    harness_debug_push_ui_event(UiEvent{UiEventKind::Sent, 20});
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::Sent, 20});
    harness_debug_push_button_event(ButtonEvent{10, ButtonEventKind::Tap});
    REQUIRE_BUTTON_EVENTS(ButtonEvent{10, ButtonEventKind::Tap});
    harness_clear_captures();
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_UI_EVENTS();
    REQUIRE_NO_BUTTON_EVENTS();

    harness_press(2);
    harness_advance(DEBOUNCE_MS);
    harness_release(2);
    harness_advance(DEBOUNCE_MS);

    REQUIRE_NO_COMMANDS();
    REQUIRE_COMMANDS();
    REQUIRE_NO_UI_EVENTS();
    REQUIRE_BUTTON_EVENTS(ButtonEvent{2, ButtonEventKind::Tap});
    REQUIRE_LAMP(false);
    return 0;
}
