#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <cstdint>

// None is 0 so a zero-initialised value never means "send on USB".
enum class Transport : uint8_t { None, Usb, Ble };

// PRD §9.4, evaluated per cue. Pure.
Transport transport_select(bool usb_mounted, bool ble_ready);

#endif
