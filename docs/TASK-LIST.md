# TASK-LIST — USB MIDI Foot Controller v1

Companion to `PRD.md`. Every task below is scoped to be completed and committed independently.

---

## How to use this file

Work tasks **in ID order**. Do not start a task whose dependencies are unmet.

Each task specifies:

- **Branch** — create from `master`, exact name given.
- **Commit** — exact commit message. Do not improvise wording.
- **Verify** — `host` (automated test), `build` (compiles clean), or `hardware` (human, on-device).
- **Covers** — acceptance criteria numbers from PRD §14 that this task satisfies.
- **Done when** — the objective completion condition.

### Git workflow, per task

Dependencies are linear, so branches **stack**. Branch each task from its predecessor's branch, not from `master`.

**Exception — T04 branches from `master`.** T01, T02, and T03 were completed interactively and have already been merged, and `master` additionally carries `.github/workflows/ci.yml`, which T03's branch does not. Branching T04 from T03's branch would leave the entire stack without CI. From T05 onward, stacking resumes normally: T05 from T04's branch, T06 from T05's, and so on.

```
git checkout task/T04-debounce          # the predecessor's branch
git checkout -b task/T05-tap-hold-classification
# ...work...
git add -A
git commit -m "T05: implement tap and hold classification with per-button hold capability"
git push -u origin task/T05-tap-hold-classification
```

One task, one branch, one commit. Squash work-in-progress before pushing. **Never merge to `master`** — that is a human decision. Open a draft PR against the predecessor's branch and continue to the next task.

### Progress logging

After each task, append a dated entry to `PROGRESS.md`: task ID, what was done, what was tried and abandoned, and anything that contradicted the PRD. Append only. Never rewrite or delete prior entries.

---

## Guardrails — read before starting

**Out of scope. Do not implement, do not research, do not prepare for:**

- **PRD §5.2 Tier 2 hardening.** No OTP fuse operations. No secure boot signing. No disabling of the UF2 bootloader or SWD debug. These operations are **irreversible and will permanently brick the board**. If any task appears to require them, stop and ask. Tier 1 (§5.1) is fully in scope and is the whole of the security work for v1.
- Display / I2C driver (v2). Pins are reserved only.
- Bluetooth, WiFi, BLE MIDI (v3).
- Expression pedal / ADC input.
- Runtime-editable cue mappings, preset banks.
- MIDI channels 11–16.

**Stop and ask rather than guess if:**

- A PRD requirement appears self-contradictory.
- A task needs a decision listed in PRD §15 (Open questions).
- A host test cannot be written for something marked `Verify: host`.
- You are about to change `config.h` values that the PRD specifies explicitly.

**Never attempt to verify a task marked `Verify: hardware`.** Those require physical footswitches, a scope or slow-motion camera, and a MIDI host. Mark them blocked and move on. Looping on them will not converge.

---

## Bootstrap — do these interactively, before starting the loop

Three tasks are not suitable for unattended work. **The agent still writes the code** — these are supervised, not hand-written. Run them in a normal Cursor agent session where you can answer questions in real time and verify the specific things listed below, then log them in `PROGRESS.md` in the usual format so the loop knows to start at T04.

| Task | Why not the loop |
|---|---|
| **T01** | Toolchain setup is environment-specific — SDK path, submodule pin, board file. An agent can confirm a `.uf2` was produced but not that it is a valid one. Failures here are silent and poison everything downstream. |
| **T02** | Mechanical, but it is the single source of truth for all 33 cues. A transcription error survives the compile-time assertion and, worse, the loop would then write tests *from* `config.h` rather than the PRD — the error becomes self-consistent and invisible. Mitigated by the PRD-parsing test required in T02's completion criteria. |
| **T03** | The harness is the loop's only feedback signal, and its API shape constrains all eighteen downstream tasks. A subtly broken harness means hours of firmware written blind and reported green — hence the negative control in its completion criteria. |

The loop must refuse to start until all three are complete and `ctest` is green.

---

## Phase 0 — Scaffolding

