#include "transport.h"

// T32 stub: always USB, which is v1 behaviour. T34 replaces the body with the
// PRD §9.4 table.
Transport transport_select(bool usb_mounted, bool ble_ready) {
    static_cast<void>(usb_mounted);
    static_cast<void>(ble_ready);
    return Transport::Usb;
}
