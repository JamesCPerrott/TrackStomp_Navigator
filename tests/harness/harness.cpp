#include "harness.h"

#include "config.h"
#include "input/buttons.h"
#include "sequencer/sequencer.h"
#include "ui/ui.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>

namespace {

uint32_t g_now_ms = 0;
bool g_pressed[BUTTON_COUNT]{};
uint32_t g_transitions[BUTTON_COUNT]{};
std::vector<Command> g_commands;
std::vector<UiEvent> g_ui_events;
std::vector<ButtonEvent> g_button_events;
std::vector<uint8_t> g_lamp_trace;

bool valid_button(uint8_t button) {
    return button >= 1U && button <= BUTTON_COUNT;
}

std::size_t button_index(uint8_t button) {
    return static_cast<std::size_t>(button - 1U);
}

void check_button(const char* file, int line, uint8_t button) {
    if (!valid_button(button)) {
        harness_fail(file, line, "button id must be in 1..BUTTON_COUNT");
    }
}

void drain_captures() {
    ButtonEvent button_event{};
    while (buttons_poll_event(&button_event)) {
        g_button_events.push_back(button_event);
    }

    Command command{};
    while (sequencer_poll_command(&command)) {
        g_commands.push_back(command);
    }

    UiEvent ui_event{};
    while (sequencer_poll_ui_event(&ui_event)) {
        g_ui_events.push_back(ui_event);
    }
}

void pipeline() {
    buttons_scan(g_now_ms);
    sequencer_tick(g_now_ms);
    ui_tick(g_now_ms);
    drain_captures();
    g_lamp_trace.push_back(ui_lamp() ? uint8_t{1} : uint8_t{0});
}

void set_level_no_scan(uint8_t button, bool pressed) {
    const std::size_t index = button_index(button);
    if (g_pressed[index] != pressed) {
        g_pressed[index] = pressed;
        g_transitions[index] += 1U;
    }
}

void dump_commands(std::ostream& out) {
    out << "commands:";
    if (g_commands.empty()) {
        out << " (none)";
    }
    for (const Command& command : g_commands) {
        out << ' ' << static_cast<unsigned>(command.note);
    }
    out << '\n';
}

const char* ui_kind_name(UiEventKind kind) {
    switch (kind) {
    case UiEventKind::Idle:
        return "Idle";
    case UiEventKind::Pending:
        return "Pending";
    case UiEventKind::Locked:
        return "Locked";
    case UiEventKind::Sent:
        return "Sent";
    case UiEventKind::SetupEnter:
        return "SetupEnter";
    case UiEventKind::SetupChannel:
        return "SetupChannel";
    case UiEventKind::SetupExit:
        return "SetupExit";
    }
    return "unknown";
}

const char* button_kind_name(ButtonEventKind kind) {
    switch (kind) {
    case ButtonEventKind::Tap:
        return "Tap";
    case ButtonEventKind::Hold:
        return "Hold";
    case ButtonEventKind::ChordHold:
        return "ChordHold";
    }
    return "unknown";
}

} // namespace

void harness_reset() {
    g_now_ms = 0;
    for (std::size_t i = 0; i < BUTTON_COUNT; ++i) {
        g_pressed[i]     = false;
        g_transitions[i] = 0;
    }
    harness_clear_captures();
    pipeline();
}

uint32_t harness_now_ms() {
    return g_now_ms;
}

void harness_advance(uint32_t ms) {
    for (uint32_t i = 0; i < ms; ++i) {
        g_now_ms += 1U;
        pipeline();
    }
}

void harness_advance_to(uint32_t t) {
    if (t < g_now_ms) {
        harness_fail(__FILE__, __LINE__, "harness_advance_to cannot move backwards");
    }
    harness_advance(t - g_now_ms);
}

void harness_set_pressed(uint8_t button, bool pressed) {
    check_button(__FILE__, __LINE__, button);
    set_level_no_scan(button, pressed);
    pipeline();
}

void harness_press(uint8_t button) {
    harness_set_pressed(button, true);
}

void harness_release(uint8_t button) {
    harness_set_pressed(button, false);
}

bool harness_button_pressed(uint8_t button) {
    check_button(__FILE__, __LINE__, button);
    return g_pressed[button_index(button)];
}

