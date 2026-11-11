#include "harness.h"

#include "config.h"
#include "input/buttons.h"
#include "link/link_state.h"
#include "main.h"
#include "midi/ble_midi.h"
#include "midi/midi_out.h"
#include "midi/transport.h"
#include "sequencer/sequencer.h"
#include "ui/ui.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <sstream>
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
std::vector<uint16_t> g_switch_trace;
uint16_t g_switch_leds = 0;

// Fake transport (Phase 10). g_link_events is the fake's outbox: queued by
// harness_link_event, polled by loop_dispatch through ble_midi_poll_event.
std::vector<LinkEvent> g_link_events;
std::vector<LinkRequest> g_link_requests;
std::vector<TransportSend> g_transport_sends;
std::vector<std::vector<uint8_t>> g_ble_packets;

constexpr uint8_t kMidiStatusBit        = 0x80U;
constexpr uint8_t kMidiKindMask         = 0xF0U;
constexpr uint8_t kMidiNoteOn           = 0x90U;
constexpr uint8_t kMidiNoteOff          = 0x80U;
constexpr std::size_t kMidiMessageBytes = 3;
constexpr std::size_t kBlePacketBytes   = 5;
constexpr uint8_t kBleHeaderMask        = 0xC0U; // header is 1 0 t12..t7 (PRD §9.3.2)
constexpr uint8_t kBleHeaderBits        = 0x80U;

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

[[noreturn]] void fail_wire(int line, const char* wire, const char* what,
                            const std::vector<uint8_t>& bytes) {
    std::ostringstream out;
    out << wire << " wire at t=" << g_now_ms << " ms: " << what << ':';
    if (bytes.empty()) {
        out << " (no bytes)";
    }
    for (const uint8_t byte : bytes) {
        out << " 0x" << std::hex << static_cast<unsigned>(byte);
    }
    const std::string message = out.str();
    harness_fail(__FILE__, line, message.c_str());
}

// The harness's own parser for one three-byte channel message taken off a
// fake wire. It does not share code with midi_out or the BLE encoder.
// Returns true for a Note On, which is a cue. A Note Off is legal only when
// SEND_NOTE_OFF is set and is not a cue. Anything else fails the test.
bool decode_channel_message(const char* wire, const std::vector<uint8_t>& message, uint8_t& note) {
    if (message.size() != kMidiMessageBytes) {
        fail_wire(__LINE__, wire, "ended inside a MIDI message", message);
    }
    const uint8_t kind  = message[0] & kMidiKindMask;
    const bool note_on  = (kind == kMidiNoteOn);
    const bool note_off = SEND_NOTE_OFF && (kind == kMidiNoteOff);
    if (!note_on && !note_off) {
        fail_wire(__LINE__, wire, "status byte is not a Note On", message);
    }
    if ((message[1] & kMidiStatusBit) != 0U || (message[2] & kMidiStatusBit) != 0U) {
        fail_wire(__LINE__, wire, "data byte has bit 7 set", message);
    }
    note = message[1];
    return note_on;
}

// Turns every byte midi_out wrote to the USB wire into TransportSends.
void drain_usb_wire() {
    if (midi_out_harness_bytes_lost()) {
        harness_fail(__FILE__, __LINE__, "USB wire capture overflowed before it was drained");
    }
    std::vector<uint8_t> message;
    uint8_t byte = 0;
    while (midi_out_poll_byte_for_harness(&byte)) {
        message.push_back(byte);
        if (message.size() == kMidiMessageBytes) {
            uint8_t note = 0;
            if (decode_channel_message("USB", message, note)) {
                g_transport_sends.push_back(TransportSend{Transport::Usb, note});
            }
            message.clear();
        }
    }
    if (!message.empty()) {
        fail_wire(__LINE__, "USB", "ended inside a MIDI message", message);
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

    drain_usb_wire();
}

std::string format_switch_mask(uint16_t mask) {
    std::ostringstream out;
    out << '{';
    bool first = true;
    for (uint8_t led = 1; led <= SWITCH_LED_COUNT; ++led) {
        const uint16_t bit = static_cast<uint16_t>(uint16_t{1} << (led - 1U));
        if ((mask & bit) != 0U) {
            if (!first) {
                out << ',';
            }
            first = false;
            out << static_cast<unsigned>(led);
        }
    }
    out << "} (";
    for (uint8_t i = 0; i < SWITCH_LED_COUNT; ++i) {
        const uint8_t bit   = static_cast<uint8_t>(SWITCH_LED_COUNT - 1U - i);
        const uint16_t flag = static_cast<uint16_t>(uint16_t{1} << bit);
        out << (((mask & flag) != 0U) ? '1' : '0');
    }
    out << ')';
    return out.str();
}

void reject_unused_switch_bits(const char* file, int line, uint16_t mask) {
    const uint16_t used = static_cast<uint16_t>((uint16_t{1} << SWITCH_LED_COUNT) - 1U);
    if ((mask & static_cast<uint16_t>(~used)) != 0U) {
        std::ostringstream out;
        out << "switch LED mask has bits at or above SWITCH_LED_COUNT: 0x" << std::hex
            << static_cast<unsigned>(mask);
        const std::string message = out.str();
        harness_fail(file, line, message.c_str());
    }
}

void pipeline() {
    buttons_scan(g_now_ms);
    sequencer_tick(g_now_ms);
    link_tick(g_now_ms);
    loop_dispatch(g_now_ms);
    ui_tick(g_now_ms);
    drain_captures();
    g_lamp_trace.push_back(ui_lamp() ? uint8_t{1} : uint8_t{0});
    const uint16_t switch_mask = ui_switch_leds();
    reject_unused_switch_bits(__FILE__, __LINE__, switch_mask);
    g_switch_leds = switch_mask;
    g_switch_trace.push_back(switch_mask);
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
    case ButtonEventKind::PairChordHold:
        return "PairChordHold";
    }
    return "unknown";
}

