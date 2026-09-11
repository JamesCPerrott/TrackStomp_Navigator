#include "sequencer.h"

#include <cstdint>

void sequencer_tick(uint32_t now) {
    static_cast<void>(now);
}

bool sequencer_poll_command(Command* out) {
    static_cast<void>(out);
    return false;
}

bool sequencer_poll_ui_event(UiEvent* out) {
    static_cast<void>(out);
    return false;
}
