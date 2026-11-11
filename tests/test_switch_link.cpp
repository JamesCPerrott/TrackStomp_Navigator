#include "config.h"
#include "harness.h"
#include "link/link_state.h"
#include "ui/ui.h"

#include <cstdint>

namespace {

constexpr uint16_t kPairLeds =
    static_cast<uint16_t>(SWITCH_LED(PAIR_CHORD_BUTTON_A) | SWITCH_LED(PAIR_CHORD_BUTTON_B));

void expect_mask(uint16_t mask) {
    REQUIRE_SWITCH_LEDS(mask);
    REQUIRE(harness_switch_led_count() <= 2U);
}

void expect_trace_ceiling() {
    for (const uint16_t sample : harness_switch_trace()) {
        uint8_t count = 0;
        for (uint8_t bit = 0; bit < SWITCH_LED_COUNT; ++bit) {
            const uint16_t flag = static_cast<uint16_t>(uint16_t{1} << bit);
            if ((sample & flag) != 0U) {
                count = static_cast<uint8_t>(count + 1U);
            }
        }
        REQUIRE(count <= 2U);
    }
}

void tap(uint8_t id) {
    harness_press(id);
    harness_advance(DEBOUNCE_MS);
    harness_release(id);
    harness_advance(DEBOUNCE_MS);
}

uint32_t enter_setup() {
    const uint32_t start = harness_now_ms();
    harness_press(CHORD_BUTTON_A);
    harness_press(CHORD_BUTTON_B);
    harness_advance_to(start + CHORD_HOLD_MS);
    const uint32_t opened = harness_now_ms();
    harness_release(CHORD_BUTTON_A);
    harness_release(CHORD_BUTTON_B);
    harness_advance(DEBOUNCE_MS);
    return opened;
}

uint32_t open_pairing_window() {
    const uint32_t pair_at = harness_now_ms();
    harness_press(PAIR_CHORD_BUTTON_A);
    harness_press(PAIR_CHORD_BUTTON_B);
    harness_advance_to(pair_at + PAIR_CHORD_HOLD_MS);
    return harness_now_ms();
}

void release_pair() {
    harness_release(PAIR_CHORD_BUTTON_A);
    harness_release(PAIR_CHORD_BUTTON_B);
    harness_advance(DEBOUNCE_MS);
}

} // namespace

