LOOP-STATUS: RUNNING — Phase 10

# Progress log

Append-only. Never rewrite or delete a prior entry — the record of what was tried and abandoned is the point. If an earlier entry turns out to be wrong, add a new entry saying so.

The loop reads the most recent entry on each wake to find where it left off. **Human-completed tasks are logged here in the same format** — each phase has bootstrap tasks done interactively before the loop starts, and the loop will refuse to run until it sees them here and in the task list.

`LOOP-STATUS` on line 1 is the only mutable content in this file. Values: `NOT STARTED`, `RUNNING`, `HALTED — <reason>`, `COMPLETE`.

**`LOOP-STATUS` is scoped to the phase named on the line, not to the project.** v1 (T01–T21) reached `COMPLETE` on 2026-09-11; that fact now lives in the T21 entry and the Phase 9 divider below, not in the status line. A new phase resets the line. Without this, the first thing a Phase 9 loop reads is a `COMPLETE` it did not earn.

**`Commit: (this commit)` is correct for task entries — keep using it.** Each entry lands in the same commit as the code it describes, and a commit cannot contain its own hash; amending to add one changes the hash. `git log -- docs/PROGRESS.md` recovers the real SHA for any entry.

Validation is the exception. `VALIDATION.md` references a *different* commit — the firmware under test — so it records a tag and an artifact hash rather than `(this commit)`. v1 is tagged `v1.0`.

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

## T07 — hold-lockout — 2026-09-11

**Status:** complete
**Branch:** task/T07-hold-lockout
**Commit:** (this commit)
**Criteria covered:** 19, 20, 21 (ButtonEvent layer)
**Tests:** 1 added (hold_lockout), 8 total, all green

**Done:**
A hold enters `LOCKED` keyed to that button; other presses/releases emit
nothing. Lockout ends on release of the locked button, and every button still
down is tap-suppressed (20). Chord lockout ends only when both 6 and 9 are
released (Q004: no SETUP yet). After unlock, taps work again (21).

**Tried and abandoned:**
None.

**Contradicts PRD:**
None. MIDI notes for 19–21 wait for T08.

**Questions raised:** Q004 [ASSUMED] — chord lockout unlocks without SETUP.

**Defects noted in earlier tasks (not fixed):**
None.

## T08 — sequence-state-machine — 2026-09-11

**Status:** complete
**Branch:** task/T08-sequence-state-machine
**Commit:** (this commit)
**Criteria covered:** 4, 5, 6, 7, 8, 9, 10, 11, 12, 16, 17
**Tests:** 1 added (sequence_state_machine), capture updated, 9 total, all green

**Done:**
IDLE/PENDING resolution using `CUE_TABLE`. Test 6 (`3, 3` → 24) written first.
Prefix reassignment, Repeat (10), orphan suffixes, silent timeout, holds
clearing pending (16, 17). Sequencer reads a copied event queue so T04–T07
ButtonEvent assertions stay green (Q005).

**Tried and abandoned:**
None.

**Contradicts PRD:**
None. SETUP on ChordHold is T09.

**Questions raised:** Q005 [ASSUMED] — dual ButtonEvent queues.

**Defects noted in earlier tasks (not fixed):**
None.

## T09 — setup-mode-state — 2026-09-11

**Status:** complete
**Branch:** task/T09-setup-mode-state
**Commit:** (this commit)
**Criteria covered:** 28, 30, 31, 32, 33, 35, 38
**Tests:** 1 added (setup_mode_state), 10 total, all green

**Done:**
ChordHold discards PENDING and enters SETUP. Cues suppressed. Taps 1–10 set
`pending_channel`. Holds do nothing (no lockout). After the entry pair is
released, a 6+9 overlap-then-release exits unconditionally. Inactivity of
`SETUP_TIMEOUT_MS` commits. RAM-only channel; no flash (Q006).

**Tried and abandoned:**
None.

**Contradicts PRD:**
None. Flash write on exit is T19.

**Questions raised:** Q006 [ASSUMED] — setup notify/poll on input.

**Defects noted in earlier tasks (not fixed):**
None.

## T10 — led-pattern-engine — 2026-09-11

**Status:** complete
**Branch:** task/T10-led-pattern-engine
**Commit:** (this commit)
**Criteria covered:** 60
**Tests:** 1 added (led_pattern_engine), 11 total, all green

**Done:**
Tick-driven 7-level priority stack in `ui_tick`. No `sleep_ms`. Setup is
solid on (priority 3); idle is off. Other layers stubbed for T11–T13.
UiEvents copied for the engine drain (Q007). Host test asserts a < 10 ms
bound per tick and the setup on/off waveform.

**Tried and abandoned:**
Indexing `harness_lamp_trace()[t]` as time — extra `harness_press` samples
at the same timestamp shift the index; used `.back()` instead.

**Contradicts PRD:**
None.

**Questions raised:** Q007 [ASSUMED] — dual UiEvent queues.

**Defects noted in earlier tasks (not fixed):**
None.

## T11 — led-performance-patterns — 2026-09-11

**Status:** complete
**Branch:** task/T11-led-performance-patterns
**Commit:** (this commit)
**Criteria covered:** 45, 46, 47, 48, 49
**Tests:** 1 added (led_performance_patterns), 12 total, all green

**Done:**
Pending slow-flashes at 1000 ms (`PENDING_FLASH_MS` on/off). Lockout
fast-flashes at 100 ms until the locked button's accepted press ends (Q008).
Every `Sent` cue is one `CUE_FLASH_MS` pulse, with a forced
`CUE_FLASH_GAP_MS` off gap when the lamp is already lit. Hold uses `Locked`
only, so lockout outranks cue flash. Solid on remains setup-only.

**Tried and abandoned:**
None.

**Contradicts PRD:**
None.

**Questions raised:** Q008 [ASSUMED] — lockout flash duration via
`buttons_accepted_pressed`.

**Defects noted in earlier tasks (not fixed):**
None.

## T12 — led-chord-progress — 2026-09-11

**Status:** complete
**Branch:** task/T12-led-chord-progress
**Commit:** (this commit)
**Criteria covered:** 41, 42, 43, 44
**Tests:** 1 added (led_chord_progress), 13 total, all green

**Done:**
While `chord_armed`, priority 2 forces the lamp off for
`CHORD_LED_DELAY_MS`, then flashes at a 250 ms period. Setup solid takes
over at 5 s. Abort drops the override immediately; re-form restarts the
blackout from `buttons_chord_start()` (Q009).

**Tried and abandoned:**
Driving the waveform from the tick `chord_armed` rises — that would be
debounce-late and miss “500 ms from the re-press”.

**Contradicts PRD:**
None.

**Questions raised:** Q009 [ASSUMED] — poll chord_armed / chord_start from
input.

**Defects noted in earlier tasks (not fixed):**
None.

## T13 — led-channel-blink — 2026-09-11

**Status:** complete
**Branch:** task/T13-led-channel-blink
**Commit:** (this commit)
**Criteria covered:** 50, 51, 52, 53, 55, 56
**Tests:** 1 added (led_channel_blink), 14 total, all green

**Done:**
Channel blink plays gap / N pulses / gap at priority 1, then falls
through. Started on boot (`ui_indicate_channel` in `main`), setup
channel taps, and both setup-exit paths. A new blink cancels the old.
`Sent`/`Locked` abort it so a cue is not delayed. Setup stays solid on
entry, including while 6 and 9 are held (Q010).

**Tried and abandoned:**
Starting the boot blink on `now < g_last_now` reset — that hid PENDING
under the lead gap after every `harness_reset`.

**Contradicts PRD:**
None.

**Questions raised:** Q010 [ASSUMED] — boot via `ui_indicate_channel`, not
on harness reset.

**Defects noted in earlier tasks (not fixed):**
None.

## T14 — gpio-input — 2026-09-11

**Status:** complete
**Branch:** task/T14-gpio-input
**Commit:** (this commit)
**Criteria covered:** —
**Tests:** 0 added, 14 total, all green. pico2 `.uf2` produced.

