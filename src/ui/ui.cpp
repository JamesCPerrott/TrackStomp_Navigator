#include "ui.h"

#include "config.h"
#include "input/buttons.h"
#include "sequencer/sequencer.h"

#ifndef HOST_TEST
#include "hardware/gpio.h"
#endif

#include <cstdint>

namespace {

enum class CuePhase : uint8_t { None, Gap, Flash };

bool g_lamp              = false;
bool g_setup             = false;
bool g_pending           = false;
bool g_blink_active      = false;
uint8_t g_lock_id        = 0;
uint8_t g_blink_n        = 0;
uint32_t g_last_now      = 0;
uint32_t g_pending_start = 0;
uint32_t g_lock_start    = 0;
uint32_t g_cue_start     = 0;
uint32_t g_blink_start   = 0;
CuePhase g_cue_phase     = CuePhase::None;

void reset_engine() {
    g_lamp          = false;
    g_setup         = false;
    g_pending       = false;
    g_blink_active  = false;
    g_lock_id       = 0;
    g_blink_n       = 0;
    g_pending_start = 0;
    g_lock_start    = 0;
    g_cue_start     = 0;
    g_blink_start   = 0;
    g_cue_phase     = CuePhase::None;
}

bool flash_on(uint32_t now, uint32_t start, uint32_t half_period) {
    return ((now - start) / half_period) % 2U == 0U;
}

void start_blink(uint8_t n, uint32_t now) {
    g_blink_active = true;
    g_blink_n      = n;
    g_blink_start  = now;
}

void abort_blink() {
    g_blink_active = false;
}

void apply_event(const UiEvent& event, uint32_t now) {
    switch (event.kind) {
    case UiEventKind::SetupEnter:
        g_setup     = true;
        g_pending   = false;
        g_cue_phase = CuePhase::None;
        abort_blink();
        break;
    case UiEventKind::SetupExit:
        g_setup = false;
        start_blink(event.value, now);
        break;
    case UiEventKind::Idle:
        g_pending = false;
        break;
    case UiEventKind::Pending:
        g_pending       = true;
        g_pending_start = now;
        break;
    case UiEventKind::Locked:
        abort_blink();
        g_pending    = false;
        g_cue_phase  = CuePhase::None;
        g_lock_id    = event.value;
        g_lock_start = now;
        break;
    case UiEventKind::Sent: {
        abort_blink();
        g_pending          = false;
        const bool lamp_on = g_lamp || (g_cue_phase == CuePhase::Flash);
        g_cue_phase        = lamp_on ? CuePhase::Gap : CuePhase::Flash;
        g_cue_start        = now;
        break;
    }
    case UiEventKind::SetupChannel:
        start_blink(event.value, now);
        break;
    }
}

bool blink_lamp(uint32_t now) {
    const uint32_t pulse   = CHANNEL_BLINK_ON_MS + CHANNEL_BLINK_OFF_MS;
    const uint32_t body    = static_cast<uint32_t>(g_blink_n) * pulse;
    const uint32_t total   = CHANNEL_BLINK_GAP_MS + body + CHANNEL_BLINK_GAP_MS;
    const uint32_t elapsed = now - g_blink_start;
    if (elapsed >= total) {
        g_blink_active = false;
        return false;
    }
    if (elapsed < CHANNEL_BLINK_GAP_MS) {
        return false;
    }
    if (elapsed < CHANNEL_BLINK_GAP_MS + body) {
        const uint32_t into = elapsed - CHANNEL_BLINK_GAP_MS;
        return (into % pulse) < CHANNEL_BLINK_ON_MS;
    }
    return false;
}

bool resolve_stack(uint32_t now) {
    if (g_blink_active) {
        const bool on = blink_lamp(now);
        if (g_blink_active) {
            return on; // 1: channel blink
        }
    }
    if (buttons_chord_armed()) {
        const uint32_t start   = buttons_chord_start();
        const uint32_t elapsed = now - start;
        if (elapsed < CHORD_LED_DELAY_MS) {
            return false; // 2: blackout
        }
        return flash_on(now, start + CHORD_LED_DELAY_MS, CHORD_LED_FLASH_MS);
    }
    if (g_setup) {
        return true; // 3: setup solid on
    }
    if (g_lock_id != 0U) {
        return flash_on(now, g_lock_start, LOCKOUT_BLINK_MS); // 4
    }
    if (g_cue_phase == CuePhase::Gap) {
        if (now - g_cue_start >= CUE_FLASH_GAP_MS) {
            g_cue_phase = CuePhase::Flash;
            g_cue_start = now;
            return true;
        }
        return false;
    }
    if (g_cue_phase == CuePhase::Flash) {
        if (now - g_cue_start < CUE_FLASH_MS) {
            return true; // 5
        }
        g_cue_phase = CuePhase::None;
    }
    if (g_pending) {
        return flash_on(now, g_pending_start, PENDING_FLASH_MS); // 6
    }
    return false; // 7: idle off
}

} // namespace

void ui_tick(uint32_t now) {
    if (now < g_last_now) {
        reset_engine();
    }
    g_last_now = now;

    UiEvent event{};
    while (sequencer_poll_ui_event_for_engine(&event)) {
        apply_event(event, now);
    }

    if (g_lock_id != 0U && !buttons_accepted_pressed(g_lock_id)) {
        g_lock_id = 0;
    }

    g_lamp = resolve_stack(now);
#ifndef HOST_TEST
    gpio_put(LED_PANEL_GPIO, g_lamp);
    gpio_put(LED_ONBOARD_GPIO, g_lamp);
#endif
}

bool ui_lamp() {
    return g_lamp;
}

void ui_indicate_channel(uint8_t n) {
    start_blink(n, g_last_now);
}

void ui_led_init() {
#ifndef HOST_TEST
    gpio_init(LED_PANEL_GPIO);
    gpio_set_dir(LED_PANEL_GPIO, GPIO_OUT);
    gpio_set_drive_strength(LED_PANEL_GPIO, GPIO_DRIVE_STRENGTH_4MA);
    gpio_put(LED_PANEL_GPIO, false);

    gpio_init(LED_ONBOARD_GPIO);
    gpio_set_dir(LED_ONBOARD_GPIO, GPIO_OUT);
    gpio_put(LED_ONBOARD_GPIO, false);
#endif
}
