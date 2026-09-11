#include "buttons.h"

#include "config.h"

#include <cstddef>
#include <cstdint>

namespace {

constexpr std::size_t kEventQueueSize = 16;

struct DebounceSlot {
    bool candidate_pressed;
    bool accepted_pressed;
    uint32_t candidate_since;
};

DebounceSlot g_slots[BUTTON_COUNT]{};
ButtonEvent g_queue[kEventQueueSize]{};
std::size_t g_queue_head  = 0;
std::size_t g_queue_count = 0;
uint32_t g_last_now       = 0;
bool g_initialized        = false;

bool pin_pressed(uint32_t levels, uint8_t index) {
    const uint32_t bit = uint32_t{1} << (BUTTON_GPIO_BASE + index);
    return (levels & bit) == 0U;
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

void init_from_gpio(uint32_t now, uint32_t levels) {
    queue_clear();
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        const bool pressed           = pin_pressed(levels, i);
        g_slots[i].candidate_pressed = pressed;
        g_slots[i].accepted_pressed  = pressed;
        g_slots[i].candidate_since   = now;
    }
    g_initialized = true;
    g_last_now    = now;
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
        DebounceSlot& slot = g_slots[i];
        const bool pressed = pin_pressed(levels, i);
        if (pressed != slot.candidate_pressed) {
            slot.candidate_pressed = pressed;
            slot.candidate_since   = now;
        }
        if (slot.candidate_pressed != slot.accepted_pressed &&
            (now - slot.candidate_since) >= DEBOUNCE_MS) {
            slot.accepted_pressed = slot.candidate_pressed;
            if (slot.accepted_pressed) {
                queue_push(static_cast<uint8_t>(i + 1U), ButtonEventKind::Tap);
            }
        }
    }
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