**Done:**
GP6–GP15 initialised as inputs with internal pull-ups only (no
pull-downs). `buttons_gpio_levels()` returns a single masked
`gpio_get_all()`. Host path unchanged (`gpio_host.cpp`). `buttons_init()`
at boot (Q011). Hardware confirmation of the switches is T22.

**Tried and abandoned:**
None.

**Contradicts PRD:**
None.

**Questions raised:** Q011 [ASSUMED] — `buttons_init` at boot, scan loop
still T20.

**Defects noted in earlier tasks (not fixed):**
None.

## T15 — usb-midi-descriptors — 2026-09-11

**Status:** complete
**Branch:** task/T15-usb-midi-descriptors
**Commit:** (this commit)
**Criteria covered:** 1, 2 (build only; host enumeration is hardware)
**Tests:** 0 added, 14 total, all green. pico2 `.uf2` produced.

**Done:**
`CFG_TUD_ENABLED 1`, MIDI class only (CDC/MSC/HID/vendor off). Device
identity from PRD §9.1: VID `0x2E8A`, PID `0xFFFE` (placeholder until a
`raspberrypi/usb-pid` allocation is merged), `bcdDevice` `0x0100`,
strings `TrackStomp` / `TrackStomp Navigator` / 16-hex serial from
`pico_get_unique_board_id_string()` / jack `Navigator Cues`. One virtual
cable, IN plus TinyUSB-required OUT (Q012).

**Tried and abandoned:**
`TUD_MIDI_DESCRIPTOR` — jack string index is hardcoded 0, so the port
would not be named `Navigator Cues`.

**Contradicts PRD:**
None.

**Questions raised:** Q012 [ASSUMED] — OUT endpoint kept; USB task is T20.

**Defects noted in earlier tasks (not fixed):**
None.

## T16 — midi-output — 2026-09-11

**Status:** complete
**Branch:** task/T16-midi-output
**Commit:** (this commit)
**Criteria covered:** 3 (build/host bytes; on-host Playback is hardware)
**Tests:** 1 added (midi_note_transmission), 15 total, all green

**Done:**
MIDI layer owns the channel (default 1). `midi_out_send` writes a full
Note On, velocity `MIDI_VELOCITY`, no running status. `SEND_NOTE_OFF` is
honoured and currently false. Sequencer is not called (Q013). Host
capture asserts channel 1 and 8 encodings and repeated identical cues.

**Tried and abandoned:**
Polling `sequencer_poll_command` from midi_out — that would empty the
harness Command drain.

**Contradicts PRD:**
None.

**Questions raised:** Q013 [ASSUMED] — send API only; T20 drains Commands.

**Defects noted in earlier tasks (not fixed):**
None.

## T17 — led-driver — 2026-09-11

**Status:** complete
**Branch:** task/T17-led-driver
**Commit:** (this commit)
**Criteria covered:** —
**Tests:** 0 added, 15 total, all green. pico2 `.uf2` produced.

**Done:**
Panel GP16 (4 mA) and onboard GP25 are driven from the same `ui_lamp`
value, high = on, no inversion. `ui_led_init` at boot (Q014). Host path
unchanged. Hardware confirmation that both LEDs track is T22.

**Tried and abandoned:**
None.

**Contradicts PRD:**
None.

**Questions raised:** Q014 [ASSUMED] — drive from `ui_tick`; loop is T20.

**Defects noted in earlier tasks (not fixed):**
None.

## T18 — config-store-read — 2026-09-11

**Status:** complete
**Branch:** task/T18-config-store-read
**Commit:** (this commit)
**Criteria covered:** 39, 40, 54 (host: blank/CRC/magic/range fallback to
channel 1 and no write; on-device erase/boot blink is T22)
**Tests:** 1 added (config_store_read), 16 total, all green. pico2 `.uf2`
produced. Linker FLASH length is 4 MB minus 4 KB.

**Done:**
Last 4 KB excluded via `pico_override_flash_size`. `ConfigRecord` matches
§10.1. Boot reads, validates magic/version/CRC32/channel, falls back to
channel 1, and does not write (Q015–Q017). Host covers erased, corrupt
CRC, bad magic, out-of-range, unknown version, and a valid channel 8.

**Tried and abandoned:**
A host-only static constructor to fill the fake sector with `0xFF` —
`cert-err58-cpp` rejects throwing static initializers.

**Contradicts PRD:**
None.

**Questions raised:** Q015 [ASSUMED] CRC-32/ISO-HDLC; Q016 [ASSUMED]
linker override as offset; Q017 [ASSUMED] boot apply in `main`.

**Defects noted in earlier tasks (not fixed):**
None.

## T19 — config-store-write — 2026-09-11

**Status:** complete
**Branch:** task/T19-config-store-write
**Commit:** (this commit)
**Criteria covered:** 36, 37 (host: write only when channel changes, no-op
skip; on-device setup-exit persist is T20/T22)
**Tests:** write cases added to config_store_read, 16 total, all green.
pico2 `.uf2` produced.

**Done:**
`config_store_write_channel` programs a CRC'd `ConfigRecord` into the
reserved sector via a noinline RAM helper with IRQs off. Unchanged or
out-of-range channels skip erase. No call site in the main loop yet
(Q018). Host asserts factory write-1 is a no-op and rewrite of 8 is too.

**Tried and abandoned:**
None.

**Contradicts PRD:**
None.

**Questions raised:** Q018 [ASSUMED] — write API; T20 owns the setup-exit
call; noinline RAM commit.

**Defects noted in earlier tasks (not fixed):**
None.

## T20 — main-loop-integration — 2026-09-11

**Status:** complete
**Branch:** task/T20-main-loop-integration
**Commit:** (this commit)
**Criteria covered:** — (wires T08–T19; on-device USB/MIDI is T22)
**Tests:** 1 added (main_loop_dispatch), 17 total, all green. pico2 `.uf2`
produced.

**Done:**
Cooperative loop is `tud_task`, scan, sequencer, dispatch, `ui_tick`,
`sleep_us(500)`. Dispatch sends Commands over MIDI and commits setup
channel to flash + midi_out (Q019). Sequencer still does not call GPIO,
MIDI, or flash. Core 1 unused.

**Tried and abandoned:**
`tusb_init(void)` — TinyUSB in SDK 2.3.1 requires a rhport/role init
unless `CFG_TUSB_RHPORT0_MODE` is defined.

**Contradicts PRD:**
None. Dispatch sits between `sequencer_tick` and `ui_tick`; §12.2 does
not list it because MIDI/flash are not sequencer work.

**Questions raised:** Q019 [ASSUMED] — dual Command queue, setup-commit
latch, `tusb_init(rhport, &dev_init)`.

**Defects noted in earlier tasks (not fixed):**
None.

## T21 — release-build-hardening — 2026-09-11

**Status:** complete
**Branch:** task/T21-release-build-hardening
**Commit:** (this commit)
**Criteria covered:** 2 (host: no CDC/MSC/vendor in descriptors, USB stdio
off; on-device "no serial port" is T22)
**Tests:** 1 added (release_build_hardening), 18 total, all green. pico2
`.uf2` produced. Post-build grep of ELF loadable sections and UF2 finds
no `/home/` or `/Users/` paths.

**Done:**
`DEBUG_UART` still defaults OFF; when ON it defines `DEBUG_UART` and
calls `stdio_init_all()` for GP0/GP1 UART only. USB stdio stays off.
`-ffile-prefix-map` plus a CMake post-build check (Q020). Tier 2 not
implemented.

**Tried and abandoned:**
None.

**Contradicts PRD:**
None.

**Questions raised:** Q020 [ASSUMED] — prefix-map + `strings -d`/`-a`;
`stdio_init_all` only under `DEBUG_UART`.

**Defects noted in earlier tasks (not fixed):**
None.

T01–T21 are committed after this push. T22–T23 are human hardware
validation and are outside the loop.

---

# Phase 9 — Per-switch status LEDs

v1 closed here. **T01–T21 complete, merged to `master`, `ctest` green, firmware running on
hardware.** T22 and T23 remain outstanding and are hardware-only; they are not blockers for Phase 9,
which is entirely host-testable except T29.

