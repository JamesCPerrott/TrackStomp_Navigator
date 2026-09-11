#ifndef HARNESS_H
#define HARNESS_H

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
void harness_clear_captures();

// T03 only — do not copy. Seeds capture buffers because the sequencer is a stub.
void harness_debug_push_command(Command command);
void harness_debug_push_ui_event(UiEvent event);
void harness_debug_push_button_event(ButtonEvent event);

[[noreturn]] void harness_fail(const char* file, int line, const char* message);
void harness_require(const char* file, int line, bool cond, const char* expr);
void harness_require_commands(const char* file, int line, std::initializer_list<Command> expected);
void harness_require_ui_events(const char* file, int line, std::initializer_list<UiEvent> expected);
void harness_require_button_events(const char* file, int line,
                                   std::initializer_list<ButtonEvent> expected);
void harness_require_lamp(const char* file, int line, bool expected);

#define REQUIRE(cond)              harness_require(__FILE__, __LINE__, static_cast<bool>(cond), #cond)
#define REQUIRE_COMMANDS(...)      harness_require_commands(__FILE__, __LINE__, {__VA_ARGS__})
#define REQUIRE_NO_COMMANDS()      harness_require_commands(__FILE__, __LINE__, {})
#define REQUIRE_UI_EVENTS(...)     harness_require_ui_events(__FILE__, __LINE__, {__VA_ARGS__})
#define REQUIRE_NO_UI_EVENTS()     harness_require_ui_events(__FILE__, __LINE__, {})
#define REQUIRE_BUTTON_EVENTS(...) harness_require_button_events(__FILE__, __LINE__, {__VA_ARGS__})
#define REQUIRE_NO_BUTTON_EVENTS() harness_require_button_events(__FILE__, __LINE__, {})
#define REQUIRE_LAMP(expected)     harness_require_lamp(__FILE__, __LINE__, expected)

#endif