### T01 — repo-skeleton
- **Branch:** `task/T01-repo-skeleton`
- **Commit:** `T01: add Pico SDK project skeleton and CMake build for pico2`
- **Depends on:** —
- **Mode:** **interactive — human-led, not the loop**
- **PRD:** §4, §12
- **Verify:** build
- **Covers:** —
- **Done when:** Directory tree matches PRD §12 exactly, with empty stub headers/sources. `cmake -B build -DPICO_BOARD=pico2 && ninja -C build` produces a `.uf2` from a `main.cpp` that does nothing but spin. `pico_stdlib`, `tinyusb_device`, `hardware_flash`, and `hardware_sync` are linked. A `DEBUG_UART` CMake option exists and defaults to `OFF`.

  **Two settings must live inside `CMakeLists.txt`, not on the command line:**
  - `set(PICO_BOARD pico2)` — a forgotten flag would otherwise silently build an RP2040 binary that fails in confusing ways much later.
  - `set(PICOTOOL_FETCH_FROM_GIT_PATH $ENV{HOME}/pico/picotool-build)` — points at the already-built picotool 2.3.1. Without this the configure step fails or rebuilds picotool from scratch.

  **SDK bootstrap.** Copy `pico_sdk_import.cmake` from `$PICO_SDK_PATH/external/` into the repo root and include it before `project()`. Call `pico_sdk_init()` after `project()`. Link **`pico_stdlib`** alongside the others — it supplies crt0, the linker script, and the runtime, without which the output is not a valid UF2. Call `pico_add_extra_outputs()` on the target or no `.uf2` is produced.

  **Fail loudly on a bad SDK.** `CMakeLists.txt` must `FATAL_ERROR` if `PICO_SDK_PATH` is unset, and must assert `PICO_SDK_VERSION_STRING` is at least `2.3.1`. The SDK is external (see `AGENTS.md`), so the repo cannot pin it — this check is the substitute. A silently older SDK would produce a subtly wrong RP2350 binary.

  **TinyUSB needs a config header to link.** Linking `tinyusb_device` compiles TinyUSB sources, which unconditionally `#include "tusb_config.h"`. Create `src/midi/tusb_config.h` containing an include guard, a comment marking it a T01 stub owned by T15, and an explicit `#define CFG_TUD_ENABLED 0`. Do not leave it empty — relying on TinyUSB's internal defaults is version-dependent and records no intent. Point `target_include_directories` at `src/midi` so TinyUSB resolves it. The device stack stays off until T15; nothing in T01 may call `tud_task()`.

  **Configuration files** `.gitignore`, `.clang-format`, and `.clang-tidy` are present at the repo root and committed. Do not rewrite or regenerate them — they define the verification bar in `AGENTS.md` and must stay stable across every task. Confirm `build/` and `build-host/` are ignored before committing.

  **Documentation files:** verify `AGENTS.md` is at the repo root and that `PRD.md`, `TASK-LIST.md`, `QUESTIONS.md`, and `PROGRESS.md` are under `docs/`. If they are already committed, do nothing — do not move, rewrite, or reformat them. Add only what is missing. `TEST-HARNESS.md` arrives later, in T03.

### T02 — config-header
- **Branch:** `task/T02-config-header`
- **Commit:** `T02: add config.h with timing constants and cue command table`
- **Depends on:** T01
- **Mode:** **interactive — human-reviewed before the loop starts**
- **PRD:** §6.2, §13
- **Verify:** build
- **Covers:** —
- **Done when:** `config.h` contains every constant from PRD §13 with the exact specified values, plus the full 33-entry cue table from §6.2 as a single `constexpr` structure indexed by trigger. No note number or button pairing appears anywhere else in the codebase. A compile-time assertion confirms 33 distinct notes covering 0–32.

  **Plus a machine check for transcription.** The distinctness assertion does not catch a mis-paired trigger — swapping two rows still yields 33 distinct notes. Add a host test that parses the markdown table in PRD §6.2 and asserts every row matches the compiled table exactly: note number, prefix button, suffix button, trigger type. This catches transcription errors now and keeps catching them if either file is edited later. Prefer this over reviewing 33 rows by eye.

---

## Phase 1 — Test harness

