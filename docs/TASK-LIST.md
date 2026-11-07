# TASK-LIST — USB MIDI Foot Controller

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
- Display / I2C driver. **Superseded by PRD §11.5** — GP4/GP5 now carry status LEDs. A display is not planned (PRD §16).
- WiFi, network MIDI, Bluetooth Classic, and any BLE service other than BLE-MIDI. **BLE MIDI itself is in scope from Phase 10 (T30–T39) only**, exactly as PRD §5.4 and §9.3 define it.
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
- **Status:** complete
- **Branch:** `task/T04-debounce`
- **Commit:** `T04: implement per-button debounce filter`
- **Depends on:** T03
- **PRD:** §11.3
- **Verify:** host
- **Covers:** 57
- **Done when:** Per-button independent debounce accepts a level change only after `DEBOUNCE_MS` of stability. Tests inject bounce trains of 5–20 transitions over 10 ms and assert exactly one press event. All downstream logic consumes debounced state only.

### T05 — tap-hold-classification
- **Status:** complete
- **Branch:** `task/T05-tap-hold-classification`
- **Commit:** `T05: implement tap and hold classification with per-button hold capability`
- **Depends on:** T04
- **PRD:** §6.3
- **Verify:** host
- **Covers:** 13, 14, 15, 15b, 15c, 15d, 18
- **Done when:** Release before `HOLD_MS` (2000) emits `TAP`. Reaching `HOLD_MS` on a hold-capable button (**1–5, 8, 10**) emits `HOLD` **at the threshold, not on release**, and suppresses the subsequent release. Buttons 6, 7, and 9 held past `HOLD_MS` emit nothing and suppress their tap; the chord exception for 6 and 9 is handled in T06.

  **Button 8 is the only suffix button with a hold** (Mute MIDI, note 32). Check it is not accidentally grouped with 6, 7, and 9 in the suppression branch — test 15d asserts the other three still do nothing.

### T06 — chord-detection
- **Status:** complete
- **Branch:** `task/T06-chord-detection`
- **Commit:** `T06: implement 6+9 chord detection with strict continuous hold timer`
- **Depends on:** T05
- **PRD:** §6.4 (exception clause), §6.5
- **Verify:** host
- **Covers:** 22, 23, 24, 25, 26, 27, 29, 29b
- **Done when:** Overlap of buttons 6 and 9 flags both `chord_overlap` for the remainder of their presses; neither emits a tap. The timer starts at the **second** press and requires `CHORD_HOLD_MS` of **continuous** overlap. Release of either **clears** the timer — not pauses it. Re-forming the pair starts a fresh full window. An aborted chord emits nothing at all. Buttons 6 and 9 are exempt from the T05 suppression at `HOLD_MS` while `chord_armed`. Test 24 (hold 6 for 7 s while tapping 9 partway) and test 26 (re-form) are the two that must pass; write them first.

### T07 — hold-lockout
- **Status:** complete
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
- **Status:** complete
- **Branch:** `task/T08-sequence-state-machine`
- **Commit:** `T08: implement IDLE and PENDING sequence resolution`
- **Depends on:** T07
- **PRD:** §8.1–§8.4
- **Verify:** host
- **Covers:** 4, 5, 6, 7, 8, 9, 10, 11, 12, 16, 17
- **CONSTRAINT FROM T03 — read before implementing.** `buttons_poll_event` is the drain the harness uses to capture `ButtonEvent`. If the sequencer consumes that queue without leaving a copy for the harness, every `ButtonEvent` assertion in T04–T07 goes red and the failures will look like regressions in already-finished work rather than a problem here. Either leave a copy for the test drain or route the sequencer through a separate path. Confirm the full `ctest` suite is green, not just the new tests.
- **Done when:** Taps of 1–5 enter `PENDING`. A pending prefix resolves against any of 6–9 or itself. A different prefix reassigns without emitting. A tap of 10 fires Repeat and clears any pending. Orphan suffixes are ignored. `SEQUENCE_TIMEOUT_MS` expiry is silent. The self-pair check must precede prefix reassignment — test 6 (`3, 3` → note 24) catches the wrong ordering.

### T09 — setup-mode-state
- **Status:** complete
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
- **Status:** complete
- **Branch:** `task/T10-led-pattern-engine`
- **Commit:** `T10: implement non-blocking LED pattern engine with priority stack`
- **Depends on:** T09
- **PRD:** §11.4.1, §12.2
- **Verify:** host
- **Covers:** 60
- **Done when:** A tick-driven state machine resolves the 7-level priority stack and outputs a boolean lamp state per tick. **No `sleep_ms` anywhere** — a blocking pattern would stall `tud_task()` and drop USB. Host tests advance the fake clock and assert the lamp waveform. A test asserts the engine never consumes more than a bounded time per tick.

