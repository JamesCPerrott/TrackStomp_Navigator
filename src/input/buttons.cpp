#include "buttons.h"

#include "config.h"

#include <cstddef>
#include <cstdint>

namespace {

constexpr std::size_t kEventQueueSize = 16;

enum class LockKind : uint8_t { None, Button, Chord };

struct ButtonSlot {
    uint32_t candidate_since;
    uint32_t press_time;
    bool candidate_pressed;
    bool accepted_pressed;
    bool hold_fired;
    bool tap_suppressed;
    bool chord_overlap;
    uint8_t pad[3]{};
};

ButtonSlot g_slots[BUTTON_COUNT]{};
ButtonEvent g_queue[kEventQueueSize]{};
std::size_t g_queue_head  = 0;
std::size_t g_queue_count = 0;
uint32_t g_last_now       = 0;
uint32_t g_chord_start    = 0;
LockKind g_lock           = LockKind::None;
uint8_t g_lock_id         = 0;
bool g_initialized        = false;
bool g_chord_armed        = false;

bool pin_pressed(uint32_t levels, uint8_t index) {
    const uint32_t bit = uint32_t{1} << (BUTTON_GPIO_BASE + index);
    return (levels & bit) == 0U;
}

uint8_t button_id(uint8_t index) {
    return static_cast<uint8_t>(index + 1U);
}

uint8_t button_index(uint8_t id) {
    return static_cast<uint8_t>(id - 1U);
}

bool hold_capable(uint8_t id) {
    switch (id) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 8:
    case 10:
        return true;
    default:
        return false;
    }
}

bool is_chord_member(uint8_t id) {
    return id == CHORD_BUTTON_A || id == CHORD_BUTTON_B;
}

uint8_t other_chord_id(uint8_t id) {
    if (id == CHORD_BUTTON_A) {
        return CHORD_BUTTON_B;
    }
    return CHORD_BUTTON_A;
}

bool is_locked() {
    return g_lock != LockKind::None;
}

void queue_clear() {
    g_queue_head  = 0;
    g_queue_count = 0;
}

void queue_push(uint8_t id, ButtonEventKind kind) {
    if (g_queue_count >= kEventQueueSize) {
        return;
    }
    const std::size_t index = (g_queue_head + g_queue_count) % kEventQueueSize;
    g_queue[index]          = ButtonEvent{id, kind};
    g_queue_count += 1U;
}

void suppress_physically_pressed(uint32_t levels) {
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        if (pin_pressed(levels, i) || g_slots[i].accepted_pressed || g_slots[i].candidate_pressed) {
            g_slots[i].tap_suppressed = true;
        }
    }
}

void init_from_gpio(uint32_t now, uint32_t levels) {
    queue_clear();
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        const bool pressed           = pin_pressed(levels, i);
        g_slots[i].candidate_pressed = pressed;
        g_slots[i].accepted_pressed  = pressed;
        g_slots[i].candidate_since   = now;
        g_slots[i].press_time        = now;
        g_slots[i].hold_fired        = false;
        g_slots[i].tap_suppressed    = false;
        g_slots[i].chord_overlap     = false;
    }
    g_initialized = true;
    g_last_now    = now;
    g_chord_start = 0;
    g_chord_armed = false;
    g_lock        = LockKind::None;
    g_lock_id     = 0;
}

void note_chord_press(ButtonSlot& slot, uint8_t id) {
    if (!is_chord_member(id)) {
        return;
    }
    ButtonSlot& other = g_slots[button_index(other_chord_id(id))];
    if (!other.accepted_pressed) {
        return;
    }
    slot.chord_overlap  = true;
    other.chord_overlap = true;
    g_chord_armed       = true;
    g_chord_start       = slot.press_time;
}

