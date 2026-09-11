#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <cstddef>
#include <cstdint>

constexpr uint32_t CONFIG_MAGIC   = 0x4D494449U; // 'MIDI'
constexpr uint16_t CONFIG_VERSION = 1;

struct ConfigRecord {
    uint32_t magic;   // CONFIG_MAGIC
    uint16_t version; // CONFIG_VERSION
    uint8_t channel;  // 1..10
    uint8_t reserved;
    uint32_t crc32; // over all preceding bytes
};

static_assert(sizeof(ConfigRecord) == 12U);
static_assert(offsetof(ConfigRecord, crc32) == 8U);

uint8_t config_store_read_channel();
void config_store_write_channel(uint8_t channel);

#ifdef HOST_TEST
void config_store_host_erase();
void config_store_host_load(const ConfigRecord& rec);
uint32_t config_store_crc32(const void* data, std::size_t len);
uint32_t config_store_host_write_count();
#endif

#endif