const char* link_request_kind_name(LinkRequestKind kind) {
    switch (kind) {
    case LinkRequestKind::BeginPairing:
        return "BeginPairing";
    case LinkRequestKind::EndPairing:
        return "EndPairing";
    }
    return "unknown";
}

const char* transport_name(Transport transport) {
    switch (transport) {
    case Transport::None:
        return "None";
    case Transport::Usb:
        return "Usb";
    case Transport::Ble:
        return "Ble";
    }
    return "unknown";
}

void dump_link_requests(std::ostream& out) {
    out << "link requests:";
    if (g_link_requests.empty()) {
        out << " (none)";
    }
    for (const LinkRequest& request : g_link_requests) {
        out << ' ' << link_request_kind_name(request.kind);
    }
    out << '\n';
}

void dump_transport_sends(std::ostream& out) {
    out << "transport sends:";
    if (g_transport_sends.empty()) {
        out << " (none)";
    }
    for (const TransportSend& send : g_transport_sends) {
        out << ' ' << transport_name(send.transport) << ':' << static_cast<unsigned>(send.note);
    }
    out << '\n';
}

} // namespace

// The host implementation of midi/ble_midi.h. loop_dispatch and midi_out call
// these exactly as they call the real transport on a target.

void ble_midi_request(LinkRequest request) {
    g_link_requests.push_back(request);
}

bool ble_midi_poll_event(LinkEvent* out) {
    if (out == nullptr || g_link_events.empty()) {
        return false;
    }
    *out = g_link_events.front();
    g_link_events.erase(g_link_events.begin());
    return true;
}

void ble_midi_send(const uint8_t* packet, uint8_t len) {
    // USB bytes written before this packet stay ahead of it in the send list.
    drain_usb_wire();
    if (packet == nullptr && len != 0U) {
        harness_fail(__FILE__, __LINE__, "ble_midi_send was given a null packet");
    }
    std::vector<uint8_t> bytes;
    if (packet != nullptr) {
        bytes.assign(packet, packet + len);
    }
    g_ble_packets.push_back(bytes);
    if (bytes.size() != kBlePacketBytes) {
        fail_wire(__LINE__, "BLE", "packet is not 5 bytes (PRD §9.3.2)", bytes);
    }
    if ((bytes[0] & kBleHeaderMask) != kBleHeaderBits) {
        fail_wire(__LINE__, "BLE", "header byte is not 10xxxxxx", bytes);
    }
    if ((bytes[1] & kMidiStatusBit) == 0U) {
        fail_wire(__LINE__, "BLE", "timestamp byte has bit 7 clear", bytes);
    }
    // The note is read from the packet itself: byte 3, after header,
    // timestamp, and status.
    const std::vector<uint8_t> message(bytes.begin() + 2, bytes.end());
    uint8_t note = 0;
    if (decode_channel_message("BLE", message, note)) {
        g_transport_sends.push_back(TransportSend{Transport::Ble, note});
    }
}

