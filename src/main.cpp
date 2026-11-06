#include "main.h"

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

void loop_dispatch() {
    Command command{};
    while (sequencer_poll_command_for_midi(&command)) {
        midi_out_send(command.note);
    }
    uint8_t channel = 0;
    while (sequencer_poll_setup_commit(&channel)) {
        config_store_write_channel(channel);
        midi_out_set_channel(channel);
    }
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
        loop_dispatch();
        ui_tick(now);
        board_led_set(ui_lamp());
        sleep_us(500);
    }
}
#endif
