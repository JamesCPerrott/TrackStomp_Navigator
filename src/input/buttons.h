#ifndef BUTTONS_H
#define BUTTONS_H

#include <cstdint>

enum class ButtonEventKind : uint8_t { Tap, Hold, ChordHold };

struct ButtonEvent {
    uint8_t id;
    ButtonEventKind kind;
};

void buttons_scan(uint32_t now);
bool buttons_poll_event(ButtonEvent* out);
bool buttons_poll_sequencer_event(ButtonEvent* out);
void buttons_set_setup_active(bool active);
bool buttons_accepted_pressed(uint8_t id);
uint32_t buttons_gpio_levels();

#endif
