#include "config_store.h"

#include "config.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#ifndef HOST_TEST
#include "hardware/flash.h"
#include "hardware/sync.h"
#endif

namespace {

// CRC-32/ISO-HDLC (poly 0xEDB88320, init/xorout 0xFFFFFFFF). PRD §10.1 does
// not name a polynomial; this is the usual IEEE CRC-32 (Q015).
uint32_t crc32(const void* data, std::size_t len) {
    auto const* bytes = static_cast<uint8_t const*>(data);
    uint32_t crc      = 0xFFFFFFFFU;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= bytes[i];
        for (uint32_t bit = 0; bit < 8U; ++bit) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1U) ^ 0xEDB88320U;
            } else {
                crc >>= 1U;
            }
        }
    }
    return ~crc;
}

bool record_valid(const ConfigRecord& rec) {
    if (rec.magic != CONFIG_MAGIC) {
        return false;
    }
    if (rec.version != CONFIG_VERSION) {
        return false;
    }
    if (rec.channel < DEFAULT_MIDI_CHANNEL || rec.channel > MAX_MIDI_CHANNEL) {
        return false;
    }
    const uint32_t expected = crc32(&rec, offsetof(ConfigRecord, crc32));
    return expected == rec.crc32;
}

#ifdef HOST_TEST
ConfigRecord g_sector{0xFFFFFFFFU, 0xFFFF, 0xFF, 0xFF, 0xFFFFFFFFU};
uint32_t g_write_count = 0;

void fill_erased() {
    std::memset(&g_sector, 0xFF, sizeof(g_sector));
}
#else
void __no_inline_not_in_flash_func(commit_flash)(const uint8_t* page) {
    const uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(PICO_FLASH_SIZE_BYTES, FLASH_SECTOR_SIZE);
    flash_range_program(PICO_FLASH_SIZE_BYTES, page, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
}
#endif

} // namespace

uint8_t config_store_read_channel() {
    ConfigRecord rec{};
#ifdef HOST_TEST
    rec = g_sector;
#else
    // pico_override_flash_size shrinks PICO_FLASH_SIZE_BYTES by one sector, so
    // that value is the offset of the reserved last 4 KB (Q016).
    std::memcpy(&rec, reinterpret_cast<const void*>(XIP_BASE + PICO_FLASH_SIZE_BYTES), sizeof(rec));
#endif
    if (!record_valid(rec)) {
        return DEFAULT_MIDI_CHANNEL;
    }
    return rec.channel;
}

void config_store_write_channel(uint8_t channel) {
    if (channel < DEFAULT_MIDI_CHANNEL || channel > MAX_MIDI_CHANNEL) {
        return;
    }
    if (channel == config_store_read_channel()) {
        return;
    }
    ConfigRecord rec{};
    rec.magic    = CONFIG_MAGIC;
    rec.version  = CONFIG_VERSION;
    rec.channel  = channel;
    rec.reserved = 0;
    rec.crc32    = crc32(&rec, offsetof(ConfigRecord, crc32));
#ifdef HOST_TEST
    g_sector = rec;
    g_write_count += 1U;
#else
    uint8_t page[FLASH_PAGE_SIZE];
    std::memset(page, 0xFF, sizeof(page));
    std::memcpy(page, &rec, sizeof(rec));
    commit_flash(page);
#endif
}

#ifdef HOST_TEST
void config_store_host_erase() {
    fill_erased();
}

void config_store_host_load(const ConfigRecord& rec) {
    g_sector = rec;
}

uint32_t config_store_crc32(const void* data, std::size_t len) {
    return crc32(data, len);
}

uint32_t config_store_host_write_count() {
    return g_write_count;
}
#endif