### T11 — led-performance-patterns
- **Status:** complete
- **Branch:** `task/T11-led-performance-patterns`
- **Commit:** `T11: implement pending, lockout, and cue confirmation LED patterns`
- **Depends on:** T10
- **PRD:** §11.4.1, §11.4.2
- **Verify:** host
- **Covers:** 45, 46, 47, 48, 49
- **Done when:** `PENDING` slow-flashes at a 1000 ms period. Lockout fast-flashes at 100 ms. Every transmitted cue produces exactly one `CUE_FLASH_MS` flash, **preceded by a forced `CUE_FLASH_GAP_MS` off gap whenever the lamp is currently lit**. Solid on appears nowhere in performance mode. Test 47 (two rapid taps of 10 must not merge into one pulse) is the one that catches a missing gap.

### T12 — led-chord-progress
- **Status:** complete
- **Branch:** `task/T12-led-chord-progress`
- **Commit:** `T12: implement chord progress LED blackout and flash`
- **Depends on:** T11
- **PRD:** §11.4.3
- **Verify:** host
- **Covers:** 41, 42, 43, 44
- **Done when:** While `chord_armed`, the lamp is forced **hard off** for `CHORD_LED_DELAY_MS`, overriding a slow-flashing PENDING beneath it, then flashes at a 250 ms period until the chord completes. Completion goes straight to solid. Abort releases the override immediately. Re-form restarts the blackout along with the timer.

### T13 — led-channel-blink
- **Status:** complete
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
- **Status:** complete
- **Branch:** `task/T14-gpio-input`
- **Commit:** `T14: wire button scanning to GP6-GP15 with internal pull-ups`
- **Depends on:** T13
- **PRD:** §11.1, §11.2
- **Verify:** build + hardware
- **Covers:** —
- **Done when:** Buttons 1–10 map to GP6–GP15 and are read with a single masked `gpio_get_all()`. **Internal pull-ups only — no pull-downs anywhere**, per the RP2350 erratum noted in §11.2. Active-low. The debounce layer from T04 consumes this unchanged.

### T15 — usb-midi-descriptors
- **Status:** complete
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
- **Status:** complete
- **Branch:** `task/T16-midi-output`
- **Commit:** `T16: implement MIDI note transmission with runtime-owned channel`
- **Depends on:** T15
- **PRD:** §9
- **Verify:** build + hardware
- **Covers:** 3
- **Done when:** Cues transmit as Note On, velocity 1, on the channel owned solely by the MIDI layer. No Note Off (`SEND_NOTE_OFF` is `false`). No output while in `SETUP` or `LOCKED`. No running-status optimisation. Repeated identical cues are always sent.

### T17 — led-driver
- **Status:** complete
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
- **Status:** complete
- **Branch:** `task/T18-config-store-read`
- **Commit:** `T18: implement config sector read with CRC validation and channel 1 fallback`
- **Depends on:** T17
- **PRD:** §10.1, §10.2
- **Verify:** host + hardware
- **Covers:** 39, 40, 54
- **Done when:** The last 4 KB flash sector is reserved and excluded from the linker's image region. `ConfigRecord` matches §10.1. Boot validates magic, version, CRC32, and channel range; any failure falls back to channel 1. **Nothing is ever written at boot** — a factory-fresh unit with erased flash reads as channel 1 and leaves the sector untouched. Host tests cover blank, corrupt-CRC, bad-magic, and out-of-range channel.

### T19 — config-store-write
- **Status:** complete
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
- **Status:** complete
- **Branch:** `task/T20-main-loop-integration`
- **Commit:** `T20: wire all modules into the cooperative main loop`
- **Depends on:** T19
- **PRD:** §12.1, §12.2
- **Verify:** build + hardware
- **Covers:** —
- **Done when:** The loop matches §12.2 exactly, with `tud_task()` first on every iteration. Layer rules hold: the sequencer calls no GPIO, MIDI, or flash function directly; lockout and chord logic live in `input`; `config_store` is called only at boot and on setup exit. Core 1 is unused.

### T21 — release-build-hardening
- **Status:** complete
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

# Phase 9 — Per-switch status LEDs

