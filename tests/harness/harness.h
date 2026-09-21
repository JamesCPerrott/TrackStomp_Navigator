#ifndef HARNESS_H
#define HARNESS_H

#include "config.h"
#include "input/buttons.h"
#include "sequencer/sequencer.h"

#include <cstdint>
#include <initializer_list>
#include <vector>

void harness_reset();
uint32_t harness_now_ms();
void harness_advance(uint32_t ms);
void harness_advance_to(uint32_t t);

void harness_set_pressed(uint8_t button, bool pressed);
void harness_press(uint8_t button);
void harness_release(uint8_t button);
bool harness_button_pressed(uint8_t button);
void harness_inject_bounce(uint8_t button, uint8_t transitions, uint32_t duration_ms);
uint32_t harness_gpio_transition_count(uint8_t button);

const std::vector<uint8_t>& harness_lamp_trace();
const std::vector<uint16_t>& harness_switch_trace();
uint16_t harness_switch_leds();
uint8_t harness_switch_led_count();
void harness_clear_captures();

// T03 only — do not copy. Seeds capture buffers because the sequencer is a stub.
void harness_debug_push_command(Command command);
void harness_debug_push_ui_event(UiEvent event);
void harness_debug_push_button_event(ButtonEvent event);
void harness_debug_set_switch_leds(uint16_t mask);

[[noreturn]] void harness_fail(const char* file, int line, const char* message);
void harness_require(const char* file, int line, bool cond, const char* expr);
void harness_require_commands(const char* file, int line, std::initializer_list<Command> expected);
void harness_require_ui_events(const char* file, int line, std::initializer_list<UiEvent> expected);
void harness_require_button_events(const char* file, int line,
                                   std::initializer_list<ButtonEvent> expected);
void harness_require_lamp(const char* file, int line, bool expected);
void harness_require_switch_leds(const char* file, int line, uint16_t expected);

template <uint8_t N>
constexpr uint16_t switch_led_mask() {
    static_assert(N >= 1U && N <= SWITCH_LED_COUNT,
                  "SWITCH_LED argument must be 1..SWITCH_LED_COUNT");
    return static_cast<uint16_t>(uint16_t{1} << (N - 1U));
}

#define REQUIRE(cond)              harness_require(__FILE__, __LINE__, static_cast<bool>(cond), #cond)
#define REQUIRE_COMMANDS(...)      harness_require_commands(__FILE__, __LINE__, {__VA_ARGS__})
#define REQUIRE_NO_COMMANDS()      harness_require_commands(__FILE__, __LINE__, {})
#define REQUIRE_UI_EVENTS(...)     harness_require_ui_events(__FILE__, __LINE__, {__VA_ARGS__})
#define REQUIRE_NO_UI_EVENTS()     harness_require_ui_events(__FILE__, __LINE__, {})
#define REQUIRE_BUTTON_EVENTS(...) harness_require_button_events(__FILE__, __LINE__, {__VA_ARGS__})
#define REQUIRE_NO_BUTTON_EVENTS() harness_require_button_events(__FILE__, __LINE__, {})
#define REQUIRE_LAMP(expected)     harness_require_lamp(__FILE__, __LINE__, expected)
#define REQUIRE_SWITCH_LEDS(mask)  harness_require_switch_leds(__FILE__, __LINE__, mask)
#define REQUIRE_NO_SWITCH_LEDS()   harness_require_switch_leds(__FILE__, __LINE__, 0U)
#define SWITCH_LED(n)              switch_led_mask<static_cast<uint8_t>(n)>()

#endif
