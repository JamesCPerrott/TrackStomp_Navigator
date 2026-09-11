LOOP-STATUS: RUNNING

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

## T01 — repo-skeleton — 2026-09-11

**Status:** complete
**Branch:** task/T01-repo-skeleton
**Commit:** (this commit)
**Criteria covered:** —
**Tests:** none (Verify: build)

**Done:**
Copied `pico_sdk_import.cmake` from SDK 2.3.1. Added the PRD §12 tree as empty stubs,
`tusb_config.h` with `CFG_TUD_ENABLED 0`, and a spinning `main.cpp`. CMakeLists.txt
sets `PICO_BOARD pico2` and `PICOTOOL_FETCH_FROM_GIT_PATH`, asserts SDK path and
version ≥ 2.3.1, links `pico_stdlib` / `tinyusb_device` / `hardware_flash` /
`hardware_sync`, wires `DEBUG_UART` (default OFF) to UART stdio with USB stdio off,
and calls `pico_add_extra_outputs()`. Configure used `-G Ninja`. Produced
`build/trackstomp_navigator.uf2` (12 KB). Platform `rp2350-arm-s`, compiler
`pico_arm_cortex_m33_gcc`. Picotool came from `~/pico/picotool-build` (not rebuilt).

**Tried and abandoned:**
- `clang-format` and `clang-tidy` are not on PATH. `clang-tools-19` is installed but
  does not ship those binaries; the `clang-format` / `clang-tidy` packages are not
  installed. `apt-get install` could not run (sudo password required). Lint not
  executed. Source stubs are include-guards / a bare spin loop only.

**Contradicts PRD:**
None. `CMakeLists.txt` is required at the repo root and is omitted from the §12
tree dump; added there.

**Questions raised:** None (DEBUG_UART wiring and `-G Ninja` decided in-session).

**Defects noted in earlier tasks (not fixed):**
None.

## T02 — config-header — 2026-09-11

**Status:** complete
**Branch:** task/T02-config-header
**Commit:** (this commit)
**Criteria covered:** —
**Tests:** 1 added (PRD §6.2 parse vs CUE_TABLE), 1 total, green

**Done:**
Filled `src/config.h` with every PRD §13 constant at the specified values and a
33-entry `CUE_TABLE` in §6.2 row order (`Cue` + `CueTrigger`). Compile-time
asserts require length 33 and distinct notes covering 0–32. Host CMake path
`BUILD_HOST_TESTS=ON` skips the Pico SDK and builds `tests/test_cue_table.cpp`,
which parses the markdown table and compares note, trigger type, and buttons
to `CUE_TABLE[i]`. Target build compiles `config.h` via `main.cpp`. Swap of two
table rows went red, then green after revert. clang-format and clang-tidy clean
on changed files (`-p build-host`). Host configure uses `-G Ninja`.

**Tried and abandoned:**
- Default Unix Makefiles for `build-host` — `ninja -C build-host` failed; reconfigured
  with `-G Ninja`.
- `fprintf` in the test — `cert-err33-c` treats an unchecked return as an error;
  switched to `std::cerr`.
- `bool seen[33]` completeness loop — `std::all_of` is not constexpr in C++17;
  replaced with a `uint64_t` bit mask.
- `clang-tidy -p build` (ARM compile_commands) — host clang cannot find the
  cross-compile `cstdint`; used `-p build-host` per AGENTS.md.

**Contradicts PRD:**
None. `tests/` is not in the §12 tree dump; required by T02 for the transcription
check and kept self-contained for T03 to restructure.

**Questions raised:** None.

**Defects noted in earlier tasks (not fixed):**
None.

## T03 — host-test-harness — 2026-09-11

**Status:** complete
**Branch:** task/T03-host-test-harness
**Commit:** (this commit)
**Criteria covered:** —
**Tests:** 3 added (bounce, timing, capture) plus existing cue-table, 4 total, green

**Done:**
Host preset `cmake --preset host` builds `input/`, `sequencer/`, and `ui/` with no Pico
SDK (`firmware_host` + `harness`). Fake 1 ms clock: `advance`/`advance_to` run
`buttons_scan` → `sequencer_tick` → `ui_tick` each step; `press`/`release` process the
current timestamp without moving the clock. Logical buttons 1–10, bounce injection,
capture buffers for Command / UiEvent / ButtonEvent / lamp. API in
`docs/TEST-HARNESS.md`. Three exemplar tests. Negative control: `g_now_ms += 2`
in `harness_advance` made `timing_deadline` fail
(`harness_now_ms() == HOLD_MS - 1U`), then reverted. Target build still links.
clang-format and clang-tidy clean on changed files (`-p build-host`).

