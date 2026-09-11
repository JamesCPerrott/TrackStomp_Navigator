#include "config.h"
#include "harness.h"
#include "input/buttons.h"
#include "sequencer/sequencer.h"

// Capture-buffer exemplar with real sequencer output: 2 then 7 → note 5.
int main() {
    harness_reset();
    REQUIRE_NO_COMMANDS();
    REQUIRE_NO_UI_EVENTS();
    REQUIRE_NO_BUTTON_EVENTS();

    harness_press(2);
    harness_advance(DEBOUNCE_MS);
    harness_release(2);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_NO_COMMANDS();
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::Pending, 2});
    REQUIRE_BUTTON_EVENTS(ButtonEvent{2, ButtonEventKind::Tap});

    harness_clear_captures();
    harness_press(7);
    harness_advance(DEBOUNCE_MS);
    harness_release(7);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_COMMANDS(Command{5});
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::Sent, 5});
    REQUIRE_BUTTON_EVENTS(ButtonEvent{7, ButtonEventKind::Tap});
    REQUIRE_LAMP(false);
    return 0;
}