### T03 — host-test-harness
- **Branch:** `task/T03-host-test-harness`
- **Commit:** `T03: add host test harness with fake clock and synthetic event injection`
- **Depends on:** T02
- **Mode:** **interactive — human-led, not the loop**
- **PRD:** §12.1
- **Verify:** host
- **Covers:** —
- **Done when:** All five of the following:
  1. A second CMake target builds `input/`, `sequencer/`, and `ui/` for the host with no Pico SDK dependency.
  2. The harness provides a fake millisecond clock the test advances manually, per-button raw GPIO level assertion, and capture buffers for emitted `Command` and `UiEvent`.
  3. **The harness API is documented in `docs/TEST-HARNESS.md`** — every function the loop will call, with signatures and one usage example each.
  4. **Three exemplar tests exist, one per shape the loop will need to imitate:** a bounce-injection test, a timing/deadline test that advances the clock across a threshold, and a capture-buffer assertion test.
  5. **A negative control passes.** Deliberately break one implementation — an off-by-one in the debounce interval, say — and confirm the relevant test goes red, then revert. A harness whose tests cannot fail is worse than no harness, because it reports green for eighteen tasks in a row. This is the single most important check in the bootstrap phase.
  6. `ctest` runs green.

> **This task gates everything after it.** It is the loop's only feedback signal, and items 3 and 4 are what let the loop write consistent tests instead of inventing three incompatible styles. Do not start the loop until `ctest` is green.

---

## Phase 2 — Input layer

### T04 — debounce
- **Branch:** `task/T04-debounce`
- **Commit:** `T04: implement per-button debounce filter`
- **Depends on:** T03
- **PRD:** §11.3
- **Verify:** host
- **Covers:** 57
- **Done when:** Per-button independent debounce accepts a level change only after `DEBOUNCE_MS` of stability. Tests inject bounce trains of 5–20 transitions over 10 ms and assert exactly one press event. All downstream logic consumes debounced state only.

### T05 — tap-hold-classification
- **Branch:** `task/T05-tap-hold-classification`
- **Commit:** `T05: implement tap and hold classification with per-button hold capability`
- **Depends on:** T04
- **PRD:** §6.3
- **Verify:** host
- **Covers:** 13, 14, 15, 15b, 15c, 15d, 18
- **Done when:** Release before `HOLD_MS` (2000) emits `TAP`. Reaching `HOLD_MS` on a hold-capable button (**1–5, 8, 10**) emits `HOLD` **at the threshold, not on release**, and suppresses the subsequent release. Buttons 6, 7, and 9 held past `HOLD_MS` emit nothing and suppress their tap; the chord exception for 6 and 9 is handled in T06.

  **Button 8 is the only suffix button with a hold** (Mute MIDI, note 32). Check it is not accidentally grouped with 6, 7, and 9 in the suppression branch — test 15d asserts the other three still do nothing.

### T06 — chord-detection
- **Branch:** `task/T06-chord-detection`
- **Commit:** `T06: implement 6+9 chord detection with strict continuous hold timer`
- **Depends on:** T05
- **PRD:** §6.4 (exception clause), §6.5
- **Verify:** host
- **Covers:** 22, 23, 24, 25, 26, 27, 29, 29b
- **Done when:** Overlap of buttons 6 and 9 flags both `chord_overlap` for the remainder of their presses; neither emits a tap. The timer starts at the **second** press and requires `CHORD_HOLD_MS` of **continuous** overlap. Release of either **clears** the timer — not pauses it. Re-forming the pair starts a fresh full window. An aborted chord emits nothing at all. Buttons 6 and 9 are exempt from the T05 suppression at `HOLD_MS` while `chord_armed`. Test 24 (hold 6 for 7 s while tapping 9 partway) and test 26 (re-form) are the two that must pass; write them first.

### T07 — hold-lockout
- **Branch:** `task/T07-hold-lockout`
- **Commit:** `T07: implement hold lockout and chord lockout with phantom-tap suppression`
- **Depends on:** T06
- **PRD:** §6.4
- **Verify:** host
- **Covers:** 19, 20, 21
- **Done when:** A fired hold enters `LOCKED` keyed to that button; all other presses and releases are discarded while locked. Lockout ends on release of the locked button. **On exit, every button still physically pressed is marked tap-suppressed** so its later release emits nothing. Chord lockout ends only when both 6 and 9 are released.