**Tried and abandoned:**
- Lowercase `0xFFFFFFFFu` suffix — `readability-uppercase-literal-suffix`; switched to
  `0xFFFFFFFFU`.
- Relying on transitive includes for `uint32_t` / `std::vector` —
  `misc-include-cleaner`; added direct includes in the `.cpp` files.

**Contradicts PRD:**
None. Stub headers now declare the tick/poll/event types the harness needs; bodies
remain no-ops. `buttons_poll_event` is the test drain; T08 must not consume that
queue without leaving a copy if T04–T07 ButtonEvent assertions are to stay green.

**Questions raised:** None.

**Defects noted in earlier tasks (not fixed):**
None.

## T04 — debounce — 2026-09-11

**Status:** complete
**Branch:** task/T04-debounce
**Commit:** (this commit)
**Criteria covered:** 57
**Tests:** 1 added (debounce_filter), bounce and capture updated, 5 total, all green

**Done:**
Per-button debounce in `buttons_scan`: a GPIO level is accepted only after
`DEBOUNCE_MS` of stability. Debounced rising edges enqueue `ButtonEventKind::Tap`
(Q001). Bounce trains of 5–20 edges over 10 ms emit exactly one event. Criterion
57: 50 clean press/release cycles emit one event per press. Host link of
`firmware_host` and `harness` is now cyclic so `buttons_gpio_levels()` resolves.

**Tried and abandoned:**
- `clang-format` on `CMakeLists.txt` — the LLVM style wrecks CMake syntax;
  restored from git and re-applied the test target by hand.

**Contradicts PRD:**
None.

**Questions raised:** Q001 [ASSUMED] — Tap on debounced press until T05 classifies.

**Defects noted in earlier tasks (not fixed):**
None.

## T05 — tap-hold-classification — 2026-09-11

**Status:** complete
**Branch:** task/T05-tap-hold-classification
**Commit:** (this commit)
**Criteria covered:** 13, 14, 15, 15b, 15d, 18 (ButtonEvent layer; notes/lockout are T08/T07)
**Tests:** 1 added (tap_hold_classification), bounce/debounce/timing updated, 6 total, all green

**Done:**
Classification on debounced edges: release before `HOLD_MS` emits `TAP`.
Hold-capable buttons 1–5, 8, 10 emit `HOLD` at the threshold and suppress the
release. Buttons 6, 7, and 9 held past `HOLD_MS` emit nothing (15d written
first). Button 8 is not grouped with them. Chord waiver for 6+9 is T06.
Branched from `master` because T04 was already merged and the task branch
deleted.

**Tried and abandoned:**
- `ButtonSlot` field order bools-then-uint32s — `clang-analyzer-optin.performance.Padding`
  failed as an error; reordered timestamps first.

**Contradicts PRD:**
None. Criteria 15c (pending discarded) and MIDI note numbers wait for T07/T08.

**Questions raised:** Q002 [ASSUMED] — hold clock from `candidate_since`.

**Defects noted in earlier tasks (not fixed):**
None.

## T06 — chord-detection — 2026-09-11

**Status:** complete
**Branch:** task/T06-chord-detection
**Commit:** (this commit)
**Criteria covered:** 22, 23, 24, 25, 26, 27, 29, 29b (ButtonEvent layer)
**Tests:** 1 added (chord_detection); 24 and 26 written first; 7 total, all green

**Done:**
6+9 overlap flags `chord_overlap` for the rest of each press; neither taps.
Timer starts at the second press (`candidate_since`) and needs `CHORD_HOLD_MS`
of continuous debounced overlap. Release of either clears it; re-form starts
fresh. 6 and 9 skip T05 hold-suppression while `chord_armed`. A hold of 8
disarms the chord (29b). Completed chord emits `ChordHold` (Q003).

**Tried and abandoned:**
None.

**Contradicts PRD:**
None. Setup entry and chord lockout remain T09/T07.

**Questions raised:** Q003 [ASSUMED] — single ChordHold with id 6.

**Defects noted in earlier tasks (not fixed):**
None.

