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

static_assert(std::size(CUE_TABLE) == 33);
static_assert(cue_table_notes_are_unique_and_complete());

#endif