---

## Phase 3 — Sequencer

### T08 — sequence-state-machine
- **Branch:** `task/T08-sequence-state-machine`
- **Commit:** `T08: implement IDLE and PENDING sequence resolution`
- **Depends on:** T07
- **PRD:** §8.1–§8.4
- **Verify:** host
- **Covers:** 4, 5, 6, 7, 8, 9, 10, 11, 12, 16, 17
- **CONSTRAINT FROM T03 — read before implementing.** `buttons_poll_event` is the drain the harness uses to capture `ButtonEvent`. If the sequencer consumes that queue without leaving a copy for the harness, every `ButtonEvent` assertion in T04–T07 goes red and the failures will look like regressions in already-finished work rather than a problem here. Either leave a copy for the test drain or route the sequencer through a separate path. Confirm the full `ctest` suite is green, not just the new tests.
- **Done when:** Taps of 1–5 enter `PENDING`. A pending prefix resolves against any of 6–9 or itself. A different prefix reassigns without emitting. A tap of 10 fires Repeat and clears any pending. Orphan suffixes are ignored. `SEQUENCE_TIMEOUT_MS` expiry is silent. The self-pair check must precede prefix reassignment — test 6 (`3, 3` → note 24) catches the wrong ordering.

### T09 — setup-mode-state
- **Branch:** `task/T09-setup-mode-state`
- **Commit:** `T09: implement MIDI channel setup mode state and exit paths`
- **Depends on:** T08
- **PRD:** §7.1–§7.4
- **Verify:** host
- **Covers:** 28, 30, 31, 32, 33, 35, 38
- **Done when:** A completed chord discards any pending sequence and enters `SETUP` via chord lockout. **All cue output is suppressed in `SETUP`.** Taps of 1–10 set `pending_channel` to 1–10. Holds do nothing. A 6+9 overlap-then-release exits **unconditionally, whether or not a channel was selected**. `SETUP_TIMEOUT_MS` inactivity exits and commits. Channel selection is RAM-only; nothing touches flash in this task.

---

## Phase 4 — LED indication

### T10 — led-pattern-engine
- **Branch:** `task/T10-led-pattern-engine`
- **Commit:** `T10: implement non-blocking LED pattern engine with priority stack`
- **Depends on:** T09
- **PRD:** §11.4.1, §12.2
- **Verify:** host
- **Covers:** 60
- **Done when:** A tick-driven state machine resolves the 7-level priority stack and outputs a boolean lamp state per tick. **No `sleep_ms` anywhere** — a blocking pattern would stall `tud_task()` and drop USB. Host tests advance the fake clock and assert the lamp waveform. A test asserts the engine never consumes more than a bounded time per tick.

### T11 — led-performance-patterns
- **Branch:** `task/T11-led-performance-patterns`
- **Commit:** `T11: implement pending, lockout, and cue confirmation LED patterns`
- **Depends on:** T10
- **PRD:** §11.4.1, §11.4.2
- **Verify:** host
- **Covers:** 45, 46, 47, 48, 49
- **Done when:** `PENDING` slow-flashes at a 1000 ms period. Lockout fast-flashes at 100 ms. Every transmitted cue produces exactly one `CUE_FLASH_MS` flash, **preceded by a forced `CUE_FLASH_GAP_MS` off gap whenever the lamp is currently lit**. Solid on appears nowhere in performance mode. Test 47 (two rapid taps of 10 must not merge into one pulse) is the one that catches a missing gap.

### T12 — led-chord-progress
- **Branch:** `task/T12-led-chord-progress`
- **Commit:** `T12: implement chord progress LED blackout and flash`
- **Depends on:** T11
- **PRD:** §11.4.3
- **Verify:** host
- **Covers:** 41, 42, 43, 44
- **Done when:** While `chord_armed`, the lamp is forced **hard off** for `CHORD_LED_DELAY_MS`, overriding a slow-flashing PENDING beneath it, then flashes at a 250 ms period until the chord completes. Completion goes straight to solid. Abort releases the override immediately. Re-form restarts the blackout along with the timer.