Added after v1 was merged. Implements PRD §11.5. **Read §11.5 in full before starting T24** — the one-indication-at-a-time rule in §11.5.3 constrains every task in this phase, and an engine that composites independent per-LED states will pass individual tests and fail criterion 78.

## Scope boundary for this phase

**The panel LED engine of §11.4 is finished work and is out of scope.** Do not modify `ui_lamp()`, its pattern engine, its tests, or its GP16/GP25 driver. Criteria 41–56 must still pass unchanged at every commit (criterion 81). The per-switch engine is a *second, independent* engine living beside it inside `ui`.

**The `UiEvent` contract does not change.** No new `UiEventKind`, no new field, no change to what any existing event carries. This is deliberate: T08–T13 assert ordered `UiEvent` lists, and inserting or altering an event would turn finished tests red for reasons that have nothing to do with a defect. The button(s) behind a cue are recovered by reverse lookup in `CUE_TABLE` (T24), not from the event stream and not by inspecting `ButtonEvent`.

**`harness_lamp_trace()` does not change.** Its signature, its meaning, and its one-sample-per-timestamp behaviour are relied on by T10–T13. The per-switch trace is a parallel API (T25).

**Branching.** T24 branches from `master` — v1 is merged, so the v1 stack is history. T25 onward stack normally, each from its predecessor's branch.

### T24 — cue-table-reverse-lookup
- **Status:** complete
- **Branch:** `task/T24-cue-table-reverse-lookup`
- **Commit:** `T24: add reverse lookup from cue note to originating buttons`
- **Depends on:** — (branch from `master`)
- **PRD:** §6.2, §11.5.4, §12.1
- **Verify:** host
- **Covers:** —
- **Done when:** A pure function in the T02 cue-table module maps a transmitted note back to the button or buttons that produced it, returning a small struct carrying the cue class (pair, self-pair, standalone, hold) and one or two button ids. **Additive only** — no existing signature changes and no existing test is touched.

  All 33 notes round-trip. Host tests assert: note 0 → pair(1, 6); note 24 → self-pair(3); note 20 → standalone(10); note 27 → hold(1); note 32 → hold(8); note 21 → hold(10). An unmapped note returns a clearly invalid result rather than a plausible wrong one, and a test asserts that.

  The table stays the single source of truth per §6.2 — the reverse direction is derived from `CUE_TABLE`, never a second hardcoded list. A test asserting that every entry in `CUE_TABLE` round-trips is the guard against that drifting.

### T25 — switch-led-harness
- **Status:** complete
- **Branch:** `task/T25-switch-led-harness`
- **Commit:** `T25: extend the host harness with per-switch LED capture`
- **Depends on:** T24
- **PRD:** §11.5.1, §11.5.3
- **Verify:** host
- **Covers:** —
- **Done when:** The harness captures per-switch LED state per processed timestamp, per `TEST-HARNESS.md`. `ui_switch_leds()` exists and returns a 10-bit mask, bit 0 being LED 1; for this task it may return a stub 0. `harness_switch_trace()`, `REQUIRE_SWITCH_LEDS(...)`, and `REQUIRE_NO_SWITCH_LEDS()` are available and documented.

  **This is the bootstrap task of the phase and it carries T03's risk: a harness whose assertions cannot fail will report green for every task after it.** Before this task is complete, prove the matcher fails — write a test that asserts the wrong mask, confirm it fails with `file:line`, then correct it. `harness_lamp_trace()` is unchanged and T10–T13 still pass.

### T26 — switch-led-engine
- **Status:** complete
- **Branch:** `task/T26-switch-led-engine`
- **Commit:** `T26: implement the per-switch indication engine with pending and confirmation states`
- **Depends on:** T25
- **PRD:** §11.5.3, §11.5.4
- **Verify:** host
- **Covers:** 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 78, 79, 80
- **Done when:** The engine implements the §11.5.3 priority stack and the §11.5.4 waveforms, driven by the existing `UiEvent` stream plus T24's reverse lookup. **One indication is visible at a time** — the engine resolves priority to a single active indication owning one or two LEDs and emits a mask with every other bit clear. It does not composite independent per-LED states.

  Cancel and override are distinct mechanisms and are implemented as such: an accepted `ButtonEvent` destroys a confirmation, while chord progress (T27) suspends and restores what sits beneath it. A press discarded by lockout cancels nothing.

  The hold envelope is a floor, not a ceiling — solid from the hold fire, for at least `LED_CONFIRM_MS`, and never ending before the button is released. Criteria 68 and 69 are the two halves of this and both must pass.

  No `sleep_ms`, no blocking, no allocation. Advanced only by its tick function.

