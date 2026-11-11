#include "midi_out.h"

#include "ble_midi.h"
#include "ble_midi_packet.h"
#include "config.h"
#include "link/link_state.h"
#include "transport.h"

#ifndef HOST_TEST
#include "tusb.h"
#endif

#include <cstddef>
#include <cstdint>

namespace {

uint8_t g_channel = DEFAULT_MIDI_CHANNEL;

#ifdef HOST_TEST
constexpr std::size_t kCaptureSize = 64;
uint8_t g_capture[kCaptureSize]{};
std::size_t g_capture_head  = 0;
std::size_t g_capture_count = 0;

// Second copy of the same bytes for the harness (the Q019 dual-drain pattern).
constexpr std::size_t kHarnessSize = 256;
uint8_t g_harness[kHarnessSize]{};
std::size_t g_harness_head  = 0;
std::size_t g_harness_count = 0;
bool g_harness_lost         = false;
bool g_usb_mounted          = true;

void capture_byte(uint8_t value) {
    if (g_harness_count < kHarnessSize) {
        const std::size_t index = (g_harness_head + g_harness_count) % kHarnessSize;
        g_harness[index]        = value;
        g_harness_count += 1U;
    } else {
        g_harness_lost = true;
    }
    if (g_capture_count >= kCaptureSize) {
        return;
    }
    const std::size_t index = (g_capture_head + g_capture_count) % kCaptureSize;
    g_capture[index]        = value;
    g_capture_count += 1U;
}
#endif

bool usb_mounted() {
#ifdef HOST_TEST
    return g_usb_mounted;
#else
    return tud_mounted();
#endif
}

void write_bytes(uint8_t const* data, uint32_t len) {
#ifdef HOST_TEST
    for (uint32_t i = 0; i < len; ++i) {
        capture_byte(data[i]);
    }
#else
    tud_midi_stream_write(0, data, len);
#endif
}

void send_usb(uint8_t note) {
    const uint8_t status = static_cast<uint8_t>(0x90U | (g_channel - 1U));
    uint8_t const msg[3] = {status, note, MIDI_VELOCITY};
    write_bytes(msg, 3U);
    if (SEND_NOTE_OFF) {
        uint8_t const off[3] = {static_cast<uint8_t>(0x80U | (g_channel - 1U)), note, 0};
        write_bytes(off, 3U);
    }
}

void send_ble(uint8_t note, uint32_t now_ms) {
    const BleMidiPacket on = ble_midi_encode_note_on(note, g_channel, now_ms);
    ble_midi_send(on.bytes, BLE_MIDI_PACKET_LENGTH);
    if (SEND_NOTE_OFF) {
        const BleMidiPacket off = ble_midi_encode_note_off(note, g_channel, now_ms);
        ble_midi_send(off.bytes, BLE_MIDI_PACKET_LENGTH);
    }
}

} // namespace

void midi_out_set_channel(uint8_t channel) {
    if (channel < 1U || channel > MAX_MIDI_CHANNEL) {
        return;
    }
    g_channel = channel;
}

uint8_t midi_out_channel() {
    return g_channel;
}

void midi_out_send(uint8_t note, uint32_t now_ms) {
    const Transport chosen = transport_select(usb_mounted(), link_ready());
    if (chosen == Transport::Usb) {
        send_usb(note);
        return;
    }
    if (chosen == Transport::Ble) {
        send_ble(note, now_ms);
    }
}

#ifdef HOST_TEST
void midi_out_reset() {
    g_channel       = DEFAULT_MIDI_CHANNEL;
    g_capture_head  = 0;
    g_capture_count = 0;
    g_harness_head  = 0;
    g_harness_count = 0;
    g_harness_lost  = false;
    g_usb_mounted   = true;
}

bool midi_out_poll_byte(uint8_t* out) {
    if (out == nullptr || g_capture_count == 0U) {
        return false;
    }
    *out           = g_capture[g_capture_head];
    g_capture_head = (g_capture_head + 1U) % kCaptureSize;
    g_capture_count -= 1U;
    return true;
}

void midi_out_set_usb_mounted(bool mounted) {
    g_usb_mounted = mounted;
}

bool midi_out_poll_byte_for_harness(uint8_t* out) {
    if (out == nullptr || g_harness_count == 0U) {
        return false;
    }
    *out           = g_harness[g_harness_head];
    g_harness_head = (g_harness_head + 1U) % kHarnessSize;
    g_harness_count -= 1U;
    return true;
}

bool midi_out_harness_bytes_lost() {
    return g_harness_lost;
}
#endif