void harness_reset() {
    g_now_ms = 0;
    for (std::size_t i = 0; i < BUTTON_COUNT; ++i) {
        g_pressed[i]     = false;
        g_transitions[i] = 0;
    }
    // Unconditional: the modules reset themselves only when time runs
    // backwards, which a reset at t=0 does not do. buttons first; ui_reset
    // reads its event counter.
    buttons_reset();
    sequencer_reset();
    ui_reset();
    midi_out_reset();
    link_reset();
    g_link_events.clear();
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

const std::vector<uint16_t>& harness_switch_trace() {
    return g_switch_trace;
}

uint16_t harness_switch_leds() {
    return g_switch_leds;
}

uint8_t harness_switch_led_count() {
    uint8_t count = 0;
    for (uint8_t bit = 0; bit < SWITCH_LED_COUNT; ++bit) {
        const uint16_t flag = static_cast<uint16_t>(uint16_t{1} << bit);
        if ((g_switch_leds & flag) != 0U) {
            count = static_cast<uint8_t>(count + 1U);
        }
    }
    return count;
}

void harness_clear_captures() {
    g_commands.clear();
    g_ui_events.clear();
    g_button_events.clear();
    g_lamp_trace.clear();
    g_switch_trace.clear();
    drain_usb_wire(); // so bytes already on the wire do not surface after the clear
    g_link_requests.clear();
    g_transport_sends.clear();
    g_ble_packets.clear();
}

void harness_set_usb_mounted(bool mounted) {
    midi_out_set_usb_mounted(mounted);
    pipeline();
}

void harness_link_event(LinkEventKind kind) {
    g_link_events.push_back(LinkEvent{kind});
    pipeline();
}

const std::vector<std::vector<uint8_t>>& harness_ble_packets() {
    return g_ble_packets;
}

void harness_debug_link_request(LinkRequestKind kind) {
    link_on_request(LinkRequest{kind});
    pipeline();
}

void harness_debug_ble_packet(std::initializer_list<uint8_t> bytes) {
    const std::vector<uint8_t> packet(bytes);
    if (packet.size() > UINT8_MAX) {
        harness_fail(__FILE__, __LINE__, "harness_debug_ble_packet takes at most 255 bytes");
    }
    ble_midi_send(packet.data(), static_cast<uint8_t>(packet.size()));
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

void harness_debug_set_switch_leds(uint16_t mask) {
    reject_unused_switch_bits(__FILE__, __LINE__, mask);
    g_switch_leds = mask;
    if (!g_switch_trace.empty()) {
        g_switch_trace.back() = mask;
    }
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

void harness_require_switch_leds(const char* file, int line, uint16_t expected) {
    if (g_switch_leds != expected) {
        std::ostringstream out;
        out << "switch LEDs " << format_switch_mask(g_switch_leds)
            << " != " << format_switch_mask(expected);
        const std::string message = out.str();
        harness_fail(file, line, message.c_str());
    }
}

void harness_require_link_requests(const char* file, int line,
                                   std::initializer_list<LinkRequest> expected) {
    if (expected.size() != g_link_requests.size()) {
        std::cerr << file << ':' << line << " FAIL: expected " << expected.size()
                  << " LinkRequest(s), got " << g_link_requests.size() << '\n';
        dump_link_requests(std::cerr);
        std::exit(1);
    }

    std::size_t i = 0;
    for (const LinkRequest& want : expected) {
        if (g_link_requests[i].kind != want.kind) {
            std::cerr << file << ':' << line << " FAIL: link request[" << i << "] "
                      << link_request_kind_name(g_link_requests[i].kind)
                      << " != " << link_request_kind_name(want.kind) << '\n';
            dump_link_requests(std::cerr);
            std::exit(1);
        }
        ++i;
    }
}

void harness_require_transport_sends(const char* file, int line,
                                     std::initializer_list<TransportSend> expected) {
    drain_usb_wire();
    if (expected.size() != g_transport_sends.size()) {
        std::cerr << file << ':' << line << " FAIL: expected " << expected.size()
                  << " TransportSend(s), got " << g_transport_sends.size() << '\n';
        dump_transport_sends(std::cerr);
        std::exit(1);
    }

    std::size_t i = 0;
    for (const TransportSend& want : expected) {
        const TransportSend& got = g_transport_sends[i];
        if (got.transport != want.transport || got.note != want.note) {
            std::cerr << file << ':' << line << " FAIL: transport send[" << i << "] "
                      << transport_name(got.transport) << ':' << static_cast<unsigned>(got.note)
                      << " != " << transport_name(want.transport) << ':'
                      << static_cast<unsigned>(want.note) << '\n';
            dump_transport_sends(std::cerr);
            std::exit(1);
        }
        ++i;
    }
}
