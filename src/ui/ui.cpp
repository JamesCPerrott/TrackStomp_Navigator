#include "ui.h"

#include "sequencer/sequencer.h"

#include <cstdint>

namespace {

bool g_lamp         = false;
bool g_setup        = false;
uint32_t g_last_now = 0;

void reset_engine() {
    g_lamp  = false;
    g_setup = false;
}

void apply_event(const UiEvent& event) {
    switch (event.kind) {
    case UiEventKind::SetupEnter:
        g_setup = true;
        break;
    case UiEventKind::SetupExit:
        g_setup = false;
        break;
    case UiEventKind::Idle:
    case UiEventKind::Pending:
    case UiEventKind::Locked:
    case UiEventKind::Sent:
    case UiEventKind::SetupChannel:
        break;
    }
}

// Seven-level stack from PRD §11.4.1. Priorities 1, 2, 4, 5, 6 are T11–T13.
bool resolve_stack() {
    // 1: channel blink (T13)
    // 2: chord progress (T12)
    if (g_setup) {
        return true; // 3: setup solid on
    }
    // 4: hold lockout (T11)
    // 5: cue flash (T11)
    // 6: pending flash (T11)
    return false; // 7: idle off
}

} // namespace

void ui_tick(uint32_t now) {
    if (now < g_last_now) {
        reset_engine();
    }
    g_last_now = now;

    UiEvent event{};
    while (sequencer_poll_ui_event_for_engine(&event)) {
        apply_event(event);
    }
    g_lamp = resolve_stack();
}

bool ui_lamp() {
    return g_lamp;
}
