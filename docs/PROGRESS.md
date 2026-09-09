LOOP-STATUS: NOT STARTED

# Progress log

Append-only. Never rewrite or delete a prior entry — the record of what was tried and abandoned is the point. If an earlier entry turns out to be wrong, add a new entry saying so.

The loop reads the most recent entry on each wake to find where it left off. **Human-completed tasks are logged here in the same format** — T01, T02, and T03 are done interactively before the loop starts, and the loop will refuse to run until it sees them here and in the task list.

`LOOP-STATUS` on line 1 is the only mutable content in this file. Values: `NOT STARTED`, `RUNNING`, `HALTED — <reason>`, `COMPLETE`.

---

## Template

```
## T05 — tap-hold-classification — 2026-09-08

**Status:** complete
**Branch:** task/T05-tap-hold-classification
**Commit:** a1b2c3d
**Criteria covered:** 13, 14, 15, 18
**Tests:** 11 added, 34 total, all green

**Done:**
What was built, in two or three lines.

**Tried and abandoned:**
Approaches that failed and why. This is the section that stops the next
iteration repeating the same dead end.

**Contradicts PRD:**
Anything in the spec that turned out wrong, ambiguous, or unbuildable.
None if none.

**Questions raised:** Q003 [ASSUMED]

**Defects noted in earlier tasks (not fixed):**
None.
```

---

## Log

## T00 — environment setup — 2026-09-08

**Status:** complete (not a numbered task; recorded for context)

**Done:**
Debian Trixie host prepared. Installed `build-essential cmake ninja-build git
python3 gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
pkg-config libusb-1.0-0-dev`. Cloned pico-sdk at tag 2.3.1 to `~/pico/pico-sdk`
with submodules initialised (TinyUSB present). `PICO_SDK_PATH` exported.

Verified end to end by building `pico-examples/blink` with `-DPICO_BOARD=pico2`.
Configure resolved to platform `rp2350-arm-s`, compiler
`pico_arm_cortex_m33_gcc`, GCC 14.2.1. SDK built picotool 2.3.1 into
`~/pico/picotool-build` — version matches the SDK, so no compatibility error.
`blink.uf2` produced (12 KB) and flashed successfully to hardware.

**Notes:**
- Debian's `pico-sdk-source` package (2.0.0) was deliberately NOT used — too old
  for current RP2350 work.
- The SDK-built picotool reports `PICOTOOL_NO_LIBUSB is set`. Expected: it only
  needs offline ELF-to-UF2 conversion. Device commands are unavailable; flashing
  is drag-and-drop via the `RP2350` mass-storage volume.

**Contradicts PRD:** None.
**Questions raised:** None.