Phase 9 implements PRD §11.5 — ten status LEDs, one above each footswitch, on GP5/GP4/GP3/GP2 and
GP22 down to GP17. It supersedes the v2 display: GP4 and GP5 were the reserved I2C pins.

## Bootstrap — required before the loop may run

T24 and T25 are human-led and must be logged here, in the standard entry format, before
`LOOP-PROMPT.md`'s precondition will pass.

**T24 — cue-table-reverse-lookup.** Its entry must record that the reverse direction is derived from
`CUE_TABLE` rather than from a second hardcoded list, and that every entry round-trips.

**T25 — switch-led-harness.** Its entry must explicitly record that **the matcher was proven to
fail** — a test asserting a deliberately wrong mask was written, observed failing with `file:line`,
and then corrected. This is not ceremony. T03 carried the same requirement because a harness whose
assertions cannot fail reports green for every task after it, and T25 is that same risk on a new
surface. The Phase 9 loop prompt checks this log for the statement and halts if it is absent.

Both entries should also confirm `ctest` is green including T10–T13, since the whole phase depends on
the §11.4 panel engine staying untouched.

## Standing note for every Phase 9 entry

Each entry must state whether criteria **41–56 still pass**. The per-switch engine lives beside the
finished panel-LED engine in the same module, and a regression there is the most likely way this
phase does damage. Recording it per task is what makes that visible at a glance across the run rather
than at the end of it.

Questions for this phase start at **Q026**. Q021–Q025 are the assumptions recorded when §11.5 was
specified; they are decisions already made, not open items.

---

## T24 — cue-table-reverse-lookup — 2026-09-21

**Status:** complete
**Branch:** task/T24-cue-table-reverse-lookup
**Commit:** (this commit)
**Criteria covered:** —
**Tests:** 1 added (cue_origin_lookup), 19 total, all green. pico2 `.uf2`
produced. T10–T13 still pass (`led_pattern_engine`,
`led_performance_patterns`, `led_chord_progress`, `led_channel_blink`), so
criteria **41–56 still pass**.

**Done:**
`cue_origin_for_note` in `config.h` returns `CueOrigin`: a cue class plus the
buttons stored on the matching `CUE_TABLE` entry. The reverse direction is
**derived from `CUE_TABLE` rather than a second list** — a scan of that
array, with class taken from the entry's shape (hold trigger, else one
button is standalone, else equal buttons are a self-pair, else a pair). An
unmapped note returns `CueClass::Invalid` with both buttons `0xFF`. **Every
entry round-trips**, via `static_assert(cue_table_origins_round_trip())` and
a host loop over the real table. The round-trip test **was proven to fail**:
expecting `button_b + 1` reported
`tests/test_cue_origin.cpp:50 FAIL: REQUIRE failed: origin.button_b == static_cast<uint8_t>(cue.button_b + 1U)`,
then the assertion was restored.

**Tried and abandoned:**
Early `return false` in `cue_table_origins_round_trip`. clang-tidy
`readability-use-anyofallof` wants `std::all_of`, which is not `constexpr`
in C++17 — the same limit T02 hit. The predicate accumulates a `bool`
instead.

**Contradicts PRD:**
None.

**Questions raised:** None.

**Defects noted in earlier tasks (not fixed):**
None.

## T25 — switch-led-harness — 2026-09-21

**Status:** complete
**Branch:** task/T25-switch-led-harness
**Commit:** (this commit)
**Criteria covered:** —
**Tests:** 1 added (switch_led_capture), 20 total, all green. pico2 `.uf2`
produced. T10–T13 still pass (`led_pattern_engine`,
`led_performance_patterns`, `led_chord_progress`, `led_channel_blink`), so
criteria **41–56 still pass**. `harness_lamp_trace()` is unchanged.

**Done:**
Parallel per-switch capture beside the lamp trace: one `uint16_t` mask per
pipeline pass, bit 0 = LED 1. `ui_switch_leds()` exists and returns 0.
`REQUIRE_SWITCH_LEDS` is an exact match. `SWITCH_LED(n)` is a constant 1..10;
0 and 11 do not compile. Bits at or above `SWITCH_LED_COUNT` fail the test.
**The switch-LED matcher was proven to fail.** Each case was introduced
alone, observed failing with `file:line`, then restored:
- wrong LED: seed 2, require 1 —
  `tests/test_switch_leds.cpp:10 FAIL: switch LEDs {2} (0000000010) != {1} (0000000001)`
- superset (exact match): seed 1 and 6, require 1 —
  `tests/test_switch_leds.cpp:14 FAIL: switch LEDs {1,6} (0000100001) != {1} (0000000001)`
- subset: seed 1, require 1 and 6 —
  `tests/test_switch_leds.cpp:10 FAIL: switch LEDs {1} (0000000001) != {1,6} (0000100001)`
- non-empty against empty: seed 1, `REQUIRE_NO_SWITCH_LEDS()` —
  `tests/test_switch_leds.cpp:10 FAIL: switch LEDs {1} (0000000001) != {} (0000000000)`
- off-by-one: value seeded at index 20, asserted at index 19 —
  `tests/test_switch_leds.cpp:29 FAIL: REQUIRE failed: harness_switch_trace().at(19) == SWITCH_LED(1)`
- unused bits: seed bit 10 —
  `tests/harness/harness.cpp:288 FAIL: switch LED mask has bits at or above SWITCH_LED_COUNT: 0x400`
clang-format and clang-tidy clean on changed files (`-p build-host`).

**Tried and abandoned:**
None.

**Contradicts PRD:**
None. `ui_switch_leds()` is a stub 0 until T26. A press at the current
timestamp appends a second sample for that `now` on both traces; the switch
trace matches the lamp trace, which already did this.

**Questions raised:** None.

**Defects noted in earlier tasks (not fixed):**
None.

## T26 — switch-led-engine — 2026-09-21

**Status:** complete
**Branch:** task/T26-switch-led-engine
**Commit:** (this commit)
**Criteria covered:** 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 78, 79, 80
**Tests:** 1 added (switch_led_engine), 21 total, all green. pico2 `.uf2`
produced. T10–T13 still pass (`led_pattern_engine`,
`led_performance_patterns`, `led_chord_progress`, `led_channel_blink`), so
criteria **41–56 still pass**.

**Done:**
One indication owns the mask. Pending flashes at 125/125 on the prefix LED.
A pair is both buttons solid for `LED_CONFIRM_MS`; a self-pair is one LED
with the 2000/1000/2000 notch; a standalone is five 1000 ms phases ending
lit. A hold is solid from the fire, for at least `LED_CONFIRM_MS`, and stays
lit while `buttons_accepted_pressed` is true. An accepted `ButtonEvent`
destroys a running confirmation before that tick's `UiEvent` is applied, so
a new pending or cue replaces it and an orphan suffix leaves the surface
dark. A press discarded by lockout never reaches `queue_push` and cancels
nothing. Cue buttons come from `cue_origin_for_note`. Chord, setup blink,
and boot are not drawn in this task.

**Tried and abandoned:**
None. A per-LED state machine was not attempted; the resolver picks a single
confirmation or the pending flash and clears every other bit.

**Contradicts PRD:**
None. The hold floor follows the debounced accepted press (Q026), so the LED
stays through release debounce rather than the raw pin edge.

**Questions raised:** Q026

**Defects noted in earlier tasks (not fixed):**
None.

## T27 — switch-led-setup-chord — 2026-09-21

**Status:** complete
**Branch:** task/T27-switch-led-setup-chord
**Commit:** (this commit)
**Criteria covered:** 71, 72, 73, 74, 75, 76, 77
**Tests:** 1 added (switch_led_setup_chord), 22 total, all green. pico2 `.uf2`
produced. T10–T13 still pass (`led_pattern_engine`,
`led_performance_patterns`, `led_chord_progress`, `led_channel_blink`), so
criteria **41–56 still pass**. Criterion 82's host bound is the existing
`ui_tick` timing check in `led_pattern_engine`, which now runs both engines.