### T27 — switch-led-setup-chord
- **Status:** complete
- **Branch:** `task/T27-switch-led-setup-chord`
- **Commit:** `T27: add setup channel, chord progress, and boot indication to the switch LEDs`
- **Depends on:** T26
- **PRD:** §11.5.5, §11.5.6, §11.5.7
- **Verify:** host
- **Covers:** 71, 72, 73, 74, 75, 76, 77
- **Done when:** Chord progress occupies priority 2 and overrides the whole surface — 500 ms of full darkness including a live pending flash, then 6 and 9 in phase at 125/125, then straight to the setup channel blink at 5000 ms. Abort restores the suspended indication on the same tick; re-form restarts the blackout.

  Setup Mode blinks the selected channel's LED at 1000/1000, starting from the **on** phase on every selection change, and continues through `LOCKED(CHORD)` until both 6 and 9 are released. Exit by either route extinguishes all ten.

  Boot lights the stored channel solid for `LED_BOOT_CHANNEL_MS`, cancellable by any press.

  Chord timing may be read via the same `buttons_chord_start()` accessor the §11.4 engine already uses. That is an existing pattern (QUESTIONS Q009), not a new one — do not add a second mechanism.

### T28 — switch-led-driver
- **Status:** complete
- **Branch:** `task/T28-switch-led-driver`
- **Commit:** `T28: drive the ten per-switch status LEDs from the indication engine`
- **Depends on:** T27
- **PRD:** §11.5.1, §11.5.2
- **Verify:** build + hardware
- **Covers:** —
- **Done when:** The engine's per-tick mask drives all ten GPIOs from `SWITCH_LED_GPIO` in a single `gpio_put_masked()` call, with **no inversion** — the low-side NPN is non-inverting, so high means lit, exactly as on GP16. Pad drive strength is 4 mA on all ten pins. The bit-position mask is built from the constant table; the pins are **not** contiguous and nothing may assume they are.

  Thin shim only. No pattern logic, no timing, no state in this layer — it mirrors T17's shape.

  `SWITCH_LED_DUTY` is wired but uniform at 255. Do not implement PWM ramping or use duty for signalling; §11.5.2 confines it to brightness matching.

### T29 — switch-led-bench
- **Branch:** `task/T29-switch-led-bench`
- **Commit:** `T29: record per-switch LED bench validation results`
- **Depends on:** T28, and T22
- **Verify:** hardware
- **Covers:** 81, 82, 83
- **Done when:** `VALIDATION.md` records pass/fail for criteria 61–83 with the date and firmware commit. A 240 fps camera is needed for 64, 65, 66, and 71. Confirm the two-LED ceiling of criterion 78 by observation across a full session, confirm brightness uniformity through the finished panel from standing height, and re-run criteria 41–56 to confirm the panel LED is genuinely unaffected.

  **Measure the LED series resistor on the bench before finalising it.** §11.5.2 sizes 100 Ω against a 3.6 V nominal forward voltage with under a volt of headroom, so the real value depends on the parts in hand and on how the finished panel reads on a dark stage.

---

# Phase 10 — BLE MIDI on the Pico 2 W

Implements PRD §5.4, §7.5, §8.5, §9.3, §9.4, §10.1 (flash map), §11.1 (Pico 2 W pins), §11.5.8, §11.6 and §11.7. **Read §5.4, §7.5, §9.3, §9.4 and §10.1 in full before starting T30.** The security posture in §5.4 constrains T38, the flash map in §10.1 constrains T31 and T38, and the "USB wins" rule in §9.4 is what stops every cue firing twice.

## Scope boundary for this phase

**`input`, `sequencer`, and `ui` stay board-independent.** No `cyw43_*`, BTstack, or TinyUSB call appears in them. BTstack headers are included by `src/midi/ble_midi.cpp` and nothing else.

**The `UiEvent` contract does not change**, for the same reason as Phase 9: T08–T13 assert ordered `UiEvent` lists. `ui` learns Bluetooth state by polling `link_state` accessors (QUESTIONS Q032).

**`ButtonEventKind` gains exactly one value, `PairChordHold`.** It is emitted only while setup is active, so no existing ordered `ButtonEvent` assertion can see it. No other change to `ButtonEvent`.

**The §11.4 panel engine is finished work**, unchanged since Phase 9. T30 changes only the *driver shim* for the onboard mirror (GP25 → CYW43 LED on `pico2_w`); `ui_lamp()`, its engine, and its tests are untouched. **Criteria 41–56 must pass at every commit.**

