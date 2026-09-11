#include "config.h"
#include "harness.h"
#include "storage/config_store.h"

#include <cstddef>

namespace {

ConfigRecord make_valid(uint8_t channel) {
    ConfigRecord rec{};
    rec.magic    = CONFIG_MAGIC;
    rec.version  = CONFIG_VERSION;
    rec.channel  = channel;
    rec.reserved = 0;
    rec.crc32    = config_store_crc32(&rec, offsetof(ConfigRecord, crc32));
    return rec;
}

} // namespace

int main() {
    config_store_host_erase();
    REQUIRE(config_store_read_channel() == DEFAULT_MIDI_CHANNEL);
    REQUIRE(config_store_host_write_count() == 0U);

    ConfigRecord bad_crc = make_valid(8);
    bad_crc.crc32 ^= 1U;
    config_store_host_load(bad_crc);
    REQUIRE(config_store_read_channel() == DEFAULT_MIDI_CHANNEL);
    REQUIRE(config_store_host_write_count() == 0U);

    ConfigRecord bad_magic = make_valid(8);
    bad_magic.magic        = 0;
    bad_magic.crc32        = config_store_crc32(&bad_magic, offsetof(ConfigRecord, crc32));
    config_store_host_load(bad_magic);
    REQUIRE(config_store_read_channel() == DEFAULT_MIDI_CHANNEL);
    REQUIRE(config_store_host_write_count() == 0U);

    config_store_host_load(make_valid(0));
    REQUIRE(config_store_read_channel() == DEFAULT_MIDI_CHANNEL);
    config_store_host_load(make_valid(11));
    REQUIRE(config_store_read_channel() == DEFAULT_MIDI_CHANNEL);
    REQUIRE(config_store_host_write_count() == 0U);

    ConfigRecord unknown_version = make_valid(5);
    unknown_version.version      = 2;
    unknown_version.crc32 = config_store_crc32(&unknown_version, offsetof(ConfigRecord, crc32));
    config_store_host_load(unknown_version);
    REQUIRE(config_store_read_channel() == DEFAULT_MIDI_CHANNEL);

    config_store_host_load(make_valid(8));
    REQUIRE(config_store_read_channel() == 8U);
    REQUIRE(config_store_host_write_count() == 0U);

    config_store_host_erase();
    config_store_write_channel(DEFAULT_MIDI_CHANNEL);
    REQUIRE(config_store_host_write_count() == 0U);

    config_store_write_channel(8);
    REQUIRE(config_store_read_channel() == 8U);
    REQUIRE(config_store_host_write_count() == 1U);

    config_store_write_channel(8);
    REQUIRE(config_store_host_write_count() == 1U);

    config_store_write_channel(0);
    config_store_write_channel(11);
    REQUIRE(config_store_read_channel() == 8U);
    REQUIRE(config_store_host_write_count() == 1U);

    config_store_write_channel(1);
    REQUIRE(config_store_read_channel() == 1U);
    REQUIRE(config_store_host_write_count() == 2U);

    return 0;
}
