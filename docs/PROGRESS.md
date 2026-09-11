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