**Both firmware targets build at every commit.** `pico2` (USB-only, existing CI job) and `pico2_w` (new CI job from T30). A task that breaks either is not complete. On `pico2` every Bluetooth path compiles out or reports "no radio", and behaviour is exactly v1 + Phase 9.

**New dependencies approved for this phase, `pico2_w` only:** `pico_cyw43_arch_poll`, `pico_btstack_ble`, `pico_btstack_cyw43`, `pico_btstack_make_gatt_header()`. Nothing else. `pico_btstack_classic`, any `lwip` variant, `pico_cyw43_arch_threadsafe_background`, and any FreeRTOS variant are BLOCKERs.

**Never name GP23, GP24, GP25, or GP29 in `pico2_w` code.** They belong to the radio (PRD §11.1).

**Branching.** T30 branches from `master` once the Phase 10 docs are merged. T31 onward stack normally, each from its predecessor's branch.

## Bootstrap — interactive, before the loop

| Task | Why not the loop |
|---|---|
| **T30** | Board bring-up is environment-specific (SDK submodules for BTstack and cyw43-driver, a new preset, a new CI job), and the GP25 fix is only provable on a real Pico 2 W. A silent failure here — the radio never initialising — poisons every later task. |
| **T31** | The flash map is the one place a mistake destroys data on units already in use: move the config sector and every pedal loses its channel; overlap the bond store and pairing corrupts config. RP2350-E10 must be read and judged by a person. |
| **T32** | The link harness is the loop's only feedback signal for T34–T37, and its API shape constrains all of them. It carries T03's and T25's risk: **a harness whose assertions cannot fail reports green for every task after it.** |

The loop runs **T33–T37**. **T38** (the BTstack transport itself) is also interactive — it implements the §5.4 security posture and cannot be verified without a radio. **T39** is hardware.

The loop must refuse to start until T30–T32 are complete, logged in `PROGRESS.md`, and `ctest` is green.

### T30 — pico2w-board-target
- **Branch:** `task/T30-pico2w-board-target`
- **Commit:** `T30: add pico2_w build target and move the onboard LED mirror to the CYW43`
- **Depends on:** — (branch from `master`)
- **Mode:** **interactive — human-led, not the loop**
- **PRD:** §4, §11.1, §11.2, §12, §12.2
- **Verify:** build + hardware
- **Covers:** 111 (partial), 102
- **Done when:**
  - `CMakePresets.json` gains a `pico2_w` configure preset building into `build-w`, with `PICO_BOARD=pico2_w`. The existing `pico2` preset and `build/` are unchanged. `CMakeLists.txt` keeps `set(PICO_BOARD pico2)` as the default for the `pico2` path only — **confirm a `pico2_w` configure actually reports `pico2_w`**, since T01 deliberately pinned the board inside `CMakeLists.txt`.
  - `.github/workflows/ci.yml` gains a **Firmware build (pico2_w)** job beside the existing two. `AGENTS.md` lists the new commands (already amended in the Phase 10 docs commit).
  - On `pico2_w` only: `pico_cyw43_arch_poll` is linked, `cyw43_arch_init()` runs at boot, and `cyw43_arch_poll()` runs every main-loop iteration immediately after `tud_task()`. No BTstack yet.
  - A new `src/board/board_led.h/.cpp` shim owns the onboard LED: GP25 on `pico2`, `cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, …)` on `pico2_w`, **written only when the value changes**. T17's GP25 write moves into this shim; GP16 handling is untouched. `ui_lamp()` and its engine are not edited.
  - No `gpio_*` call in `src/` can receive 23, 24, 25, or 29 on `pico2_w`, and nothing uses `PICO_DEFAULT_LED_PIN` (the `pico2_w` board header does not define it). Prove it with a grep over `gpio_` call sites plus a `static_assert` in `board_led.cpp`, and paste the grep and its output into `PROGRESS.md`. Note numbers 23 and 25 exist in the cue table, so a bare number grep is not evidence.
  - Before starting, confirm `$PICO_SDK_PATH/lib/btstack/src/bluetooth.h` and `$PICO_SDK_PATH/lib/cyw43-driver/src/cyw43.h` exist. If they do not, **stop** — initialising SDK submodules is the human's job (AGENTS.md: never modify anything under `$PICO_SDK_PATH`).
  - **On hardware:** a Pico 2 W running this build enumerates exactly as criteria 1 and 2, the per-switch LEDs and buttons behave as on the Pico 2, and the onboard LED mirrors the panel engine through a Setup Mode entry, channel blink and exit.

