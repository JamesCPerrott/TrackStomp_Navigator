#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>
#include <iterator>

constexpr uint32_t SCAN_INTERVAL_MS    = 1;
constexpr uint32_t DEBOUNCE_MS         = 20;
constexpr uint32_t HOLD_MS             = 2000;
constexpr uint32_t CHORD_HOLD_MS       = 5000;
constexpr uint32_t SEQUENCE_TIMEOUT_MS = 5000;
constexpr uint32_t SETUP_TIMEOUT_MS    = 30000;
constexpr uint8_t DEFAULT_MIDI_CHANNEL = 1;  // 1-based; wire value is n-1
constexpr uint8_t MAX_MIDI_CHANNEL     = 10; // limited by button count
constexpr uint8_t MIDI_VELOCITY        = 1;
constexpr bool SEND_NOTE_OFF           = false; // Playback cues are fire-and-forget
constexpr uint8_t BUTTON_GPIO_BASE     = 6;
constexpr uint8_t BUTTON_COUNT         = 10;
constexpr uint8_t CHORD_BUTTON_A       = 6;
constexpr uint8_t CHORD_BUTTON_B       = 9;
constexpr uint8_t LED_PANEL_GPIO       = 16; // NPN switch, high = on
constexpr uint8_t LED_ONBOARD_GPIO     = 25; // Pico 2 onboard LED mirror

// LED timing
constexpr uint32_t CHORD_LED_DELAY_MS   = 500; // silent window at chord start
constexpr uint32_t CHORD_LED_FLASH_MS   = 125; // on and off; 250 ms period
constexpr uint32_t LOCKOUT_BLINK_MS     = 50;  // on and off; 100 ms period
constexpr uint32_t PENDING_FLASH_MS     = 500; // on and off; 1000 ms period
constexpr uint32_t CUE_FLASH_MS         = 150;
constexpr uint32_t CUE_FLASH_GAP_MS     = 100; // forced off before a cue flash
constexpr uint32_t CHANNEL_BLINK_GAP_MS = 400;
constexpr uint32_t CHANNEL_BLINK_ON_MS  = 150;
constexpr uint32_t CHANNEL_BLINK_OFF_MS = 150;

// Per-switch LED array (§11.5). Bits 0–9 are LEDs 1–10. Index 0 is button 1.
constexpr uint8_t SWITCH_LED_COUNT = 10;

// Not a contiguous range. Do not derive pin n as SWITCH_LED_GPIO[0] - n.
constexpr uint8_t SWITCH_LED_GPIO[SWITCH_LED_COUNT] = {5, 4, 3, 2, 22, 21, 20, 19, 18, 17};

// Brightness trim only (§11.5.2). Uniform full scale; the driver does not PWM.
constexpr uint8_t SWITCH_LED_DUTY[SWITCH_LED_COUNT] = {255, 255, 255, 255, 255,
                                                       255, 255, 255, 255, 255};

constexpr uint32_t switch_led_pin_mask() {
    uint32_t mask = 0;
    for (uint8_t pin : SWITCH_LED_GPIO) {
        mask |= uint32_t{1} << pin;
    }
    return mask;
}

constexpr uint32_t switch_led_gpio_value(uint16_t leds) {
    uint32_t value = 0;
    for (uint8_t index = 0; index < SWITCH_LED_COUNT; ++index) {
        const uint16_t bit = static_cast<uint16_t>(uint16_t{1} << index);
        if ((leds & bit) != 0U) {
            value |= uint32_t{1} << SWITCH_LED_GPIO[index];
        }
    }
    return value;
}

constexpr bool switch_led_duty_is_full_scale() {
    bool full = true;
    for (uint8_t duty : SWITCH_LED_DUTY) {
        if (duty != 255U) {
            full = false;
        }
    }
    return full;
}

static_assert(SWITCH_LED_GPIO[4] != static_cast<uint8_t>(SWITCH_LED_GPIO[0] - 4U));
static_assert(switch_led_pin_mask() ==
              ((uint32_t{1} << 5U) | (uint32_t{1} << 4U) | (uint32_t{1} << 3U) |
               (uint32_t{1} << 2U) | (uint32_t{1} << 22U) | (uint32_t{1} << 21U) |
               (uint32_t{1} << 20U) | (uint32_t{1} << 19U) | (uint32_t{1} << 18U) |
               (uint32_t{1} << 17U)));
static_assert(switch_led_gpio_value(uint16_t{1} << 0U) == (uint32_t{1} << 5U));
static_assert(switch_led_gpio_value(uint16_t{1} << 4U) == (uint32_t{1} << 22U));
static_assert(switch_led_gpio_value(uint16_t{1} << 9U) == (uint32_t{1} << 17U));
static_assert(switch_led_gpio_value(static_cast<uint16_t>((uint16_t{1} << 5U) |
                                                          (uint16_t{1} << 8U))) ==
              ((uint32_t{1} << 21U) | (uint32_t{1} << 18U)));
static_assert(switch_led_gpio_value(0U) == 0U);
static_assert(switch_led_duty_is_full_scale());

