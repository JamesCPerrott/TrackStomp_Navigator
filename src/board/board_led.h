#ifndef BOARD_LED_H
#define BOARD_LED_H

// Onboard mirror of ui_lamp(). GP25 on pico2; CYW43 WL_GPIO0 on pico2_w.
// Writes only when the level changes. enabled == false skips every write (Q033).
void board_led_init(bool enabled);
void board_led_set(bool on);

#endif