**Done:**
Chord progress is priority 2 and is read from `buttons_chord_armed()` /
`buttons_chord_start()`, the same accessors as the panel engine. The first
500 ms is full darkness, including over a live pending flash or a
confirmation; then LEDs 6 and 9 flash together at 125/125. Abort releases
the override on that tick and the indication underneath is wherever its own
clock says it is — an off-phase pending flash stays off rather than
restarting. Re-forming the chord restarts the blackout from the new press.
At 5000 ms, setup entry replaces the flash with the stored channel blinking
1000/1000 from the on phase, including while 6 and 9 are still held. A
channel tap restarts that on phase on the same tick. Exit by chord or by
the 30 s timeout clears all ten, with no exit pattern. Boot, started from
`ui_indicate_channel` beside the panel blink, holds the stored channel solid
for 2000 ms and is cancelled by an accepted button event. It is not started
on harness reset.

**Tried and abandoned:**
None.

**Contradicts PRD:**
None.

**Questions raised:** None.

**Defects noted in earlier tasks (not fixed):**
None.

## T28 — switch-led-driver — 2026-09-21

**Status:** complete
**Branch:** task/T28-switch-led-driver
**Commit:** (this commit)
**Criteria covered:** —
**Tests:** none added, 22 total, all green. pico2 `.uf2` produced. T10–T13
still pass (`led_pattern_engine`, `led_performance_patterns`,
`led_chord_progress`, `led_channel_blink`), so criteria **41–56 still pass**.
Criteria 61–80 are the T26 and T27 host tests. 81 is those panel tests.
82 is the existing `ui_tick` bound in `led_pattern_engine`.

**Done:**
`ui_tick` drives all ten switch LEDs with one `gpio_put_masked(mask, value)`.
The mask and the value are built from `SWITCH_LED_GPIO`, so the gap between
GP2 and GP22 is explicit. High means lit. Each pin is initialized to 4 mA,
matching the panel LED. `SWITCH_LED_DUTY` is the PRD table, all 255, and
`switch_led_duty_is_full_scale()` rejects any other value. This layer does
not PWM and does not encode state in brightness; the GPIO level is the
engine bit. Compile-time checks map LED 1 to GP5, LED 5 to GP22, LED 10 to
GP17, and LEDs 6 and 9 to GP21 and GP18.

**Tried and abandoned:**
Context7 has no page for `gpio_put_masked`. The call matches Pico SDK 2.3.1
`hardware/gpio.h`: `gpio_put_masked(uint32_t mask, uint32_t value)` drives
each set mask bit from the same bit of `value`.

**Contradicts PRD:**
None. A non-255 duty cannot dim here; PWM would be a new peripheral, and
the static assert makes that edit fail the build instead of compiling as a
silent full-on.

**Questions raised:** None.

**Defects noted in earlier tasks (not fixed):**
None.

---

# Phase 10 — BLE MIDI on the Pico 2 W

Phase 9's automated tasks are logged above. T29 is still hardware and is not a blocker for this phase.

## T30 — pico2w-board-target — 2026-11-06

**Status:** build complete; hardware not run
**Branch:** task/T30-pico2w-board-target
**Commit:** (this commit)
**Criteria covered:** 111 (partial — gpio call-site review below; the onboard mirror was not observed). 102 was not run.
**Tests:** none added, 22 total, all green. pico2 and pico2_w `.uf2` produced. T10–T13 still pass (`led_pattern_engine`, `led_performance_patterns`, `led_chord_progress`, `led_channel_blink`), so criteria **41–56 still pass**.

**Done:**
`pico2_w` preset configures into `build-w`. The configure log says `Target board (PICO_BOARD) is 'pico2_w'.` and `Using board configuration from .../boards/pico2_w.h`. The `pico2` preset still reports `pico2` and `pico2.h`. `CMakeLists.txt` sets `PICO_BOARD` to `pico2` only when the caller has not set it (T01's pin, without overriding T30). CI gains a Firmware build (pico2_w) job that fails before configure if `lib/btstack/src/bluetooth.h`, `lib/cyw43-driver/src/cyw43.h`, or `lib/lwip` is missing (`lib/lwip/src/Filelists.cmake` is the SDK's own checkout test, so an empty submodule directory fails too).

`pico2_w` links `pico_cyw43_arch_poll` only. `CYW43_LWIP=0` and `PICO_CYW43_ARCH_POLL=1` are on that target's compile line. `PICO_CYW43_ARCH_THREADSAFE_BACKGROUND`, `PICO_CYW43_ARCH_FREERTOS`, and `CYW43_ENABLE_BLUETOOTH` are not defined. The driver's default `CYW43_LWIP` is 1 (`cyw43_config.h`); `pico_cyw43_arch_poll` does not clear it, so the target sets 0. The ELF defines `cyw43_arch_init` and the weak no-lwIP `cyw43_cb_tcpip_*` stubs. It does not define `lwip_`, `btstack_`, or FreeRTOS symbols. The pico2 ELF has no `cyw43` symbols. The SDK still compiles `cyw43_arch_threadsafe_background.c` and `cyw43_arch_freertos.c` because they are sources of `pico_cyw43_arch`; both objects are empty under the poll arch.

`cyw43_arch_init()` runs once at boot on `pico2_w`, before `board_led_init`. `cyw43_arch_poll()` runs immediately after `tud_task()` when that init returned 0. A failed init logs `cyw43_arch_init failed: %d` on `DEBUG_UART` builds only, skips the poll, and skips every onboard-LED write (Q033). The scan loop still runs. No BTstack.

`src/board/board_led.cpp` owns the mirror. pico2 drives GP25 (`kPico2OnboardLed`) and is initialised beside `ui_led_init`. pico2_w calls `cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, …)` and writes only when the level changes. `static_assert(CYW43_WL_GPIO_LED_PIN == 0)` and `#error` if `PICO_DEFAULT_LED_PIN` is defined both sit in the pico2_w branch; that build succeeded, so the pin is 0 and the macro is absent. `ui.cpp` lost T17's four GP25 lines and nothing else. `ui_lamp()` is unchanged. `LED_ONBOARD_GPIO` left `config.h` so a pico2_w translation unit does not name pin 25.

**gpio_ call sites.** The pattern excludes `cyw43_arch_gpio_put` and `switch_led_gpio_value`. A bare search for 23 or 25 is not evidence: those numbers are cue notes.

```
grep -nE '(^|[^[:alnum:]_])gpio_(init|init_mask|set_dir|set_dir_in_masked|set_drive_strength|put|put_masked|pull_up|get_all)\(' -r src --include='*.cpp' --include='*.h' --include='*.c'
```

```
src/input/buttons.cpp:319:    gpio_init_mask(kButtonGpioMask);
src/input/buttons.cpp:320:    gpio_set_dir_in_masked(kButtonGpioMask);
src/input/buttons.cpp:322:        gpio_pull_up(BUTTON_GPIO_BASE + i);
src/input/buttons.cpp:329:    return gpio_get_all() & kButtonGpioMask;
src/ui/ui.cpp:391:    gpio_put(LED_PANEL_GPIO, g_lamp);
src/ui/ui.cpp:392:    gpio_put_masked(switch_led_pin_mask(), switch_led_gpio_value(g_switch_mask));
src/ui/ui.cpp:411:    gpio_init(LED_PANEL_GPIO);
src/ui/ui.cpp:412:    gpio_set_dir(LED_PANEL_GPIO, GPIO_OUT);
src/ui/ui.cpp:413:    gpio_set_drive_strength(LED_PANEL_GPIO, GPIO_DRIVE_STRENGTH_4MA);
src/ui/ui.cpp:414:    gpio_put(LED_PANEL_GPIO, false);
src/ui/ui.cpp:418:        gpio_init(pin);
src/ui/ui.cpp:419:        gpio_set_dir(pin, GPIO_OUT);
src/ui/ui.cpp:420:        gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_4MA);
src/ui/ui.cpp:422:    gpio_put_masked(switch_led_pin_mask(), 0U);
src/board/board_led.cpp:36:    gpio_put(kPico2OnboardLed, on);
src/board/board_led.cpp:49:    gpio_init(kPico2OnboardLed);
src/board/board_led.cpp:50:    gpio_set_dir(kPico2OnboardLed, GPIO_OUT);
```