void harness_inject_bounce(uint8_t button, uint8_t transitions, uint32_t duration_ms) {
    check_button(__FILE__, __LINE__, button);

    uint32_t remaining = transitions;
    if (duration_ms == 0U) {
        for (uint32_t i = 0; i < remaining; ++i) {
            set_level_no_scan(button, !g_pressed[button_index(button)]);
        }
    } else {
        for (uint32_t step = 0; step < duration_ms; ++step) {
            const uint32_t steps_left = duration_ms - step;
            uint32_t n                = 0;
            if (remaining > 0U) {
                n = (remaining + steps_left - 1U) / steps_left;
            }
            for (uint32_t i = 0; i < n; ++i) {
                set_level_no_scan(button, !g_pressed[button_index(button)]);
            }
            remaining -= n;
            harness_advance(1);
        }
    }

    if (!g_pressed[button_index(button)]) {
        harness_press(button);
    }
}

uint32_t harness_gpio_transition_count(uint8_t button) {
    check_button(__FILE__, __LINE__, button);
    return g_transitions[button_index(button)];
}

const std::vector<uint8_t>& harness_lamp_trace() {
    return g_lamp_trace;
}

void harness_clear_captures() {
    g_commands.clear();
    g_ui_events.clear();
    g_button_events.clear();
    g_lamp_trace.clear();
}

void harness_debug_push_command(Command command) {
    g_commands.push_back(command);
}

void harness_debug_push_ui_event(UiEvent event) {
    g_ui_events.push_back(event);
}

void harness_debug_push_button_event(ButtonEvent event) {
    g_button_events.push_back(event);
}

[[noreturn]] void harness_fail(const char* file, int line, const char* message) {
    std::cerr << file << ':' << line << " FAIL: " << message << '\n';
    std::exit(1);
}

void harness_require(const char* file, int line, bool cond, const char* expr) {
    if (!cond) {
        std::string message = "REQUIRE failed: ";
        message += expr;
        harness_fail(file, line, message.c_str());
    }
}

void harness_require_commands(const char* file, int line, std::initializer_list<Command> expected) {
    if (expected.size() != g_commands.size()) {
        std::cerr << file << ':' << line << " FAIL: expected " << expected.size()
                  << " command(s), got " << g_commands.size() << '\n';
        dump_commands(std::cerr);
        std::exit(1);
    }

    std::size_t i = 0;
    for (const Command& want : expected) {
        if (g_commands[i].note != want.note) {
            std::cerr << file << ':' << line << " FAIL: command[" << i << "] note "
                      << static_cast<unsigned>(g_commands[i].note)
                      << " != " << static_cast<unsigned>(want.note) << '\n';
            dump_commands(std::cerr);
            std::exit(1);
        }
        ++i;
    }
}

void harness_require_ui_events(const char* file, int line,
                               std::initializer_list<UiEvent> expected) {
    if (expected.size() != g_ui_events.size()) {
        std::cerr << file << ':' << line << " FAIL: expected " << expected.size()
                  << " UiEvent(s), got " << g_ui_events.size() << '\n';
        std::exit(1);
    }

    std::size_t i = 0;
    for (const UiEvent& want : expected) {
        if (g_ui_events[i].kind != want.kind || g_ui_events[i].value != want.value) {
            std::cerr << file << ':' << line << " FAIL: ui[" << i << "] "
                      << ui_kind_name(g_ui_events[i].kind) << '('
                      << static_cast<unsigned>(g_ui_events[i].value)
                      << ") != " << ui_kind_name(want.kind) << '('
                      << static_cast<unsigned>(want.value) << ")\n";
            std::exit(1);
        }
        ++i;
    }
}

void harness_require_button_events(const char* file, int line,
                                   std::initializer_list<ButtonEvent> expected) {
    if (expected.size() != g_button_events.size()) {
        std::cerr << file << ':' << line << " FAIL: expected " << expected.size()
                  << " ButtonEvent(s), got " << g_button_events.size() << '\n';
        std::exit(1);
    }

    std::size_t i = 0;
    for (const ButtonEvent& want : expected) {
        if (g_button_events[i].id != want.id || g_button_events[i].kind != want.kind) {
            std::cerr << file << ':' << line << " FAIL: button[" << i << "] "
                      << static_cast<unsigned>(g_button_events[i].id) << ' '
                      << button_kind_name(g_button_events[i].kind)
                      << " != " << static_cast<unsigned>(want.id) << ' '
                      << button_kind_name(want.kind) << '\n';
            std::exit(1);
        }
        ++i;
    }
}

void harness_require_lamp(const char* file, int line, bool expected) {
    const bool actual = ui_lamp();
    if (actual != expected) {
        std::cerr << file << ':' << line << " FAIL: lamp " << (actual ? "on" : "off")
                  << " != " << (expected ? "on" : "off") << '\n';
        std::exit(1);
    }
}
