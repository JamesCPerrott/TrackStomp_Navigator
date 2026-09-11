#include "sequencer.h"

#include "config.h"
#include "input/buttons.h"

#include <cstddef>
#include <cstdint>

namespace {

constexpr std::size_t kQueueSize = 16;

enum class SeqState : uint8_t { Idle, Pending, Setup };

SeqState g_state          = SeqState::Idle;
uint8_t g_prefix          = 0;
uint8_t g_current_channel = DEFAULT_MIDI_CHANNEL;
uint8_t g_pending_channel = DEFAULT_MIDI_CHANNEL;
uint32_t g_deadline       = 0;
uint32_t g_last_now       = 0;
bool g_exit_ready         = false;
bool g_exit_overlap       = false;
Command g_commands[kQueueSize]{};
UiEvent g_ui_events[kQueueSize]{};
UiEvent g_ui_engine[kQueueSize]{};
std::size_t g_cmd_head     = 0;
std::size_t g_cmd_count    = 0;
std::size_t g_ui_head      = 0;
std::size_t g_ui_count     = 0;
std::size_t g_ui_eng_head  = 0;
std::size_t g_ui_eng_count = 0;

bool is_prefix(uint8_t id) {
    return id >= 1U && id <= 5U;
}

bool is_suffix(uint8_t id) {
    return id >= 6U && id <= 9U;
}

void push_command(uint8_t note) {
    if (g_cmd_count >= kQueueSize) {
        return;
    }
    const std::size_t index = (g_cmd_head + g_cmd_count) % kQueueSize;
    g_commands[index]       = Command{note};
    g_cmd_count += 1U;
}

void push_ui(UiEventKind kind, uint8_t value) {
    const UiEvent event{kind, value};
    if (g_ui_count < kQueueSize) {
        const std::size_t index = (g_ui_head + g_ui_count) % kQueueSize;
        g_ui_events[index]      = event;
        g_ui_count += 1U;
    }
    if (g_ui_eng_count < kQueueSize) {
        const std::size_t index = (g_ui_eng_head + g_ui_eng_count) % kQueueSize;
        g_ui_engine[index]      = event;
        g_ui_eng_count += 1U;
    }
}

bool lookup_note(CueTrigger trigger, uint8_t button_a, uint8_t button_b, uint8_t& note) {
    for (const Cue& cue : CUE_TABLE) {
        if (cue.trigger == trigger && cue.button_a == button_a && cue.button_b == button_b) {
            note = cue.note;
            return true;
        }
    }
    return false;
}

void send_note(uint8_t note) {
    push_command(note);
    push_ui(UiEventKind::Sent, note);
}

void enter_pending(uint8_t prefix, uint32_t now) {
    g_state    = SeqState::Pending;
    g_prefix   = prefix;
    g_deadline = now + SEQUENCE_TIMEOUT_MS;
    push_ui(UiEventKind::Pending, prefix);
}

void enter_idle() {
    g_state        = SeqState::Idle;
    g_prefix       = 0;
    g_exit_ready   = false;
    g_exit_overlap = false;
    buttons_set_setup_active(false);
}

void enter_setup() {
    g_state           = SeqState::Setup;
    g_prefix          = 0;
    g_pending_channel = g_current_channel;
    g_exit_ready      = false;
    g_exit_overlap    = false;
    buttons_set_setup_active(true);
    push_ui(UiEventKind::SetupEnter, g_current_channel);
}

void exit_setup() {
    g_current_channel = g_pending_channel;
    push_ui(UiEventKind::SetupExit, g_current_channel);
    enter_idle();
}

void reset_sequencer() {
    g_current_channel = DEFAULT_MIDI_CHANNEL;
    g_pending_channel = DEFAULT_MIDI_CHANNEL;
    enter_idle();
    g_deadline     = 0;
    g_cmd_head     = 0;
    g_cmd_count    = 0;
    g_ui_head      = 0;
    g_ui_count     = 0;
    g_ui_eng_head  = 0;
    g_ui_eng_count = 0;
}

void resolve_pair(uint8_t prefix, uint8_t suffix) {
    const CueTrigger trigger = (suffix == prefix) ? CueTrigger::SelfPair : CueTrigger::PrefixSuffix;
    uint8_t note             = 0;
    if (lookup_note(trigger, prefix, suffix, note)) {
        send_note(note);
    }
    enter_idle();
}

void on_tap(uint8_t id, uint32_t now) {
    if (g_state == SeqState::Setup) {
        g_pending_channel = id;
        g_deadline        = now + SETUP_TIMEOUT_MS;
        push_ui(UiEventKind::SetupChannel, id);
        return;
    }

    if (g_state == SeqState::Pending) {
        if (is_suffix(id) || id == g_prefix) {
            resolve_pair(g_prefix, id);
            return;
        }
        if (is_prefix(id)) {
            enter_pending(id, now);
            return;
        }
        if (id == 10U) {
            uint8_t note = 0;
            if (lookup_note(CueTrigger::Tap, 10, 0, note)) {
                send_note(note);
            }
            enter_idle();
        }
        return;
    }

    if (is_prefix(id)) {
        enter_pending(id, now);
        return;
    }
    if (id == 10U) {
        uint8_t note = 0;
        if (lookup_note(CueTrigger::Tap, 10, 0, note)) {
            send_note(note);
        }
        enter_idle();
    }
}

void on_hold(uint8_t id) {
    if (g_state == SeqState::Setup) {
        return;
    }
    uint8_t note = 0;
    if (lookup_note(CueTrigger::Hold, id, 0, note)) {
        push_command(note);
        push_ui(UiEventKind::Locked, id);
    }
    enter_idle();
}

void on_chord_hold(uint32_t now) {
    g_deadline = now + SETUP_TIMEOUT_MS;
    enter_setup();
}

} // namespace