// 100 and 95 first. Every sample stays at one indication, at most two LEDs.
int main() {
    // 95. Ready during the boot channel: that LED runs to 2000 ms, then 2 and 4
    // for one second, then off.
    harness_reset();
    ui_indicate_channel(DEFAULT_MIDI_CHANNEL);
    harness_advance_to(1000U);
    harness_link_event(LinkEventKind::Ready);
    REQUIRE(link_boot_connected());
    expect_mask(SWITCH_LED(DEFAULT_MIDI_CHANNEL));
    harness_advance_to(LED_BOOT_CHANNEL_MS - 1U);
    expect_mask(SWITCH_LED(DEFAULT_MIDI_CHANNEL));
    harness_advance_to(LED_BOOT_CHANNEL_MS);
    expect_mask(kPairLeds);
    harness_advance_to(LED_BOOT_CHANNEL_MS + LED_LINK_CONNECTED_MS - 1U);
    expect_mask(kPairLeds);
    harness_advance_to(LED_BOOT_CHANNEL_MS + LED_LINK_CONNECTED_MS);
    expect_mask(0U);
    expect_trace_ceiling();

    // A press before the flash starts drops it. The boot LED is still up at
    // the tick before the press is accepted.
    harness_reset();
    ui_indicate_channel(DEFAULT_MIDI_CHANNEL);
    harness_advance_to(1000U);
    harness_link_event(LinkEventKind::Ready);
    harness_advance_to(1500U);
    harness_press(6);
    harness_advance(DEBOUNCE_MS);
    harness_release(6);
    harness_advance(DEBOUNCE_MS - 1U);
    expect_mask(SWITCH_LED(DEFAULT_MIDI_CHANNEL));
    harness_advance(1U);
    expect_mask(0U);
    harness_advance_to(LED_BOOT_CHANNEL_MS);
    expect_mask(0U);
    harness_advance_to(LED_BOOT_CHANNEL_MS + LED_LINK_CONNECTED_MS);
    expect_mask(0U);
    REQUIRE(link_boot_connected());
    expect_trace_ceiling();

    // A press during the flash extinguishes it on the accepted tick.
    harness_reset();
    ui_indicate_channel(DEFAULT_MIDI_CHANNEL);
    harness_advance_to(1000U);
    harness_link_event(LinkEventKind::Ready);
    const uint32_t during = LED_BOOT_CHANNEL_MS + 500U;
    harness_advance_to(during);
    expect_mask(kPairLeds);
    harness_press(6);
    harness_advance(DEBOUNCE_MS);
    harness_release(6);
    harness_advance(DEBOUNCE_MS - 1U);
    expect_mask(kPairLeds);
    harness_advance(1U);
    expect_mask(0U);
    harness_advance_to(LED_BOOT_CHANNEL_MS + LED_LINK_CONNECTED_MS);
    expect_mask(0U);
    expect_trace_ceiling();

    // 96. Ready at 31 s does not latch and lights nothing. The same after a
    // flash has already played: a later ready is not a second flash.
    harness_reset();
    ui_indicate_channel(DEFAULT_MIDI_CHANNEL);
    harness_advance_to(31000U);
    harness_link_event(LinkEventKind::Ready);
    REQUIRE(!link_boot_connected());
    expect_mask(0U);
    harness_advance(LED_LINK_CONNECTED_MS);
    expect_mask(0U);
    expect_trace_ceiling();

    // Exactly the window length is outside. One millisecond earlier latches,
    // and the flash starts then because the boot channel has already ended.
    harness_reset();
    ui_indicate_channel(8);
    harness_advance_to(BOOT_CONNECT_WINDOW_MS);
    harness_link_event(LinkEventKind::Ready);
    REQUIRE(!link_boot_connected());
    expect_mask(0U);
    expect_trace_ceiling();

    harness_reset();
    ui_indicate_channel(8);
    harness_advance_to(BOOT_CONNECT_WINDOW_MS - 1U);
    harness_link_event(LinkEventKind::Ready);
    REQUIRE(link_boot_connected());
    expect_mask(kPairLeds);
    harness_advance_to((BOOT_CONNECT_WINDOW_MS - 1U) + LED_LINK_CONNECTED_MS - 1U);
    expect_mask(kPairLeds);
    harness_advance(1U);
    expect_mask(0U);
    expect_trace_ceiling();

    harness_reset();
    ui_indicate_channel(DEFAULT_MIDI_CHANNEL);
    harness_advance_to(1000U);
    harness_link_event(LinkEventKind::Ready);
    harness_advance_to(LED_BOOT_CHANNEL_MS + LED_LINK_CONNECTED_MS);
    expect_mask(0U);
    harness_link_event(LinkEventKind::Lost);
    harness_advance_to(5000U);
    harness_link_event(LinkEventKind::Ready);
    REQUIRE(link_boot_connected());
    expect_mask(0U);
    harness_advance(LED_LINK_CONNECTED_MS);
    expect_mask(0U);
    expect_trace_ceiling();

    // 89. Advertising 500/500 from the on phase. A tap does not restart it
    // and does not reveal the channel LED.
    harness_reset();
    enter_setup();
    const uint32_t window = open_pairing_window();
    expect_mask(kPairLeds);
    tap(7);
    harness_advance_to(window + LED_ADVERTISING_MS - 1U);
    expect_mask(kPairLeds);
    harness_advance(1U);
    REQUIRE(harness_now_ms() == window + LED_ADVERTISING_MS);
    expect_mask(0U);
    harness_advance_to(window + (LED_ADVERTISING_MS * 2U) - 1U);
    expect_mask(0U);
    harness_advance(1U);
    expect_mask(kPairLeds);
    expect_trace_ceiling();

    // Bonding is 100/100 from its own on phase. Failure returns to advertising
    // from the on phase and does not keep the fast clock.
    const uint32_t bond_at = harness_now_ms();
    harness_link_event(LinkEventKind::PairingStarted);
    expect_mask(kPairLeds);
    harness_advance_to(bond_at + LED_BONDING_MS - 1U);
    expect_mask(kPairLeds);
    harness_advance(1U);
    REQUIRE(harness_now_ms() == bond_at + LED_BONDING_MS);
    expect_mask(0U);
    harness_advance_to(bond_at + (LED_BONDING_MS * 2U) - 1U);
    expect_mask(0U);
    harness_advance(1U);
    expect_mask(kPairLeds);
    const uint32_t fail_at = harness_now_ms();
    harness_link_event(LinkEventKind::PairingFailed);
    expect_mask(kPairLeds);
    harness_advance_to(fail_at + LED_ADVERTISING_MS - 1U);
    expect_mask(kPairLeds);
    harness_advance(1U);
    REQUIRE(harness_now_ms() == fail_at + LED_ADVERTISING_MS);
    expect_mask(0U);
    expect_trace_ceiling();

    // Bond OK is solid for 2000 ms. The next tick is the setup channel, from
    // its on phase, which the pre-window clock would have had off.
    harness_reset();
    const uint32_t setup_ok = enter_setup();
    open_pairing_window();
    release_pair();
    harness_advance(LED_SETUP_BLINK_MS);
    const uint32_t ok_at = harness_now_ms();
    harness_link_event(LinkEventKind::PairingSucceeded);
    expect_mask(kPairLeds);
    harness_advance_to(ok_at + LED_BOND_OK_MS - 1U);
    expect_mask(kPairLeds);
    harness_advance(1U);
    REQUIRE(harness_now_ms() == ok_at + LED_BOND_OK_MS);
    const uint32_t resumed = harness_now_ms();
    REQUIRE(((resumed - setup_ok) / LED_SETUP_BLINK_MS) % 2U == 1U);
    expect_mask(SWITCH_LED(DEFAULT_MIDI_CHANNEL));
    harness_advance_to(resumed + LED_SETUP_BLINK_MS - 1U);
    expect_mask(SWITCH_LED(DEFAULT_MIDI_CHANNEL));
    harness_advance(1U);
    expect_mask(0U);
    expect_trace_ceiling();

    // 6+9 during the bond-OK flash: all ten off. The flash does not finish.
    harness_reset();
    enter_setup();
    open_pairing_window();
    release_pair();
    harness_link_event(LinkEventKind::PairingSucceeded);
    expect_mask(kPairLeds);
    harness_press(CHORD_BUTTON_A);
    harness_press(CHORD_BUTTON_B);
    harness_advance(DEBOUNCE_MS);
    expect_mask(kPairLeds);
    harness_release(CHORD_BUTTON_A);
    harness_release(CHORD_BUTTON_B);
    harness_advance(DEBOUNCE_MS - 1U);
    expect_mask(kPairLeds);
    harness_advance(1U);
    expect_mask(0U);
    harness_advance(LED_BOND_OK_MS);
    expect_mask(0U);
    expect_trace_ceiling();

    // A second 2+4 restarts advertising from the on phase. Without the
    // restart this tick would be the off half of the first window.
    harness_reset();
    enter_setup();
    const uint32_t first = open_pairing_window();
    release_pair();
    harness_advance_to(first + LED_ADVERTISING_MS);
    expect_mask(0U);
    const uint32_t second_press = harness_now_ms();
    harness_press(PAIR_CHORD_BUTTON_A);
    harness_press(PAIR_CHORD_BUTTON_B);
    harness_advance_to(second_press + PAIR_CHORD_HOLD_MS);
    const uint32_t second = harness_now_ms();
    REQUIRE(((second - first) / LED_ADVERTISING_MS) % 2U == 1U);
    expect_mask(kPairLeds);
    harness_advance_to(second + LED_ADVERTISING_MS - 1U);
    expect_mask(kPairLeds);
    harness_advance(1U);
    expect_mask(0U);
    expect_trace_ceiling();

    // Window timeout resumes the setup channel from its on phase. The clock
    // from setup entry would have been in the off half.
    harness_reset();
    const uint32_t setup_to = enter_setup();
    harness_advance(LED_SETUP_BLINK_MS);
    const uint32_t timed = open_pairing_window();
    release_pair();
    harness_advance_to(timed + PAIRING_WINDOW_MS - 1U);
    const uint32_t age = (harness_now_ms() - timed) / LED_ADVERTISING_MS;
    if ((age % 2U) == 0U) {
        expect_mask(kPairLeds);
    } else {
        expect_mask(0U);
    }
    harness_advance(1U);
    REQUIRE(harness_now_ms() == timed + PAIRING_WINDOW_MS);
    REQUIRE(((harness_now_ms() - setup_to) / LED_SETUP_BLINK_MS) % 2U == 1U);
    expect_mask(SWITCH_LED(DEFAULT_MIDI_CHANNEL));
    expect_trace_ceiling();
    return 0;
}