On a pico2_w compile, the three `board_led.cpp` hits are in the `#else` of `RASPBERRYPI_PICO2_W` (`kPico2OnboardLed` is 25 only there). `nm` of `build-w` `board_led.cpp.o` shows `cyw43_arch_gpio_put` and not `gpio_init`. The pico2 object shows `gpio_init` and not `cyw43`. The other calls cannot receive 23, 24, 25, or 29: `kButtonGpioMask` is bits 6–15, `LED_PANEL_GPIO` is 16, and `SWITCH_LED_GPIO` is {5, 4, 3, 2, 22, 21, 20, 19, 18, 17}. Nothing in `src/` uses `PICO_DEFAULT_LED_PIN` as a pin.

**Tried and abandoned:**
None.

**Contradicts PRD:**
None.

**Questions raised:** Q033 [ASSUMED]

**Defects noted in earlier tasks (not fixed):**
None. Criterion 102 and the onboard-LED half of 111 need a Pico 2 W. Not run here.

## T31 — flash-map — 2026-11-06

**Status:** build complete; hardware not run
**Branch:** task/T31-flash-map
**Commit:** (this commit)
**Criteria covered:** 114 is the bench check and was not run. This commit is the fix for the v1 defect behind 36 and 38.
**Tests:** 1 added (`flash_map`), 23 total, all green. pico2 and pico2_w `.uf2` produced. T10–T13 still pass (`led_pattern_engine`, `led_performance_patterns`, `led_chord_progress`, `led_channel_blink`), so criteria **41–56 still pass**. `config_store_read` (T18/T19) passes unchanged.

**Done:**
Removed `pico_override_flash_size`. `PICO_FLASH_SIZE_BYTES` is no longer a compile definition on either target, so it stays the board header's 4 MB. The linker `FLASH` region is shortened only by `pico_flash_region.ld` (`LENGTH = 0x3fc000`), passed through `pico_add_linker_script_override_path` with no compile definition. `config_store` erases, programs, and reads at `CONFIG_FLASH_OFFSET`. Q016 is superseded for addressing: the store no longer treats `PICO_FLASH_SIZE_BYTES` as the sector offset.

`config.h` has the §13 flash-map constants. Static assertions require the image, config, bond, and E10 regions to be sector-aligned, contiguous, and non-overlapping, ending at `FLASH_TOTAL_BYTES`, with `CONFIG_FLASH_OFFSET == 0x3FC000`, `BOND_FLASH_OFFSET == 0x3FD000`, `FLASH_E10_RESERVED_OFFSET == 0x3FF000`, and `IMAGE_FLASH_LIMIT == 0x3FC000`. Nothing in `src/` passes the E10 offset to a read, erase, or program. The target half of `config_store.cpp` asserts both the config sector and the bond region sit inside `PICO_FLASH_SIZE_BYTES`. On `pico2_w`, `PICO_FLASH_BANK_STORAGE_OFFSET=0x3FD000` and `PICO_FLASH_BANK_TOTAL_SIZE=0x2000` are compile definitions, and `src/storage/flash_bank_map.cpp` (that target only) static-asserts they equal `BOND_FLASH_OFFSET` and `BOND_FLASH_SIZE`. `--abs-block` and `PICO_RP2350_A2_SUPPORTED` are unchanged. The pico2 UF2 recipe still passes `--abs-block`.

**Defect found in T18/T19 (Q016) and fixed here.** `pico_override_flash_size` set `PICO_FLASH_SIZE_BYTES` to `(4 * 1024 * 1024) - 4096` (`0x3FF000`). `config_store` then called `flash_range_erase(PICO_FLASH_SIZE_BYTES, FLASH_SECTOR_SIZE)` and the same offset for program. SDK 2.3.1 `hardware_flash/flash.c` begins both functions with `hard_assert(flash_offs + count <= PICO_FLASH_SIZE_BYTES)`. `0x3FF000 + 4096 <= 0x3FF000` is false. In a release build `hard_assert` calls `hard_assertion_failure()`, which calls `panic("Hard assert")` and does not return. That is the freeze on every setup exit that actually wrote, on both boards, seen at T28 and at T30. An exit with no channel change never called erase, so it looked fine, and nothing was ever saved. Criteria 36 and 38 had not been run on hardware.

**RP2350-E10.** Quoted from the RP2350 datasheet, Appendix D.2.1, pages 1351–1352 (PDF `datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf`, 1 October 2026):

> Reference: RP2350-E10
> Summary: UF2 drag-and-drop doesn’t work with partition tables
> Affects: RP2350 A2
> Description: When dragging and dropping a UF2 onto the USB Mass Storage Device, the bootrom on chip revision A2 doesn’t set up the flash before checking the partition table. This causes the UF2 download to fail if there is a partition table present.
> Workaround: Add a single block at the start of the UF2 with an Absolute family ID, targeting the end of Flash, with block number set to 0 and number of blocks set to 2. This block is written to flash first but doesn’t reboot the device, and sets up the flash for the rest of the UF2 to be downloaded correctly. This is handled for you automatically by `picotool` in the Pico SDK, which adds this block when generating UF2s if the `--abs-block` flag is specified. This workaround means that the last block of flash is erased when downloading such a UF2, which could overwrite user data.
> As of `picotool` version 2.1.0, this additional UF2 block is marked with `UF2_EXTENSION_RP2_IGNORE_BLOCK` (`0x9957e304`). The RP2350 A3 and later bootroms contain a fix for this erratum, and therefore don’t need the workaround. The presence of this extension allows the newer RP2350 to recognize and ignore the workaround block, thus avoiding the risk of overwriting user data.
> Fixed by: RP2350 A3 bootrom, Documentation, Software

Both board headers set `PICO_RP2350_A2_SUPPORTED`, so the SDK passes `--abs-block`. picotool 2.3.1 places that absolute block at `0x10FFFF00`. An A2 bootrom writes it, and §5.5.2 erases flash a 4 KB sector at a time, so the last sector is erased on every drag-and-drop. A3 and later ignore the block. Config therefore lives at `0x3FC000`, below the bond store. `0x3FF000` is `FLASH_E10_RESERVED_OFFSET` and is never written (PRD §10.1, §15 row 32). No unit had a saved channel, so the move has nothing to migrate.

**Linker map.** `Memory Configuration` from both `.elf.map` files. The SDK's generated fragment for a 4 MB part would say `0x00400000`. Both maps show the project fragment won:

```
Name             Origin             Length             Attributes
FLASH            0x10000000         0x003fc000         xr
```

**Highest loaded address**, end of the highest flash-resident LOAD (`.flash_end`), against `0x103FC000` (`XIP_BASE + IMAGE_FLASH_LIMIT`):

| Target | Highest loaded address | Below 0x103FC000 |
|---|---|---|
| pico2 | `0x1000666C` | yes |
| pico2_w | `0x10043548` | yes |

**Tried and abandoned:**
The first plan kept the config record in the last sector (`0x3FF000`) and only stopped using `pico_override_flash_size`. That loses the record on every A2 drag-and-drop because of `--abs-block`. Abandoned before this commit; the map above is the one that landed.

**Contradicts PRD:**
None.

**Questions raised:** None.

**Defects noted in earlier tasks (not fixed):**
None in software. Criterion 114 (channel 8 survives exit, power-cycle, and a UF2 drag-and-drop, including the 30 s timeout, on both boards) was not run.

## T31 — criterion 114 bench — 2026-11-06

**Status:** passed on hardware
**Branch:** task/T31-flash-map
**Commit:** effe42c (firmware under test; this note is a later append)
**Criteria covered:** 114, and with it the on-target half of 36 and 38

**Done:**
James ran criterion 114 on a Pico 2 and a Pico 2 W, both flashed with the T31 build `effe42c`. He reported the criterion passed on both boards. That is: select channel 8 and exit setup, and the pedal keeps playing with cues on channel 8 (no freeze); power-cycle, and it boots on channel 8; reflash by UF2 drag-and-drop, and it is still channel 8; change the channel again and leave by the 30 s timeout, and that exit does not freeze. The earlier T31 entry's "hardware not run" is superseded by this one.

**Tried and abandoned:**
None.

**Contradicts PRD:**
None.

**Questions raised:** None.