// Per-switch indication timing (§11.5.4).
constexpr uint32_t LED_PENDING_FLASH_MS = 125; // on and off; 250 ms period
constexpr uint32_t LED_CONFIRM_MS       = 5000;
constexpr uint32_t LED_SELF_PAIR_ON_MS  = 2000;
constexpr uint32_t LED_SELF_PAIR_OFF_MS = 1000;
constexpr uint32_t LED_STANDALONE_MS    = 1000; // on and off; five phases
constexpr uint32_t LED_SETUP_BLINK_MS   = 1000; // on and off; 2000 ms period
constexpr uint32_t LED_BOOT_CHANNEL_MS  = 2000;

static_assert((LED_SELF_PAIR_ON_MS + LED_SELF_PAIR_OFF_MS + LED_SELF_PAIR_ON_MS) == LED_CONFIRM_MS);
static_assert((LED_STANDALONE_MS * 5U) == LED_CONFIRM_MS);

enum class CueTrigger : uint8_t { Tap, SelfPair, PrefixSuffix, Hold };

struct Cue {
    uint8_t note;
    CueTrigger trigger;
    uint8_t button_a; // prefix, or the sole tap/hold button
    uint8_t button_b; // suffix; 0 when unused (tap/hold). 0 is not a valid button.
};

inline constexpr Cue CUE_TABLE[] = {
    {0, CueTrigger::PrefixSuffix, 1, 6},  {1, CueTrigger::PrefixSuffix, 1, 7},
    {2, CueTrigger::PrefixSuffix, 1, 8},  {3, CueTrigger::PrefixSuffix, 1, 9},
    {4, CueTrigger::PrefixSuffix, 2, 6},  {5, CueTrigger::PrefixSuffix, 2, 7},
    {6, CueTrigger::PrefixSuffix, 2, 8},  {7, CueTrigger::PrefixSuffix, 2, 9},
    {8, CueTrigger::PrefixSuffix, 3, 6},  {9, CueTrigger::PrefixSuffix, 3, 7},
    {10, CueTrigger::PrefixSuffix, 3, 8}, {11, CueTrigger::PrefixSuffix, 3, 9},
    {12, CueTrigger::PrefixSuffix, 4, 6}, {13, CueTrigger::PrefixSuffix, 4, 7},
    {14, CueTrigger::PrefixSuffix, 4, 8}, {15, CueTrigger::PrefixSuffix, 4, 9},
    {16, CueTrigger::PrefixSuffix, 5, 6}, {17, CueTrigger::PrefixSuffix, 5, 7},
    {18, CueTrigger::PrefixSuffix, 5, 8}, {19, CueTrigger::PrefixSuffix, 5, 9},
    {20, CueTrigger::Tap, 10, 0},         {21, CueTrigger::Hold, 10, 0},
    {22, CueTrigger::SelfPair, 1, 1},     {23, CueTrigger::SelfPair, 2, 2},
    {24, CueTrigger::SelfPair, 3, 3},     {25, CueTrigger::SelfPair, 4, 4},
    {26, CueTrigger::SelfPair, 5, 5},     {27, CueTrigger::Hold, 1, 0},
    {28, CueTrigger::Hold, 2, 0},         {29, CueTrigger::Hold, 3, 0},
    {30, CueTrigger::Hold, 4, 0},         {31, CueTrigger::Hold, 5, 0},
    {32, CueTrigger::Hold, 8, 0},
};

constexpr bool cue_table_notes_are_unique_and_complete() {
    uint64_t mask = 0;
    for (const Cue& cue : CUE_TABLE) {
        if (cue.note >= 33U) {
            return false;
        }
        const uint64_t bit = uint64_t{1} << cue.note;
        if ((mask & bit) != 0U) {
            return false;
        }
        mask |= bit;
    }
    const uint64_t expected = (uint64_t{1} << 33U) - 1U;
    return mask == expected;
}

enum class CueClass : uint8_t { Invalid, Pair, SelfPair, Standalone, Hold };

struct CueOrigin {
    CueClass kind;
    uint8_t button_a; // prefix, or the sole button; 0xFF when kind is Invalid
    uint8_t button_b; // suffix; equals button_a for a self-pair; 0 when unused
};

constexpr CueClass cue_class_from_entry(const Cue& cue) {
    if (cue.trigger == CueTrigger::Hold) {
        return CueClass::Hold;
    }
    if (cue.button_b == 0U) {
        return CueClass::Standalone;
    }
    if (cue.button_a == cue.button_b) {
        return CueClass::SelfPair;
    }
    return CueClass::Pair;
}

constexpr CueOrigin cue_origin_for_note(uint8_t note) {
    for (const Cue& cue : CUE_TABLE) {
        if (cue.note == note) {
            return CueOrigin{cue_class_from_entry(cue), cue.button_a, cue.button_b};
        }
    }
    return CueOrigin{CueClass::Invalid, uint8_t{0xFF}, uint8_t{0xFF}};
}

constexpr bool cue_table_origins_round_trip() {
    bool matches = true;
    for (const Cue& cue : CUE_TABLE) {
        const CueOrigin origin = cue_origin_for_note(cue.note);
        if (origin.kind != cue_class_from_entry(cue)) {
            matches = false;
        }
        if (origin.button_a != cue.button_a) {
            matches = false;
        }
        if (origin.button_b != cue.button_b) {
            matches = false;
        }
    }
    return matches;
}

static_assert(std::size(CUE_TABLE) == 33);
static_assert(cue_table_notes_are_unique_and_complete());
static_assert(cue_table_origins_round_trip());

#endif