### T13 — led-channel-blink
- **Branch:** `task/T13-led-channel-blink`
- **Commit:** `T13: implement channel blink primitive and its four invocation sites`
- **Depends on:** T12
- **PRD:** §11.4.4
- **Verify:** host
- **Covers:** 50, 51, 52, 53, 55, 56
- **Done when:** A reusable blink primitive plays gap / N pulses / gap and returns to the state beneath it. Invoked at boot, on channel selection in setup, on chord exit, and on timeout exit. A new blink cancels one in progress. **Any cue send aborts an in-progress blink.** The blink never gates the state machine — a test asserts input and cue emission continue normally underneath it. Setup mode is solid throughout, including while waiting for 6 and 9 to release.

---

## Phase 5 — Target hardware

### T14 — gpio-input
- **Branch:** `task/T14-gpio-input`
- **Commit:** `T14: wire button scanning to GP6-GP15 with internal pull-ups`
- **Depends on:** T13
- **PRD:** §11.1, §11.2
- **Verify:** build + hardware
- **Covers:** —
- **Done when:** Buttons 1–10 map to GP6–GP15 and are read with a single masked `gpio_get_all()`. **Internal pull-ups only — no pull-downs anywhere**, per the RP2350 erratum noted in §11.2. Active-low. The debounce layer from T04 consumes this unchanged.

### T15 — usb-midi-descriptors
- **Branch:** `task/T15-usb-midi-descriptors`
- **Commit:** `T15: add MIDI-only USB descriptors with no CDC or MSC interfaces`
- **Depends on:** T14
- **PRD:** §5.1, §9
- **Verify:** build + hardware
- **Covers:** 1, 2
- **Done when:** Exactly one USB interface is declared: MIDI, one virtual cable, one IN endpoint. **No CDC, no MSC, no vendor interface.**

  **T15 owns `src/midi/tusb_config.h`**, stubbed in T01 with `CFG_TUD_ENABLED 0`. Change it to `1`, enable the MIDI device class, and size the endpoint buffers. The stub comment marking it T01-owned must be removed.

  Every descriptor value comes from the table in **PRD §9.1** — VID, PID, `bcdDevice`, all three string descriptors, and the MIDI embedded IN jack string. Do not invent values and do not fall back to SDK defaults. The serial is derived at runtime from `pico_get_unique_board_id_string()`, not hardcoded.

  Verify on a host that the device enumerates as `TrackStomp Navigator` with a port named `Navigator Cues`, and that the serial is a stable 16-hex-character string across reboots. No build paths or developer identity appear in any descriptor.

  **The PID is a placeholder (`0xFFFE`) until an allocation is merged into `raspberrypi/usb-pid`.** Note this in `PROGRESS.md` so it is not forgotten.

### T16 — midi-output
- **Branch:** `task/T16-midi-output`
- **Commit:** `T16: implement MIDI note transmission with runtime-owned channel`
- **Depends on:** T15
- **PRD:** §9
- **Verify:** build + hardware
- **Covers:** 3
- **Done when:** Cues transmit as Note On, velocity 1, on the channel owned solely by the MIDI layer. No Note Off (`SEND_NOTE_OFF` is `false`). No output while in `SETUP` or `LOCKED`. No running-status optimisation. Repeated identical cues are always sent.

### T17 — led-driver
- **Branch:** `task/T17-led-driver`
- **Commit:** `T17: drive the panel and onboard status LEDs from the pattern engine`
- **Depends on:** T16
- **PRD:** §11.1, §11.4
- **Verify:** build + hardware
- **Covers:** —
- **Done when:** The T10 engine's per-tick lamp state drives **both GP16 (panel, via the NPN switch of PRD §11.2) and GP25 (onboard mirror)** from the same value, with **no inversion** — the low-side NPN is non-inverting, so high means lit on both pins. Pad drive strength on GP16 is set to 4 mA. Thin shim only, no pattern logic in this layer.

  On hardware, confirm both LEDs track each other exactly through a full Setup Mode entry, channel blink, and exit.