### T31 — flash-map
- **Branch:** `task/T31-flash-map`
- **Commit:** `T31: reserve the bond store and pin the config sector to a fixed offset`
- **Depends on:** T30
- **Mode:** **interactive — human-led, not the loop**
- **PRD:** §10.1, §10.3, §13
- **Verify:** host + build + hardware
- **Covers:** 114, and fixes the v1 defect behind 36 and 38 (PRD §10.1)
- **Done when:**
  - **Fix the v1 config-write freeze first.** `pico_override_flash_size` (Q016) redefines `PICO_FLASH_SIZE_BYTES`, and `flash_range_erase/program` `hard_assert` against it, so every setup-exit write panics today. Record this in `PROGRESS.md` as a defect found in T18/T19 (Q016) and fixed here.
  - `config.h` gains the §13 flash-map constants. Static assertions prove: the three regions are sector-aligned, contiguous, non-overlapping, and end exactly at `FLASH_TOTAL_BYTES`; `CONFIG_FLASH_OFFSET == 0x3FF000`; `BOND_FLASH_OFFSET == 0x3FD000`.
  - `config_store` addresses its sector by `CONFIG_FLASH_OFFSET`, **not** by `PICO_FLASH_SIZE_BYTES` (QUESTIONS Q016 is superseded for addressing; record that in `PROGRESS.md`).
  - **Remove `pico_override_flash_size`.** `PICO_FLASH_SIZE_BYTES` stays at the true 4 MB. Shrink only the linker `FLASH` region to `IMAGE_FLASH_LIMIT` with a project `pico_flash_region.ld` passed through `pico_add_linker_script_override_path`, and no compile definition. A target-side static assertion proves `CONFIG_FLASH_OFFSET + FLASH_SECTOR_BYTES <= PICO_FLASH_SIZE_BYTES` and `BOND_FLASH_OFFSET + BOND_FLASH_SIZE <= PICO_FLASH_SIZE_BYTES`. `PICO_FLASH_BANK_STORAGE_OFFSET` and `PICO_FLASH_BANK_TOTAL_SIZE` are set as compile definitions for `pico2_w` from the same constants, and a static assertion in a `pico2_w`-only translation unit ties them to `config.h`.
  - Host tests: the existing T18/T19 config tests still pass unchanged; a new test asserts the map.
  - **RP2350-E10 is read and recorded.** `PROGRESS.md` quotes the datasheet's E10 entry and states whether a single-family UF2 drag-and-drop can write the last sector. If it can, raise a BLOCKER and stop (PRD §10.1).
  - The `.elf`'s highest loaded address is below `IMAGE_FLASH_LIMIT` on both targets (`arm-none-eabi-size -A` or `readelf -l`; record the figure).
  - **On hardware:** criterion 114 on both boards. Select channel 8 and exit → no freeze, cues on channel 8. Power-cycle → channel 8. Reflash → channel 8. Repeat the change through the 30 s timeout.

### T32 — link-harness
- **Branch:** `task/T32-link-harness`
- **Commit:** `T32: add the link state module skeleton and extend the harness with link and transport capture`
- **Depends on:** T31
- **Mode:** **interactive — human-led, not the loop**
- **PRD:** §7.5, §8.5, §9.4, §12.1
- **Verify:** host
- **Covers:** —
- **Done when:**
  - `src/link/link_state.h/.cpp` exists, pure and host-buildable, declaring `LinkEvent`, `LinkRequest`, `link_tick(now)`, `link_on_event()`, `link_poll_request()`, and the read-only accessors listed in `TEST-HARNESS.md`. For this task the behaviour may be stubbed (window never opens, BLE never ready).
  - `src/midi/transport.h` declares `Transport` and the §9.4 policy function; `midi_out` exposes a host seam so the harness can capture `TransportSend { transport, note }`. Behaviour may be stubbed (always USB).
  - The harness pipeline runs `link_tick` and the link half of `loop_dispatch` each pass, with a **fake transport**: `harness_set_usb_mounted()`, `harness_link_event()`, `REQUIRE_LINK_REQUESTS()`, `REQUIRE_TRANSPORT_SENDS()`, and the rest of the Phase 10 section of `TEST-HARNESS.md`. Document every function there with a signature and one example each, and add one exemplar test, `tests/test_link.cpp`.
  - **Negative control.** Write a test asserting a deliberately wrong `LinkRequest` list and a deliberately wrong `TransportSend`; confirm each fails with `file:line`; correct them. `PROGRESS.md` must say so in words — the loop prompt checks for it.
  - `harness_lamp_trace()`, `harness_switch_trace()`, and every existing matcher are unchanged; the full suite is green including T10–T13.

