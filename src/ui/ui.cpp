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

enum class SwitchConfirm : uint8_t { None, Hold, Pair, SelfPair, Standalone };

uint16_t g_switch_mask       = 0;
SwitchConfirm g_sw_confirm   = SwitchConfirm::None;
bool g_sw_pending            = false;
uint8_t g_sw_hold_id         = 0;
uint8_t g_sw_button_a        = 0;
uint8_t g_sw_button_b        = 0;
uint8_t g_sw_pending_id      = 0;
uint32_t g_sw_confirm_start  = 0;
uint32_t g_sw_pending_start  = 0;
uint32_t g_sw_seen_event_seq = 0;

void reset_switch() {
    g_switch_mask       = 0;
    g_sw_confirm        = SwitchConfirm::None;
    g_sw_pending        = false;
    g_sw_hold_id        = 0;
    g_sw_button_a       = 0;
    g_sw_button_b       = 0;
    g_sw_pending_id     = 0;
    g_sw_confirm_start  = 0;
    g_sw_pending_start  = 0;
    g_sw_seen_event_seq = buttons_accepted_event_seq();
}

uint16_t switch_led_bit(uint8_t id) {
    if (id < 1U || id > SWITCH_LED_COUNT) {
        return 0U;
    }
    return static_cast<uint16_t>(uint16_t{1} << (id - 1U));
}

void clear_switch_confirmation() {
    g_sw_confirm  = SwitchConfirm::None;
    g_sw_hold_id  = 0;
    g_sw_button_a = 0;
    g_sw_button_b = 0;
}

void cancel_switch_on_accepted_event() {
    if (buttons_accepted_event_seq() == g_sw_seen_event_seq) {
        return;
    }
    if (g_sw_confirm == SwitchConfirm::None) {
        return;
    }
    clear_switch_confirmation();
}

void start_switch_confirmation(SwitchConfirm kind, uint8_t button_a, uint8_t button_b,
                               uint32_t now) {
    g_sw_pending       = false;
    g_sw_confirm       = kind;
    g_sw_button_a      = button_a;
    g_sw_button_b      = button_b;
    g_sw_hold_id       = (kind == SwitchConfirm::Hold) ? button_a : uint8_t{0};
    g_sw_confirm_start = now;
}

void apply_switch_event(const UiEvent& event, uint32_t now) {
    switch (event.kind) {
    case UiEventKind::Pending:
        g_sw_pending       = true;
        g_sw_pending_id    = event.value;
        g_sw_pending_start = now;
        break;
    case UiEventKind::Idle:
        g_sw_pending = false;
        break;
    case UiEventKind::Sent: {
        const CueOrigin origin = cue_origin_for_note(event.value);
        if (origin.kind == CueClass::Pair) {
            start_switch_confirmation(SwitchConfirm::Pair, origin.button_a, origin.button_b, now);
        } else if (origin.kind == CueClass::SelfPair) {
            start_switch_confirmation(SwitchConfirm::SelfPair, origin.button_a, 0U, now);
        } else if (origin.kind == CueClass::Standalone) {
            start_switch_confirmation(SwitchConfirm::Standalone, origin.button_a, 0U, now);
        } else {
            g_sw_pending = false;
            clear_switch_confirmation();
        }
        break;
    }
    case UiEventKind::Locked:
        start_switch_confirmation(SwitchConfirm::Hold, event.value, 0U, now);
        break;
    case UiEventKind::SetupEnter:
        g_sw_pending = false;
        break;
    case UiEventKind::SetupChannel:
    case UiEventKind::SetupExit:
        break;
    }
}

void expire_switch_confirmation(uint32_t now) {
    if (g_sw_confirm == SwitchConfirm::None) {
        return;
    }
    const bool envelope_done = (now - g_sw_confirm_start) >= LED_CONFIRM_MS;
    if (!envelope_done) {
        return;
    }
    if (g_sw_confirm == SwitchConfirm::Hold && buttons_accepted_pressed(g_sw_hold_id)) {
        return;
    }
    clear_switch_confirmation();
}

uint16_t switch_confirmation_mask(uint32_t now) {
    const uint32_t elapsed = now - g_sw_confirm_start;
    if (g_sw_confirm == SwitchConfirm::Hold || g_sw_confirm == SwitchConfirm::Pair) {
        uint16_t mask = switch_led_bit(g_sw_button_a);
        if (g_sw_confirm == SwitchConfirm::Pair) {
            mask = static_cast<uint16_t>(mask | switch_led_bit(g_sw_button_b));
        }
        return mask;
    }
    if (g_sw_confirm == SwitchConfirm::SelfPair) {
        const uint32_t notch_end = LED_SELF_PAIR_ON_MS + LED_SELF_PAIR_OFF_MS;
        if (elapsed < LED_SELF_PAIR_ON_MS || elapsed >= notch_end) {
            return switch_led_bit(g_sw_button_a);
        }
        return 0U;
    }
    if (g_sw_confirm == SwitchConfirm::Standalone) {
        const uint32_t phase = elapsed / LED_STANDALONE_MS;
        if ((phase % 2U) == 0U) {
            return switch_led_bit(g_sw_button_a);
        }
        return 0U;
    }
    return 0U;
}

uint16_t resolve_switch(uint32_t now) {
    expire_switch_confirmation(now);
    if (g_sw_confirm != SwitchConfirm::None) {
        return switch_confirmation_mask(now);
    }
    if (g_sw_pending && flash_on(now, g_sw_pending_start, LED_PENDING_FLASH_MS)) {
        return switch_led_bit(g_sw_pending_id);
    }
    return 0U;
}

} // namespace

void ui_tick(uint32_t now) {
    if (now < g_last_now) {
        reset_engine();
        reset_switch();
    }
    g_last_now = now;

    cancel_switch_on_accepted_event();

    UiEvent event{};
    while (sequencer_poll_ui_event_for_engine(&event)) {
        apply_event(event, now);
        apply_switch_event(event, now);
    }

    if (g_lock_id != 0U && !buttons_accepted_pressed(g_lock_id)) {
        g_lock_id = 0;
    }

    g_lamp              = resolve_stack(now);
    g_switch_mask       = resolve_switch(now);
    g_sw_seen_event_seq = buttons_accepted_event_seq();
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

uint16_t ui_switch_leds() {
    return g_switch_mask;
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