---

## Phase 6 — Persistence

### T18 — config-store-read
- **Branch:** `task/T18-config-store-read`
- **Commit:** `T18: implement config sector read with CRC validation and channel 1 fallback`
- **Depends on:** T17
- **PRD:** §10.1, §10.2
- **Verify:** host + hardware
- **Covers:** 39, 40, 54
- **Done when:** The last 4 KB flash sector is reserved and excluded from the linker's image region. `ConfigRecord` matches §10.1. Boot validates magic, version, CRC32, and channel range; any failure falls back to channel 1. **Nothing is ever written at boot** — a factory-fresh unit with erased flash reads as channel 1 and leaves the sector untouched. Host tests cover blank, corrupt-CRC, bad-magic, and out-of-range channel.

### T19 — config-store-write
- **Branch:** `task/T19-config-store-write`
- **Commit:** `T19: implement config sector write on setup exit with no-op skip`
- **Depends on:** T18
- **PRD:** §7.3, §10.3, §10.4
- **Verify:** hardware
- **Covers:** 36, 37
- **Done when:** Writes occur only on setup exit and **only when the channel changed**. Erase and program functions are marked `__not_in_flash_func` and run with interrupts disabled via `save_and_disable_interrupts()`. No flash write exists on any performance path. The no-op skip is verified with an instrumented debug build.

---

## Phase 7 — Integration

### T20 — main-loop-integration
- **Branch:** `task/T20-main-loop-integration`
- **Commit:** `T20: wire all modules into the cooperative main loop`
- **Depends on:** T19
- **PRD:** §12.1, §12.2
- **Verify:** build + hardware
- **Covers:** —
- **Done when:** The loop matches §12.2 exactly, with `tud_task()` first on every iteration. Layer rules hold: the sequencer calls no GPIO, MIDI, or flash function directly; lockout and chord logic live in `input`; `config_store` is called only at boot and on setup exit. Core 1 is unused.

### T21 — release-build-hardening
- **Branch:** `task/T21-release-build-hardening`
- **Commit:** `T21: enforce Tier 1 release build with no serial console`
- **Depends on:** T20
- **PRD:** §5.1
- **Verify:** build + hardware
- **Covers:** 2
- **Done when:** Release builds have no USB CDC and no stdio. `DEBUG_UART` gates UART-only stdio on GP0/GP1 and defaults to `OFF`. A grep of the built binary finds no absolute source paths. **Tier 2 (§5.2) remains unimplemented — see Guardrails.**

---

## Phase 8 — Hardware validation

Human-executed. **Do not loop on these.** Each requires physical switches, a MIDI host, and a scope or slow-motion camera.

### T22 — bench-validation
- **Branch:** `task/T22-bench-validation`
- **Commit:** `T22: record bench validation results against PRD acceptance criteria`
- **Depends on:** T21
- **Verify:** hardware
- **Covers:** all of 1–56, re-verified on target
- **Done when:** `VALIDATION.md` records pass/fail for every criterion in PRD §14 with the date and firmware commit. Priority order for the ones that only fail on real hardware: 29 (bounce must not clear the chord timer), 41–44 (LED timing), 47 (flash separation), 24 (the strict chord rule).

### T23 — soak-test
- **Branch:** `task/T23-soak-test`
- **Commit:** `T23: record 30-minute soak test results`
- **Depends on:** T22
- **Verify:** hardware
- **Covers:** 58, 59
- **Done when:** 30 minutes of randomised input including partial chord attempts produces no hang, no unexpected notes, and no stuck LED state. Results appended to `VALIDATION.md`.

---

## Loop exit condition

The loop is done with the automated portion when **all of T01–T21 are committed and pushed, and `ctest` is green covering acceptance criteria 4–56 minus the hardware-only ones**.

Criteria requiring hardware and therefore excluded from the loop's exit condition: **1, 2, 3, 36, 37, 39 (on-target confirmation), 57, 58, 59**, plus on-target re-verification of everything in T22.

Do not attempt to satisfy those. Report them as blocked-on-hardware and stop.
