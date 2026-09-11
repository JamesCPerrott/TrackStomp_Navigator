#include "pico/unique_id.h"
#include "tusb.h"

#include <string.h>

enum { STRID_LANGID = 0, STRID_MANUFACTURER, STRID_PRODUCT, STRID_SERIAL, STRID_JACK };

enum { ITF_NUM_MIDI = 0, ITF_NUM_MIDI_STREAMING, ITF_NUM_TOTAL };

#define EPNUM_MIDI_OUT 0x01
#define EPNUM_MIDI_IN  0x81

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN)

static tusb_desc_device_t const desc_device = {.bLength            = sizeof(tusb_desc_device_t),
                                               .bDescriptorType    = TUSB_DESC_DEVICE,
                                               .bcdUSB             = 0x0200,
                                               .bDeviceClass       = 0x00,
                                               .bDeviceSubClass    = 0x00,
                                               .bDeviceProtocol    = 0x00,
                                               .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
                                               .idVendor           = 0x2E8A,
                                               .idProduct          = 0xFFFE,
                                               .bcdDevice          = 0x0100,
                                               .iManufacturer      = STRID_MANUFACTURER,
                                               .iProduct           = STRID_PRODUCT,
                                               .iSerialNumber      = STRID_SERIAL,
                                               .bNumConfigurations = 0x01};

uint8_t const* tud_descriptor_device_cb(void) {
    return (uint8_t const*) &desc_device;
}

static uint8_t const desc_fs_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_MIDI_DESC_HEAD(ITF_NUM_MIDI, 0, 1),
    TUD_MIDI_DESC_JACK_DESC(1, STRID_JACK),
    TUD_MIDI_DESC_EP(EPNUM_MIDI_OUT, 64, 1),
    TUD_MIDI_JACKID_IN_EMB(1),
    TUD_MIDI_DESC_EP(EPNUM_MIDI_IN, 64, 1),
    TUD_MIDI_JACKID_OUT_EMB(1)};

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_fs_configuration;
}

static char const* string_desc_arr[] = {
    (const char[]) {0x09, 0x04}, "TrackStomp", "TrackStomp Navigator", NULL, "Navigator Cues",
};

static uint16_t desc_str[32 + 1];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    size_t chr_count = 0;

    switch (index) {
    case STRID_LANGID:
        memcpy(&desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
        break;

    case STRID_SERIAL: {
        char serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
        pico_get_unique_board_id_string(serial, sizeof(serial));
        chr_count = 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES;
        for (size_t i = 0; i < chr_count; i++) {
            desc_str[1 + i] = (uint16_t) (unsigned char) serial[i];
        }
        break;
    }

    default: {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
            return NULL;
        }
        char const* str = string_desc_arr[index];
        if (str == NULL) {
            return NULL;
        }
        chr_count              = strlen(str);
        size_t const max_count = (sizeof(desc_str) / sizeof(desc_str[0])) - 1U;
        if (chr_count > max_count) {
            chr_count = max_count;
        }
        for (size_t i = 0; i < chr_count; i++) {
            desc_str[1 + i] = (uint16_t) (unsigned char) str[i];
        }
        break;
    }
    }

    desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2U * chr_count + 2U));
    return desc_str;
}
