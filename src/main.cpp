#include "main.h"

#include "link/link_state.h"
#include "midi/ble_midi.h"
#include "midi/midi_out.h"
#include "sequencer/sequencer.h"
#include "storage/config_store.h"

#ifndef HOST_TEST
#include "board/board_led.h"
#include "config.h"
#include "input/buttons.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "ui/ui.h"
#if defined(RASPBERRYPI_PICO2_W)
#include "pico/cyw43_arch.h"
#endif
#endif

#include <cstdint>

void loop_dispatch(uint32_t now_ms) {
    Command command{};
    while (sequencer_poll_command_for_midi(&command)) {
        midi_out_send(command.note, now_ms);
    }
    uint8_t channel = 0;
    while (sequencer_poll_setup_commit(&channel)) {
        config_store_write_channel(channel);
        midi_out_set_channel(channel);
    }
    // Link half (PRD §12.2). The sequencer's request is applied, then the
    // transport drain runs, in the same pass.
    LinkRequest from_sequencer{};
    while (sequencer_poll_link_request(&from_sequencer)) {
        link_on_request(from_sequencer);
    }
    LinkRequest request{};
    while (link_poll_request(&request)) {
        ble_midi_request(request);
    }
    LinkEvent event{};
    while (ble_midi_poll_event(&event)) {
        link_on_event(event);
    }
    // Same pass as the close, so inactivity restarts at this millisecond.
    sequencer_observe_link(now_ms);
}

#ifndef HOST_TEST
int main() {
    buttons_init();
    ui_led_init();
#if !defined(RASPBERRYPI_PICO2_W)
    board_led_init(true);
#endif
    const tusb_rhport_init_t dev_init = {TUSB_ROLE_DEVICE, TUSB_SPEED_AUTO};
    tusb_init(BOARD_TUD_RHPORT, &dev_init);
    const uint8_t channel = config_store_read_channel();
    midi_out_set_channel(channel);
    ui_indicate_channel(channel);
#ifdef DEBUG_UART
    stdio_init_all();
#endif
#if defined(RASPBERRYPI_PICO2_W)
    // Q033: radio init failure degrades to USB-only. The scan loop still runs.
    // cyw43_arch_init() is before the first onboard-LED write.
    const int radio_status = cyw43_arch_init();
    const bool radio_ok    = (radio_status == 0);
#ifdef DEBUG_UART
    if (!radio_ok) {
        printf("cyw43_arch_init failed: %d\n", radio_status);
    }
#endif
    board_led_init(radio_ok);
    // btstack_cyw43_init already ran inside cyw43_arch_init. Q033: a failed
    // radio init never calls this, so BeginPairing answers NoRadio.
    if (radio_ok) {
        ble_midi_init();
    }
#endif
    static_cast<void>(CUE_TABLE);
    for (;;) {
        tud_task();
#if defined(RASPBERRYPI_PICO2_W)
        if (radio_ok) {
            cyw43_arch_poll();
        }
#endif
        const uint32_t now = to_ms_since_boot(get_absolute_time());
        buttons_scan(now);
        sequencer_tick(now);
        link_tick(now);
        loop_dispatch(now);
        ui_tick(now);
        board_led_set(ui_lamp());
        sleep_us(500);
    }
}
#endif