void sequencer_tick(uint32_t now) {
    if (now < g_last_now) {
        reset_sequencer();
    }
    g_last_now = now;

    ButtonEvent event{};
    while (buttons_poll_sequencer_event(&event)) {
        switch (event.kind) {
        case ButtonEventKind::Tap:
            on_tap(event.id, now);
            break;
        case ButtonEventKind::Hold:
            on_hold(event.id);
            break;
        case ButtonEventKind::ChordHold:
            on_chord_hold(now);
            break;
        }
    }

    if (g_state == SeqState::Setup) {
        bool any_down = false;
        for (uint8_t id = 1; id <= BUTTON_COUNT; ++id) {
            if (buttons_accepted_pressed(id)) {
                any_down = true;
                break;
            }
        }
        if (any_down) {
            g_deadline = now + SETUP_TIMEOUT_MS;
        }

        const bool a_down = buttons_accepted_pressed(CHORD_BUTTON_A);
        const bool b_down = buttons_accepted_pressed(CHORD_BUTTON_B);
        if (!g_exit_ready) {
            if (!a_down && !b_down) {
                g_exit_ready = true;
            }
        } else if (a_down && b_down) {
            g_exit_overlap = true;
        } else if (g_exit_overlap && !a_down && !b_down) {
            exit_setup();
        }

        if (g_state == SeqState::Setup && !any_down && now >= g_deadline) {
            exit_setup();
        }
    }

    if (g_state == SeqState::Pending && now >= g_deadline) {
        enter_idle();
        push_ui(UiEventKind::Idle, 0);
    }
}

bool sequencer_poll_command(Command* out) {
    if (out == nullptr || g_cmd_count == 0U) {
        return false;
    }
    *out       = g_commands[g_cmd_head];
    g_cmd_head = (g_cmd_head + 1U) % kQueueSize;
    g_cmd_count -= 1U;
    return true;
}

bool sequencer_poll_ui_event(UiEvent* out) {
    if (out == nullptr || g_ui_count == 0U) {
        return false;
    }
    *out      = g_ui_events[g_ui_head];
    g_ui_head = (g_ui_head + 1U) % kQueueSize;
    g_ui_count -= 1U;
    return true;
}

bool sequencer_poll_ui_event_for_engine(UiEvent* out) {
    if (out == nullptr || g_ui_eng_count == 0U) {
        return false;
    }
    *out          = g_ui_engine[g_ui_eng_head];
    g_ui_eng_head = (g_ui_eng_head + 1U) % kQueueSize;
    g_ui_eng_count -= 1U;
    return true;
}