**Defects noted in earlier tasks (not fixed):**
None.

## T31 — host clang-tidy on flash_bank_map.cpp — 2026-11-06

**Status:** fixed, host checks green
**Branch:** task/T31-flash-map
**Commit:** not yet committed (follows effe42c)
**Criteria covered:** none new

**Done:**
CI host job passed all 23 tests, then `clang-tidy -p build-host` failed on `src/storage/flash_bank_map.cpp` with undeclared `PICO_FLASH_BANK_STORAGE_OFFSET` and `PICO_FLASH_BANK_TOTAL_SIZE`. That translation unit is pico2_w-only, so it was absent from `build-host/compile_commands.json`. Tidy inferred the sibling `config_store.cpp` command, which has `HOST_TEST` and not the bond-bank macros. The host library now compiles `flash_bank_map.cpp` with `PICO_FLASH_BANK_STORAGE_OFFSET=0x3FD000` and `PICO_FLASH_BANK_TOTAL_SIZE=0x2000`, the same definitions the pico2_w target passes. The static asserts still tie those macros to `config.h`. Host `ctest` is 23/23. `clang-tidy -p build-host src/storage/flash_bank_map.cpp` exits 0. The pico2_w compile definitions are unchanged.

**Tried and abandoned:**
None.

**Contradicts PRD:**
None.

**Questions raised:** None.

**Defects noted in earlier tasks (not fixed):**
None.

## T32 — link-harness — 2026-11-10

**Status:** complete
**Branch:** task/T32-link-harness
**Commit:** (this commit)
**Criteria covered:** —
**Tests:** 3 added (`link_transport_capture`, `harness_reset_unconditional`, `ble_no_radio_stub`), 26 total, all green from a clean `build-host`. pico2 and pico2_w `.uf2` produced from clean build directories, no compiler warnings, built one after the other with nothing else running (pico2 15:50:58–15:51:02, pico2_w 15:51:12–15:51:21). T10–T13 still pass (`led_pattern_engine`, `led_performance_patterns`, `led_chord_progress`, `led_channel_blink`), so criteria **41–56 still pass**. T26–T27 still pass (`switch_led_engine`, `switch_led_setup_chord`). `harness_lamp_trace()`, `harness_switch_trace()`, and every existing matcher are unchanged. No existing test file was edited.

**Done:**
`src/link/link_state.h/.cpp`, `src/midi/transport.h/.cpp`, `src/midi/ble_midi.h`, `src/midi/ble_midi_stub.cpp`, the fake transport in `tests/harness/harness.cpp`, the two matchers, and the exemplar `tests/test_link.cpp`. `docs/TEST-HARNESS.md`'s Phase 10 section now describes exactly what was built. `harness_reset()` now resets every module unconditionally (below).

**API changes from the planning draft.** Proposed before any code and agreed with James on 2026-11-10:

1. `link_on_request(LinkRequest)` is new. The draft had no entry point for the sequencer's request. The T32 skeleton forwards every request to the transport unchanged, so T35 can assert criterion 84 before T36 builds the window.
2. `Ready` and `Lost` are real, not stubbed: `Ready` sets `link_ready()`, `Lost` clears it. T34 needs a ready link and runs before T36.
3. `src/midi/ble_midi.h` is the transport boundary on every build: `ble_midi_request`, `ble_midi_poll_event`, `ble_midi_send`. The harness is its host implementation. `loop_dispatch` forwards `link_poll_request` → `ble_midi_request` and `ble_midi_poll_event` → `link_on_event`, the same code on host and target. `link_poll_request` has one queue and one consumer; tests never call it. `harness_link_event` queues the event in the fake and runs a pass; it does not call `link_on_event`.
4. `TransportSend` is decoded by the harness's own parser from the bytes on the two fake wires. USB: a second copy of every byte `write_bytes` produces (`midi_out_poll_byte_for_harness`; `midi_out_poll_byte` is untouched). BLE: the packet given to `ble_midi_send`, note read from byte 3. Bytes that do not decode fail the test. `TransportSend` therefore lives in `harness.h`, not `transport.h`.
5. `Transport` gains `None` as value 0. `transport_select(bool usb_mounted, bool ble_ready)` is the policy function.
6. `link_boot_connect_pending()` became `link_boot_connected()`: a latch `link_state` can compute. "Played or dropped" is `ui`'s to track, since `ui` only reads link accessors.
7. No `harness_link_ready()` wrappers. Tests call the link accessors directly.

`link_reset()` is host-only and called by `harness_reset()`, like `midi_out_reset()`. `LinkPairingPhase` is `Closed, Advertising, Bonding, BondOk`.

**Still stubbed.** The window never opens and the phase stays `Closed` (T36). Pairing events and `NoRadio` are ignored by `link_state` (T36). `link_boot_connected()` is false (T37). `transport_select` always returns `Usb` (T34); `midi_out_send` already calls it with `tud_mounted()` and `link_ready()`, so target behaviour is v1. The sequencer's behaviour is untouched (it gained only the host-only `sequencer_reset()`); T35 adds `sequencer_poll_link_request` and its hop in `loop_dispatch`. No constants were added to `config.h`.

**Target stub.** `ble_midi_stub.cpp` is linked into pico2 and pico2_w. It answers `BeginPairing` with one `NoRadio` and drops packets. James chose this over a silent no-op so that pico2 stays "exactly v1 + Phase 9" at every loop commit once T36 handles `NoRadio`. This is T38's "stub transport reports no radio" bullet delivered early; T38 replaces the stub on pico2_w only. `tests/test_ble_stub.cpp` links the stub without the harness, because the harness defines the same three functions.

**The link-request and transport-send matchers were proven to fail against deliberately wrong expectations.** With the firmware correct, `tests/test_link.cpp` was edited to assert a wrong `LinkRequest` list and, separately, a wrong `TransportSend`. Each failed with `file:line` and exit 1, and each was then corrected:

- `harness_debug_link_request(LinkRequestKind::BeginPairing)` issued `BeginPairing`; the test required `EndPairing`: `tests/test_link.cpp:131 FAIL: link request[0] BeginPairing != EndPairing`
- A real tap of button 10 with USB mounted sent note 20 on USB; the test required `{Ble, 20}`: `tests/test_link.cpp:35 FAIL: transport send[0] Usb:20 != Ble:20`

Those two are the first line of each list below. Every case in this section was introduced alone, observed failing with `file:line` and exit 1, then restored. The restored files were checked byte-for-byte by SHA-256 and the suite re-run green. All 25 were run again after the `harness_reset()` change below, alone, with the same lines.

`REQUIRE_LINK_REQUESTS`, wrong expectation in `tests/test_link.cpp`, firmware correct:
- wrong kind, issued Begin, required End — `tests/test_link.cpp:131 FAIL: link request[0] BeginPairing != EndPairing`
- wrong order, issued Begin, End, required End, Begin — `tests/test_link.cpp:133 FAIL: link request[0] BeginPairing != EndPairing`
- short list, issued Begin, End, required Begin — `tests/test_link.cpp:133 FAIL: expected 1 LinkRequest(s), got 2`
- issued Begin, `REQUIRE_NO_LINK_REQUESTS()` — `tests/test_link.cpp:131 FAIL: expected 0 LinkRequest(s), got 1`
- none issued, required Begin — `tests/test_link.cpp:21 FAIL: expected 1 LinkRequest(s), got 0`

`REQUIRE_TRANSPORT_SENDS`, wrong expectation against a real tap of button 10 with USB mounted, firmware correct:
- wrong transport, required `{Ble, 20}` — `tests/test_link.cpp:35 FAIL: transport send[0] Usb:20 != Ble:20`
- wrong note, required `{Usb, 21}` — `tests/test_link.cpp:35 FAIL: transport send[0] Usb:20 != Usb:21`
- `REQUIRE_NO_TRANSPORT_SENDS()` — `tests/test_link.cpp:35 FAIL: expected 0 TransportSend(s), got 1`
- one millisecond early, required `{Usb, 20}` at release + `DEBOUNCE_MS − 1` — `tests/test_link.cpp:32 FAIL: expected 1 TransportSend(s), got 0`
- wrong order, sent 5 then 20, required 20 then 5 — `tests/test_link.cpp:55 FAIL: transport send[0] Usb:5 != Usb:20`

