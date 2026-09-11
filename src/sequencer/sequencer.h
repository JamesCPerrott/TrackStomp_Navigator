#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <cstdint>

struct Command {
    uint8_t note;
};

enum class UiEventKind : uint8_t {
    Idle,
    Pending,
    Locked,
    Sent,
    SetupEnter,
    SetupChannel,
    SetupExit
};

struct UiEvent {
    UiEventKind kind;
    uint8_t value; // button, note, or channel; 0 when unused
};

void sequencer_tick(uint32_t now);
bool sequencer_poll_command(Command* out);
bool sequencer_poll_command_for_midi(Command* out);
bool sequencer_poll_ui_event(UiEvent* out);
bool sequencer_poll_ui_event_for_engine(UiEvent* out);
bool sequencer_poll_setup_commit(uint8_t* out);

#endif