## Loop tasks

### T33 — ble-midi-packet
- **Branch:** `task/T33-ble-midi-packet`
- **Commit:** `T33: add the BLE-MIDI packet encoder`
- **Depends on:** T32
- **PRD:** §9.3.2
- **Verify:** host
- **Covers:** 99
- **Done when:** A pure function in `src/midi/ble_midi_packet.h/.cpp` encodes one Note On (and, behind `SEND_NOTE_OFF`, one Note Off) into the five-byte §9.3.2 layout from `(note, channel, now_ms)`. Tests cover the two worked examples in criterion 99, the 13-bit wrap at 0x2000, channels 1 and 10, and every note 0–32 from `CUE_TABLE`. Bit 7 is set on header and timestamp bytes for every input, and a test asserts it across a sweep of timestamps. No BTstack include.

### T34 — transport-arbitration
- **Branch:** `task/T34-transport-arbitration`
- **Commit:** `T34: route each cue to exactly one transport with USB taking priority`
- **Depends on:** T33
- **PRD:** §9, §9.4
- **Verify:** host
- **Covers:** 97, 98 (transport half)
- **Done when:** The §9.4 table is the `transport` policy function, and `midi_out` calls it **per cue**. Tests drive `harness_set_usb_mounted()` and a ready/not-ready link through all four combinations, toggle USB between two cues of one sequence, and assert with `REQUIRE_TRANSPORT_SENDS` that every cue appears exactly once or not at all. Nothing is sent in `SETUP` or `LOCKED`. The USB path is byte-for-byte what T16 sends; the BLE path hands the T33 encoder's output to the transport seam. Existing `Command` captures are unchanged.

### T35 — pair-chord-detection
- **Branch:** `task/T35-pair-chord-detection`
- **Commit:** `T35: detect the 2+4 pair chord in setup with a strict continuous hold timer`
- **Depends on:** T34
- **PRD:** §7.5, §6.5 (rules reused)
- **Verify:** host
- **Covers:** 84, 85, 86, 87
- **Done when:** While setup is active, `input` applies the §6.5 rules to buttons `PAIR_CHORD_BUTTON_A` and `_B`: any overlap flags both `pair_overlap` and suppresses their taps; the timer starts at the second press, needs `PAIR_CHORD_HOLD_MS` of continuous overlap, is cleared on any release, and restarts in full on re-form; at the threshold one `ButtonEventKind::PairChordHold` is emitted. The sequencer turns it into one `BEGIN_PAIRING` request through its link-request drain. **Outside setup nothing changes** — criterion 86 is the guard, and all of T05–T09's tests must still pass. A contact bounce on 2 or 4 must not clear the timer (mirror the T06 bounce test).

  Write 86 first. The most likely defect is a pair chord that leaks into performance mode and swallows Select (note 28).

### T36 — pairing-window
- **Branch:** `task/T36-pairing-window`
- **Commit:** `T36: implement the pairing window with suspended setup timeout and ignored taps`
- **Depends on:** T35
- **PRD:** §7.5, §8.5
- **Verify:** host
- **Covers:** 88, 90, 91, 92, 93, 94, 98 (window half)
- **Done when:** `link_state` owns the window: `BEGIN_PAIRING` opens it (and issues the bond-clear + advertise request to the transport), `PAIRING_WINDOW_MS` closes it with `END_PAIRING`, `LinkEvent` *pairing started / succeeded / failed* drive the sub-state, and a second `BEGIN_PAIRING` restarts it. The sequencer polls `link_pairing_window_open()`: taps select nothing, the inactivity timeout is suspended and restarts from the window-closed edge, and setup exit issues `END_PAIRING` first. Every criterion in the list has a test that brackets its threshold (`− 1` / exact), and criterion 93 asserts the §7.3 flash-write rule is unchanged (`config_store_host_write_count`).

