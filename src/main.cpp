#include "config.h"
#include "input/buttons.h"
#include "ui/ui.h"

int main() {
    buttons_init();
    ui_indicate_channel(DEFAULT_MIDI_CHANNEL);
    static_cast<void>(CUE_TABLE);
    for (;;) {
    }
}
