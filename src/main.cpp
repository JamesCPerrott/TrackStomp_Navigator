#include "config.h"
#include "input/buttons.h"
#include "midi/midi_out.h"
#include "storage/config_store.h"
#include "ui/ui.h"

#include <cstdint>

int main() {
    buttons_init();
    ui_led_init();
    const uint8_t channel = config_store_read_channel();
    midi_out_set_channel(channel);
    ui_indicate_channel(channel);
    static_cast<void>(CUE_TABLE);
    for (;;) {
    }
}