Defects introduced in the firmware, test left correct:
- `midi_out` sends the cue on USB and on BLE — `tests/test_link.cpp:35 FAIL: expected 1 TransportSend(s), got 2`, list `Usb:20 Ble:20`. This is the criterion 97 defect.
- `midi_out` writes status `0xB0` — `tests/harness/harness.cpp:90 FAIL: USB wire at t=40 ms: status byte is not a Note On: 0xb0 0x14 0x1`
- `midi_out` writes two bytes of the message — `tests/harness/harness.cpp:117 FAIL: USB wire at t=40 ms: ended inside a MIDI message: 0x90 0x14`
- `loop_dispatch` does not forward events to `link_on_event` — `tests/test_link.cpp:63 FAIL: REQUIRE failed: link_ready()`
- `loop_dispatch` does not forward requests to the transport — `tests/test_link.cpp:131 FAIL: expected 1 LinkRequest(s), got 0`
- `link_on_event` ignores `Lost` — `tests/test_link.cpp:69 FAIL: REQUIRE failed: !link_ready()`
- `link_on_request` drops the request — `tests/test_link.cpp:131 FAIL: expected 1 LinkRequest(s), got 0`
- the no-radio stub does not answer — `tests/test_ble_stub.cpp:30 FAIL: ble_midi_poll_event(&event)`; it answers every poll — `tests/test_ble_stub.cpp:32 FAIL: !ble_midi_poll_event(&event)`

BLE packets the parser must reject, seeded with `harness_debug_ble_packet`:
- four bytes — `tests/harness/harness.cpp:314 FAIL: BLE wire at t=0 ms: packet is not 5 bytes (PRD §9.3.2): 0x80 0x80 0x90 0x5`
- header bit 7 clear — `tests/harness/harness.cpp:317 FAIL: BLE wire at t=0 ms: header byte is not 10xxxxxx: 0x0 0x80 0x90 0x5 0x1`
- timestamp bit 7 clear — `tests/harness/harness.cpp:320 FAIL: BLE wire at t=0 ms: timestamp byte has bit 7 clear: 0x80 0x0 0x90 0x5 0x1`
- status `0xB0` — `tests/harness/harness.cpp:90 FAIL: BLE wire at t=0 ms: status byte is not a Note On: 0xb0 0x5 0x1`
- note with bit 7 set — `tests/harness/harness.cpp:93 FAIL: BLE wire at t=0 ms: data byte has bit 7 set: 0x90 0x85 0x1`

Capture overflow: 86 direct `midi_out_send` calls with no pipeline pass — `tests/harness/harness.cpp:102 FAIL: USB wire capture overflowed before it was drained`.

A wire-parse failure is reported at a `harness.cpp` line, not the test's line, because it is raised inside the pipeline pass. The message carries the timestamp.

**`harness_reset()` resets every module unconditionally.** `buttons`, `sequencer`, and `ui` reset themselves only when time runs backwards, so a `harness_reset()` called while the clock was already at 0 left them as they were. James asked for this to be fixed here. `buttons_reset()`, `sequencer_reset()`, and `ui_reset()` are host-only (`#ifdef HOST_TEST`), the same pattern as `link_reset()` and `midi_out_reset()`. Each calls the module's existing backwards-time reset and zeroes its `g_last_now`. `harness_reset()` calls them in that order, after releasing the pins and before `midi_out_reset()`. No target code changed: an incremental rebuild that recompiled the three files produced byte-identical `.uf2` files on both boards (SHA-256 before and after: pico2 `ddb01c10…`, pico2_w `30fa0df0…`).

`tests/test_reset.cpp` (`harness_reset_unconditional`) leaves state dirty at t=0 without moving the clock, calls `harness_reset()`, and asserts power-up behaviour. Three blocks:

1. Button 10 down at the very first scan, which `buttons` takes as already accepted. After the reset it must not be accepted and no tap may appear `DEBOUNCE_MS` later.
2. `buttons_set_setup_active(true)`. After the reset a hold of button 1 must still emit `Hold` at `HOLD_MS`.
3. `ui_indicate_channel(8)`. After the reset LED 8 and the panel lamp must stay off through the blink's lead gap.

**The test was shown to fail without the new resets and pass with them.** Before the calls were added to `harness_reset()`: `tests/test_reset.cpp:23 FAIL: REQUIRE failed: !buttons_accepted_pressed(10)` (line 22 after an unused include was removed). After: exit 0. Then, with the calls in place, each was taken out again by script and restored (SHA-256 checked):

- none of the three — `tests/test_reset.cpp:22 FAIL: REQUIRE failed: !buttons_accepted_pressed(10)`
- none of the three, block 1 not dirtied so block 2 is reached — `FAIL: expected 1 ButtonEvent(s), got 0` at the `REQUIRE_BUTTON_EVENTS` of block 2 (line 41 as committed; the edited file printed 38)
- none of the three, blocks 1 and 2 not dirtied so block 3 is reached — `FAIL: switch LEDs {8} (0010000000) != {} (0000000000)` at the first `REQUIRE_NO_SWITCH_LEDS` of block 3 (line 51 as committed; the edited file printed 47)
- `buttons_reset()` alone removed — `tests/test_reset.cpp:22 FAIL: REQUIRE failed: !buttons_accepted_pressed(10)`
- `ui_reset()` alone removed — `tests/test_reset.cpp:51 FAIL: switch LEDs {8} (0010000000) != {} (0000000000)`
- `buttons_reset()` and `sequencer_reset()` removed, block 1 not dirtied — the block 2 failure again. Either of the two clears the setup flag; with both gone the hold is swallowed.
- **`sequencer_reset()` alone removed — the test still passes.** No test can catch this one. Every change to the sequencer's state comes from a `ButtonEvent`, and no `ButtonEvent` can exist at t=0: an accepted edge needs `DEBOUNCE_MS` to elapse. The sequencer has no public mutator. So it cannot be left dirty at t=0, and before this change it was not actually affected by the gap. `sequencer_reset()` is called for symmetry and is covered only through block 2.

The existing 25 tests did not change outcome with the unconditional resets.

`clang-format` is clean on every source file. `clang-tidy -p build-host` is clean on every `src/**/*.cpp` and on `tests/harness/harness.cpp`, `tests/test_link.cpp`, `tests/test_reset.cpp`, and `tests/test_ble_stub.cpp`. `ble_midi_stub.cpp` is in `build-host/compile_commands.json` through `test_ble_stub`.

**Tried and abandoned:**
- Dirtying the reset test with a button pressed at t=0 after a first `harness_reset()`, or by entering setup. Neither can work. A press at t=0 is only a debounce candidate, and `harness_reset()` releases the pin, so the candidate flips back on the same pass with or without `buttons_reset()`. Setup needs `CHORD_HOLD_MS` of clock. The test uses a button down at the first scan and a direct `buttons_set_setup_active(true)` instead.
- The first proposal had `midi_out` hand the harness a decoded note (`midi_out_poll_usb_send(uint8_t* note)`). James required the harness to parse the wire bytes itself, so the seam became a raw byte copy.
- `std::deque` for the fake transport's event queue: `cert-err58-cpp` rejects it as a static. A `std::vector` is used.
- The negative-control script edits `src/` files and restores them. One run overlapped the two firmware builds, so both targets were rebuilt afterwards, alone, from the restored sources. The build results recorded above are from a later clean build of each target, pico2 then pico2_w, with no script running.

**Contradicts PRD:**
None. The §12 file tree was amended in this commit: `ble_midi.h` is listed as the boundary on all builds, and `ble_midi_stub.cpp` and the host fake were added.

**Questions raised:** None.

**Defects noted in earlier tasks (not fixed):**
None. T30's hardware bullets (criterion 102 and the onboard-LED half of 111) were run by James on 2026-11-04 against firmware tip `0ccda5e`: PASS on both boards. Recorded in `docs/VALIDATION.md`.

