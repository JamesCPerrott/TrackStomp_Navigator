#include "main.h"

#include "midi/midi_out.h"
#include "sequencer/sequencer.h"
#include "storage/config_store.h"

#ifndef HOST_TEST
#include "config.h"
#include "input/buttons.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "ui/ui.h"
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
    const tusb_rhport_init_t dev_init = {TUSB_ROLE_DEVICE, TUSB_SPEED_AUTO};
    tusb_init(BOARD_TUD_RHPORT, &dev_init);
    const uint8_t channel = config_store_read_channel();
    midi_out_set_channel(channel);
    ui_indicate_channel(channel);
#ifdef DEBUG_UART
    stdio_init_all();
#endif
    static_cast<void>(CUE_TABLE);
    for (;;) {
        tud_task();
        const uint32_t now = to_ms_since_boot(get_absolute_time());
        buttons_scan(now);
        sequencer_tick(now);
        loop_dispatch();
        ui_tick(now);
        sleep_us(500);
    }
}
#endif
