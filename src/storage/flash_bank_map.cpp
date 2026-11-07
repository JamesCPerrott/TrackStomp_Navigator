#include "config.h"

// pico2_w only. The bond-bank macros are compile definitions of
// BOND_FLASH_OFFSET and BOND_FLASH_SIZE; this translation unit is what
// fails the build if those definitions drift from config.h.
static_assert(PICO_FLASH_BANK_STORAGE_OFFSET == BOND_FLASH_OFFSET);
static_assert(PICO_FLASH_BANK_TOTAL_SIZE == BOND_FLASH_SIZE);