**Defects in earlier work fixed in this commit, at James's request:**
- `docs/TASK-LIST.md` had no `Status: complete` line under T30 or T31. `docs/LOOP-PROMPT.md`'s precondition requires T30, T31, and T32 to be marked complete there. All three now carry the line.
- `docs/LOOP-PROMPT.md` said new questions start at Q033. Q033 was used by T30. It now says Q034.
- `docs/TEST-HARNESS.md` listed `PairChordHold` in `ButtonEventKind` before the code has it. The doc line now says T35 adds it.
- `harness_reset()` did not reset `buttons`, `sequencer`, or `ui` when the clock was already at 0. Fixed and tested as described above.

## T33 — ble-midi-packet — 2026-11-10

**Status:** complete
**Branch:** task/T33-ble-midi-packet
**Commit:** (this commit)
**Criteria covered:** 99
**Tests:** 1 added (`ble_midi_packet`), 27 total, all green. pico2 and pico2_w `.uf2` produced, no compiler warnings. T10–T13 still pass (`led_pattern_engine`, `led_performance_patterns`, `led_chord_progress`, `led_channel_blink`), so criteria **41–56 still pass**. T26–T27 still pass (`switch_led_engine`, `switch_led_setup_chord`). `harness_lamp_trace()`, `harness_switch_trace()`, and every existing matcher are unchanged. No existing test file was edited.

**Done:**
`src/midi/ble_midi_packet.h/.cpp` encodes one Note On, and one Note Off, into the five-byte §9.3.2 layout from `(note, channel, now_ms)`. Header byte is `10` plus timestamp bits 12..7. Timestamp byte has bit 7 set and carries bits 6..0. Status is `0x90 | (channel - 1)` for Note On and `0x80 | (channel - 1)` for Note Off. Note On velocity is `MIDI_VELOCITY`. The low 13 bits of `now_ms` are taken with `BLE_MIDI_TIMESTAMP_MASK`, so `0x2000` wraps to `0`. No BTstack, cyw43, or TinyUSB include. `midi_out` does not call the encoder yet; that is T34.

`BLE_MIDI_TIMESTAMP_MASK` (`0x1FFF`) was added to `config.h`. The PRD §13 block already names it. It was not in the header; T32 recorded that it added no constants.

**Tests assert:** the criterion 99 worked example `B5 BC 92 05 01`; the wrap bracket `0x1FFF` / `0x2000` / `0x2001`; channels 1 and 10; every note 0–32 and every `CUE_TABLE` note; bit 7 set and header bit 6 clear across every timestamp from 0 through `0x2000` and a set of values above the mask, with the 13 timestamp bits recovered from the two framing bytes. Note Off is locked at the same timestamp bytes, status `0x82` for channel 3, velocity 0. `SEND_NOTE_OFF` remains false.

**Tried and abandoned:**
Putting `BLE_MIDI_TIMESTAMP_MASK` in the existing `constexpr` alignment group made `clang-format` want to reflow that whole group. It sits in its own group, after `LED_PANEL_GPIO`, so the surrounding constants stay byte-for-byte.

**Contradicts PRD:**
None.

**Questions raised:**
Q034 — [ASSUMED]. Note Off velocity is 0, matching the USB Note Off in `midi_out`. §9.3.2 does not name it.

**Defects noted in earlier tasks (not fixed):**
`docs/QUESTIONS.md` still says new Phase 10 questions start at Q033. T32 moved that sentence in `docs/LOOP-PROMPT.md` to Q034, which is the number used here.

## T34 — transport-arbitration — 2026-11-10

**Status:** complete
**Branch:** task/T34-transport-arbitration
**Commit:** (this commit)
**Criteria covered:** 97, 98 (transport half)
**Tests:** 1 added (`transport_arbitration`), 28 total, all green. pico2 and pico2_w `.uf2` produced, no compiler warnings. T10–T13 still pass, so criteria **41–56 still pass**. T26–T27 still pass. `harness_lamp_trace()`, `harness_switch_trace()`, and the link and transport matchers are unchanged.

**Done:**
`transport_select` is the §9.4 table: USB mounted wins, otherwise BLE when `link_ready()`, otherwise `None`. `midi_out_send` calls it per cue. USB still writes the T16 three-byte Note On (and the Note Off only when `SEND_NOTE_OFF` is true). BLE hands `ble_midi_encode_note_on` to `ble_midi_send`. `None` returns without writing either wire.

The BLE timestamp is the loop's `now` at resolution. `loop_dispatch(uint32_t now_ms)` and `midi_out_send(uint8_t note, uint32_t now_ms)` take it. The host pipeline passes `g_now_ms`. The target loop passes the same `now` it already gave `sequencer_tick`. `tests/test_midi_out.cpp` passes `0`; its USB byte assertions are unchanged. The pairing-window half of criterion 98 is T36: the window still never opens.

**Tests assert:** the four policy combinations, directly and through a tap of button 10; the release edge at `DEBOUNCE_MS − 1` and the exact millisecond, on USB and on BLE; USB bytes `0x90`, note, `MIDI_VELOCITY` with an empty BLE capture when USB wins, including while BLE is ready; the encoder's five bytes and no USB bytes when BLE wins, on channel 1 and channel 10; a resolved cue with neither transport ready appears in `REQUIRE_COMMANDS` and not in `REQUIRE_TRANSPORT_SENDS`; two cues in a row, and a prefix-then-suffix, with USB unplugged or plugged between them; two identical BLE cues with different timestamps; a hold bracketed at `HOLD_MS − 1` / `HOLD_MS` on BLE, then a press during lockout that sends nothing; setup entry bracketed at `CHORD_HOLD_MS − 1` / `CHORD_HOLD_MS`, a press during the chord lockout, a channel tap, and a cue after exit that is sent again.

**Tried and abandoned:**
None.

**Contradicts PRD:**
None.

**Questions raised:** None.

**Defects noted in earlier tasks (not fixed):**
None.

## T35 — pair-chord-detection — 2026-11-10

**Status:** complete
**Branch:** task/T35-pair-chord-detection
**Commit:** (this commit)
**Criteria covered:** 84, 85, 86, 87
**Tests:** 1 added (`pair_chord_detection`), 29 total, all green. pico2 and pico2_w `.uf2` produced, no compiler warnings. T05–T09 still pass (`tap_hold_classification`, `chord_detection`, `hold_lockout`, `sequence_state_machine`, `setup_mode_state`). T10–T13 still pass, so criteria **41–56 still pass**. T26–T27 still pass.

**Done:**
While setup is active, `input` applies the §6.5 rules to buttons `PAIR_CHORD_BUTTON_A` and `PAIR_CHORD_BUTTON_B` (2 and 4). Any overlap sets `pair_overlap` on both for the rest of those presses, so neither tap selects a channel. The timer starts at the second press's `press_time`, needs `PAIR_CHORD_HOLD_MS` (2000) of continuous overlap, clears on any release, and restarts in full on re-form. At the threshold one `ButtonEventKind::PairChordHold` is emitted. It is not emitted outside setup. Leaving setup clears an armed timer. A contact bounce that never passes debounce does not clear it.

The sequencer, only in `SETUP`, turns that event into one `BEGIN_PAIRING` on `sequencer_poll_link_request`. `loop_dispatch` forwards it to `link_on_request` and then to the transport in the same pass. No MIDI is sent. The window itself is still T36: `link_on_request` still forwards the request unchanged.

**Tests assert:** criterion 86 first — outside setup, 2 then 4 at +100 ms produces note 28 on USB at `HOLD_MS`, nothing at `HOLD_MS − 1`, and no pairing request through `100 + PAIR_CHORD_HOLD_MS` or after both are released. In setup, nothing at `PAIR_CHORD_HOLD_MS − 1` and one `BEGIN_PAIRING` at the threshold, with no second request on the next millisecond and no command on either wire. An abort at 1500 ms selects neither channel; a later clean tap of 2 does. A release at 1000 ms and re-press at 1500 ms is silent at the original deadline and at `3500 − 1`, and requests at 3500. A 20-edge bounce on button 4 does not move that deadline.

**Tried and abandoned:**
None.

**Contradicts PRD:**
None.

**Questions raised:** None.

**Defects noted in earlier tasks (not fixed):**
None.

