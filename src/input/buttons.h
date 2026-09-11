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
uint32_t buttons_gpio_levels();

#endif