void classify_release(ButtonSlot& slot, uint8_t id, uint32_t levels) {
    if (is_chord_member(id)) {
        g_chord_armed = false;
    }

    const bool emit_tap = !slot.hold_fired && !slot.tap_suppressed && !slot.chord_overlap;
    slot.hold_fired     = false;
    slot.tap_suppressed = false;
    slot.chord_overlap  = false;

    if (g_lock == LockKind::Button && id == g_lock_id) {
        suppress_physically_pressed(levels);
        g_lock    = LockKind::None;
        g_lock_id = 0;
        return;
    }

    if (g_lock == LockKind::Chord) {
        const ButtonSlot& slot_a = g_slots[button_index(CHORD_BUTTON_A)];
        const ButtonSlot& slot_b = g_slots[button_index(CHORD_BUTTON_B)];
        if (!slot_a.accepted_pressed && !slot_b.accepted_pressed) {
            suppress_physically_pressed(levels);
            g_lock = LockKind::None;
        }
        return;
    }

    if (emit_tap) {
        queue_push(id, ButtonEventKind::Tap);
    }
}

void classify_hold(ButtonSlot& slot, uint8_t id, uint32_t now) {
    if (is_locked() || !slot.accepted_pressed || slot.hold_fired || slot.tap_suppressed) {
        return;
    }
    if (!slot.candidate_pressed) {
        return;
    }
    if ((now - slot.press_time) < HOLD_MS) {
        return;
    }
    if (hold_capable(id)) {
        queue_push(id, ButtonEventKind::Hold);
        slot.hold_fired = true;
        g_chord_armed   = false;
        g_lock          = LockKind::Button;
        g_lock_id       = id;
        return;
    }
    if (is_chord_member(id) && g_chord_armed) {
        return;
    }
    slot.tap_suppressed = true;
}

void classify_chord(uint32_t now) {
    if (is_locked() || !g_chord_armed) {
        return;
    }
    const ButtonSlot& slot_a = g_slots[button_index(CHORD_BUTTON_A)];
    const ButtonSlot& slot_b = g_slots[button_index(CHORD_BUTTON_B)];
    if (!slot_a.accepted_pressed || !slot_b.accepted_pressed) {
        return;
    }
    if (!slot_a.candidate_pressed || !slot_b.candidate_pressed) {
        return;
    }
    if ((now - g_chord_start) < CHORD_HOLD_MS) {
        return;
    }
    queue_push(CHORD_BUTTON_A, ButtonEventKind::ChordHold);
    g_chord_armed = false;
    g_lock        = LockKind::Chord;
}

} // namespace

void buttons_scan(uint32_t now) {
    const uint32_t levels = buttons_gpio_levels();

    if (!g_initialized || now < g_last_now) {
        init_from_gpio(now, levels);
        return;
    }

    g_last_now = now;

    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        ButtonSlot& slot   = g_slots[i];
        const uint8_t id   = button_id(i);
        const bool pressed = pin_pressed(levels, i);
        if (pressed != slot.candidate_pressed) {
            slot.candidate_pressed = pressed;
            slot.candidate_since   = now;
        }
        if (slot.candidate_pressed != slot.accepted_pressed &&
            (now - slot.candidate_since) >= DEBOUNCE_MS) {
            slot.accepted_pressed = slot.candidate_pressed;
            if (slot.accepted_pressed) {
                slot.press_time    = slot.candidate_since;
                slot.hold_fired    = false;
                slot.chord_overlap = false;
                if (is_locked()) {
                    slot.tap_suppressed = true;
                } else if (!slot.tap_suppressed) {
                    note_chord_press(slot, id);
                }
            } else {
                classify_release(slot, id, levels);
            }
        }
        classify_hold(slot, id, now);
    }
    classify_chord(now);
}

bool buttons_poll_event(ButtonEvent* out) {
    if (out == nullptr || g_queue_count == 0U) {
        return false;
    }
    *out         = g_queue[g_queue_head];
    g_queue_head = (g_queue_head + 1U) % kEventQueueSize;
    g_queue_count -= 1U;
    return true;
}

#ifndef HOST_TEST
uint32_t buttons_gpio_levels() {
    return 0xFFFFFFFFU;
}
#endif
