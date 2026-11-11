#include "transport.h"

// PRD §9.4. USB wins. A cue is never sent on both, and neither ready drops it.
Transport transport_select(bool usb_mounted, bool ble_ready) {
    if (usb_mounted) {
        return Transport::Usb;
    }
    if (ble_ready) {
        return Transport::Ble;
    }
    return Transport::None;
}
