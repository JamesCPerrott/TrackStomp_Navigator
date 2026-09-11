#include "config.h"
#include "harness.h"
#include "sequencer/sequencer.h"

namespace {

void tap(uint8_t id) {
    harness_press(id);
    harness_advance(DEBOUNCE_MS);
    harness_release(id);
    harness_advance(DEBOUNCE_MS);
}

} // namespace

// 47 first: two rapid taps of 10 must not merge into one pulse (missing gap).
int main() {
    harness_reset();
    tap(10);
    REQUIRE_LAMP(true);
    tap(10);
    REQUIRE_LAMP(false);
    harness_advance(CUE_FLASH_GAP_MS - 1U);
    REQUIRE_LAMP(false);
    harness_advance(1);
    REQUIRE_LAMP(true);
    harness_advance(CUE_FLASH_MS - 1U);
    REQUIRE_LAMP(true);
    harness_advance(1);
    REQUIRE_LAMP(false);

    harness_reset();
    REQUIRE_LAMP(false);
    harness_press(10);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_LAMP(false);
    harness_release(10);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_LAMP(true);
    harness_advance(CUE_FLASH_MS - 1U);
    REQUIRE_LAMP(true);
    harness_advance(1);
    REQUIRE_LAMP(false);
    harness_advance(PENDING_FLASH_MS);
    REQUIRE_LAMP(false);

    harness_reset();
    tap(2);
    REQUIRE_LAMP(true);
    tap(7);
    REQUIRE_LAMP(false);
    harness_advance(CUE_FLASH_GAP_MS - 1U);
    REQUIRE_LAMP(false);
    harness_advance(1);
    REQUIRE_LAMP(true);
    harness_advance(CUE_FLASH_MS - 1U);
    REQUIRE_LAMP(true);
    harness_advance(1);
    REQUIRE_LAMP(false);

    harness_reset();
    tap(2);
    REQUIRE_LAMP(true);
    harness_advance(PENDING_FLASH_MS - 1U);
    REQUIRE_LAMP(true);
    harness_advance(1);
    REQUIRE_LAMP(false);
    harness_advance(PENDING_FLASH_MS - 1U);
    REQUIRE_LAMP(false);
    harness_advance(1);
    REQUIRE_LAMP(true);
    tap(7);
    REQUIRE_LAMP(false);
    harness_advance(CUE_FLASH_GAP_MS);
    REQUIRE_LAMP(true);
    harness_advance(CUE_FLASH_MS);
    REQUIRE_LAMP(false);

    harness_reset();
    harness_press(1);
    harness_advance_to(HOLD_MS);
    REQUIRE_COMMANDS(Command{27});
    REQUIRE_UI_EVENTS(UiEvent{UiEventKind::Locked, 1});
    REQUIRE_LAMP(true);
    harness_advance(LOCKOUT_BLINK_MS - 1U);
    REQUIRE_LAMP(true);
    harness_advance(1);
    REQUIRE_LAMP(false);
    harness_advance(LOCKOUT_BLINK_MS - 1U);
    REQUIRE_LAMP(false);
    harness_advance(1);
    REQUIRE_LAMP(true);
    harness_release(1);
    harness_advance(DEBOUNCE_MS);
    REQUIRE_LAMP(false);

    return 0;
}