### T37 — switch-led-link-indications
- **Branch:** `task/T37-switch-led-link-indications`
- **Commit:** `T37: add pairing and boot link indications to the switch LEDs`
- **Depends on:** T36
- **PRD:** §11.5.3, §11.5.8
- **Verify:** host
- **Covers:** 89, 95, 96, 100
- **Done when:** The per-switch engine gains priority 0 (pairing window: advertising 500/500, bonding 100/100, bond OK solid 2000 ms, all on LEDs 2 and 4 in phase) and the boot link confirmation at priority 4 (one 1000 ms flash on 2 and 4, waiting for the boot channel indication, cancelled or dropped by an accepted press, at most once per power cycle, only within `BOOT_CONNECT_WINDOW_MS`). State comes from `link_state` accessors only — **no new `UiEventKind`**. After bond OK or window timeout the setup channel LED resumes from its on phase. Every test asserts `harness_switch_led_count() <= 2`. Both edges of every phase are asserted. Criteria 61–82 still pass.

## Interactive — after the loop

### T38 — ble-midi-transport
- **Branch:** `task/T38-ble-midi-transport`
- **Commit:** `T38: add the BTstack BLE-MIDI GATT server with pairing-window security policy`
- **Depends on:** T37
- **Mode:** **interactive — human-led, not the loop**
- **PRD:** §5.4, §9.3, §10.1, §10.3, §12.2
- **Verify:** build + hardware
- **Covers:** enables 103–113 (verified in T39)
- **Done when:**
  - `pico2_w` links `pico_btstack_ble` and `pico_btstack_cyw43`. `src/midi/btstack_config.h` enables BLE only, LE Secure Connections, `NVM_NUM_DEVICE_DB_ENTRIES = BLE_MAX_BONDS`, and routes BTstack logging to UART only under `DEBUG_UART` (off in release).
  - `src/midi/trackstomp.gatt`, compiled with `pico_btstack_make_gatt_header()`, contains GAP (name `TrackStomp Navigator`), GATT, and the BLE-MIDI service with its one characteristic marked as requiring encryption. **Nothing else.** A host-side or build-time check fails if any other service UUID appears in the generated header.
  - `src/midi/ble_midi.cpp` is the only file including BTstack. It implements the §9.3.3 advertising table, the §9.3.4 Security Manager policy (confirm Just Works only while the window is open; decline otherwise), the `BLE_UNBONDED_DROP_MS` drop, the connection-interval request, read-empty / write-discard, notifications from the T33 encoder, bond clear on `BEGIN_PAIRING`, and translation of BTstack events to `LinkEvent` (*ready* only when encrypted with the bond **and** subscribed).
  - On `pico2`, a stub transport reports "no radio" so the window closes immediately (PRD §7.5).
  - An instrumented debug build counts flash-bank erases and programs (criterion 112's tool).
  - **Quick hardware smoke test before handing to T39:** pair from a Mac, see Note On in a MIDI monitor, power-cycle, confirm reconnect. Record it in `PROGRESS.md`.

## Hardware validation

### T39 — ble-bench
- **Branch:** `task/T39-ble-bench`
- **Commit:** `T39: record BLE MIDI bench validation results`
- **Depends on:** T38, and the post-assembly validation round (T22, T23, T29)
- **Verify:** hardware
- **Covers:** 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114
- **Done when:** `VALIDATION.md` records pass/fail for criteria 102–114 with the date, `git describe`, and artifact SHA-256 of the `pico2_w` `.uf2`, the Mac and iPad OS versions, the Playback version, and the iPad middleware used. Run 110 in the sealed enclosure, standing on it, before deciding anything about shielding (PRD §11.6). Run 108 with the connection interval each host actually granted recorded beside the result.

---

## Loop exit condition — Phase 10

Phase 10's automated portion is done when **T33–T37 are committed and pushed, `ctest` is green covering criteria 84–101, criteria 4–82 still pass, and both `pico2` and `pico2_w` firmware builds succeed.** T38 is interactive and T39 is hardware. Do not attempt them.

---

## Loop exit condition — Phase 9

Phase 9 is done with the automated portion when **T24–T28 are committed and pushed, and `ctest` is green covering criteria 61–82**, with 41–56 still passing. Criterion 83 is hardware-only (T29). Do not attempt it.

---

## Loop exit condition — v1 (historical)

The loop is done with the automated portion when **all of T01–T21 are committed and pushed, and `ctest` is green covering acceptance criteria 4–56 minus the hardware-only ones**.

Criteria requiring hardware and therefore excluded from the loop's exit condition: **1, 2, 3, 36, 37, 39 (on-target confirmation), 57, 58, 59**, plus on-target re-verification of everything in T22.

Do not attempt to satisfy those. Report them as blocked-on-hardware and stop.
