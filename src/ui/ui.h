#ifndef UI_H
#define UI_H

#include <cstdint>

void ui_tick(uint32_t now);
bool ui_lamp();
uint16_t ui_switch_leds();
void ui_indicate_channel(uint8_t n);
void ui_led_init();

#endif
