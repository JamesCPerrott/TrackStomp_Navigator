# TrackStomp Navigator — Project Handoff

Carry this into a new conversation to pick up where the previous one left off.

Last updated 2026-11-06. Code at `master` = `796ff88` (`git describe`: `v1.0-14-g796ff88`); **Phase 10 (BLE MIDI) is scoped in the docs but no code has been written.**

---

## What this is

A USB class-compliant MIDI foot controller driving **MultiTracks Playback**. Ten momentary footswitches send Note On cues that select song sections. Buttons 1–5 pick a section type (Intro, Verse, Chorus, Bridge, Outro), buttons 6–9 pick a variant (One, Two, Three, Tag), and every section cue is a two-button tap sequence. 33 cues total, notes 0–32.

Built on a **Raspberry Pi Pico 2 (RP2350)** in C++ on the Pico SDK, moving to a **Pico 2 W** for BLE MIDI in Phase 10. The language choice is driven by a security requirement: the firmware must be compiled, not interpreted, so plugging in a USB cable exposes a MIDI endpoint and nothing else — no browsable source, no mass storage volume, no serial console.

---

## Where the project stands

**v1 firmware and Phase 9 are both written, merged, and running on hardware.**

- T01–T03 (scaffolding, cue table, host test harness) were done interactively with human review.
- T04–T21 were completed by an unattended Cursor `/loop` run — 18 tasks, each on its own branch with one commit, all merged. Tagged **`v1.0`** (`47556d4`), with the validated `.uf2` attached to a GitHub release.
- T22 and T23 (bench validation, soak) **were run but the results were never written to `docs/VALIDATION.md`.** The file exists as a populated skeleton with every criterion listed and the result column empty. Rather than reconstruct them from memory, a **full validation round will be run after final assembly** and its results will fill in VALIDATION.md for T22, T23 and T29 together.
- **Phase 9** (T24–T28) added ten per-switch status LEDs and is merged. T24 and T25 were human-led bootstrap; T26–T28 ran under the loop.
- **T29 (per-switch LED bench validation, criteria 61–83) has been completed on the bench, but the results are not yet recorded.** They will be captured in the post-assembly validation round above.
- **Phase 10 (T30–T39) — BLE MIDI — is scoped and not started.** PRD, TASK-LIST, AGENTS, QUESTIONS (Q027–Q032), TEST-HARNESS, LOOP-PROMPT, PROGRESS, and VALIDATION were all amended on 2026-11-06. The loop status line reads `NOT STARTED — Phase 10`. T30–T32 are human-led bootstrap; the loop runs T33–T37; T38 (the BTstack transport) is human-led; T39 is hardware.
- **The display is dropped** for the foreseeable future (decided 2026-10-31). PRD §16 says "not planned".
- Enumeration verified on Linux: `2e8a:fffe`, `TrackStomp Navigator`, jack string `Navigator Cues`, MIDI Streaming subclass, no CDC interface.

---

## Key design decisions and their reasoning

Understanding *why* these were chosen matters more than the values themselves — several were revised more than once before settling.

**Input grammar.** Buttons 1–5 are prefix-only on tap, so nothing fires on a bare tap and no double-tap window is needed. Button 10 taps to Repeat and holds to Loop. Button 8 is the only suffix button with a hold (Mute MIDI, note 32) — chosen because it is not a chord member. Holds are 2 s; the 6+9 chord for Setup Mode is 5 s of *continuous* overlap.

**BLE MIDI (Phase 10).** Decided with James on 2026-10-31 (PRD §15 rows 18–25):
- Playback runs on **Mac and iPad only**. Mac has native BLE MIDI; iPad needs BLE MIDI middleware to connect.
- BLE exists for **cable-free use from the 9 V supply and for older iPads**. 9 V is no longer optional on a BLE unit.
- **USB wins** when both are connected, per cue, never both — otherwise the host sees two ports and every cue fires twice. The BLE link stays up for failover.
- **Pairing lives in Setup Mode**: hold 2+4 for 2 s, which **clears the bond** and opens a 60 s pairing window. Pairing is persistent (one bond). Just Works, accepted.
- **LEDs 2 and 4**: slow flash advertising, fast flash bonding, one long flash on success, and one flash if the host connects at boot.
- **Security**: one GATT service (BLE-MIDI), encryption required on the MIDI characteristic, pairing declined outside the window, no advertising at all until first paired. PRD §5.4.
- **Latency**: USB keeps its 10 ms goal; BLE has a measured budget (median ≤ 20 ms, worst ≤ 40 ms) because it is bounded by the connection interval.
- **Enclosure shielding is deferred** until range is measured in the sealed die-cast box.

**Pico 2 W gotchas the plan already handles.** GP25 is the radio's SPI chip-select on the W, so v1's GP25 onboard-LED write would fight the radio; the onboard LED is now on the CYW43 (`WL_GPIO0`) and written only on change (T30). BTstack's default bond-store offset differs from what v1's flash-size override implies, and v1 derives the config address from that override; T31 pins both to explicit offsets and moves config to `0x3FC000`, because an A2 bootrom erases the last sector on every drag-and-drop (RP2350-E10, §10.1). Nothing may live at `0x3FF000`. BTstack is a second flash writer; T38 must prove it never writes during play or on reconnect (§10.3).

**Hold lockout.** Once a hold fires, all input is ignored until that button is released, and any button still pressed at release is tap-suppressed to prevent phantom cues.

**Chord handling.** Releasing either member clears the timer rather than pausing it. An aborted chord fires nothing at all — deliberately conservative, since a wrong cue mid-service is worse than no cue.

**MIDI Channel Setup Mode.** Entered by the 6+9 chord, exited by a 6+9 tap regardless of whether anything changed, auto-exits after 30 s and commits. Channel persists to a reserved flash sector with a CRC and falls back to channel 1 on any validation failure. Channels 11–16 are unreachable by design.

**Per-switch LEDs superseded the v2 display.** One LED above each footswitch shows pending state, confirmations, lockout and setup channel *in the place the player is already looking*, and makes Setup Mode self-explanatory — channel 7 is simply the lit LED above button 7. This removed an I2C bus, a driver, a font, a framebuffer and a failure-degradation path. PRD §11.5 is the spec; §16 records the display as deferred to v3.

**One indication at a time.** §11.5.3 permits exactly one active indication owning at most two LEDs, every other bit clear. Criterion 78 enforces it. An engine built as ten independent per-LED state machines passes its own unit tests and violates this — it was called out explicitly in the Phase 9 task list for that reason.

**Reverse lookup, not a wider event.** T24 added a pure function mapping a cue note back to its originating buttons, derived from `CUE_TABLE`. This let the LED engine light the buttons behind a cue without touching the `UiEvent` contract, which would have turned T08–T13's ordered assertions red for reasons unrelated to any defect. See `QUESTIONS.md` Q022.

**LED indication (panel LED, §11.4).** A strict priority stack on a single LED, unchanged by Phase 9 and still specified. Solid means Setup Mode and nothing else; PENDING is a 1000 ms slow flash; lockout is 100 ms; the chord shows nothing for 500 ms then flashes at 250 ms. **This LED is not populated on the current cape — see Hardware.**

---

## Hardware

**Board.** Phase 10 fits a **Pico 2 W** — same footprint and header, every pin this design uses unchanged. GP23/24/25/29 belong to the radio and are never touched by application code.

**Pins.** Buttons on GP6–GP15 with internal pull-ups only (RP2350 erratum makes pull-downs unusable). Status LEDs 1–10 on **GP5, GP4, GP3, GP2, GP22, GP21, GP20, GP19, GP18, GP17** — deliberately not contiguous, so the engine uses a bit-position table rather than an ordered range. GP26–GP28 reserved for future expression input. Every header pin is now allocated.

**LED drive.** Blue 5 mm panel LEDs (Dialight, Vf 3.6 V, 30 mA rated, 1/4" front-load, 14" pigtails), each switched by a 2N3904 low-side NPN from **VSYS** — 100 Ω collector resistor, 2.2 kΩ base resistor, non-inverting. VSYS rather than VBUS so the LEDs work on either power source. Worst-case headroom is about 0.95 V, so brightness is sensitive to Vf spread; `SWITCH_LED_DUTY` in `config.h` exists to trim it, and 82 Ω is the fallback if the panel reads dim.

**GP16 (panel LED) is not populated on this board revision.** The firmware still drives the pin; it goes nowhere. **Acceptance criteria 41–56 are therefore unverifiable on this hardware.** This is not recorded in the PRD and should be, either as a board-revision note or by amending §11.4.

**Cape.** 100 × 100 mm two-layer board, bottom-layer ground pour with VSYS and 9 V rails, all GPIO on top. Four 3.2 mm corner standoffs. Through-hole throughout, hand-assembled. All harnesses soldered directly — 24 AWG stranded, 14" on LEDs and switches, ~8" on power — with zip-tie strain relief 10 mm back from each pad row. An antenna keep-out is already cut into the pour under the Pico, opening toward the board edge.

**9 V input (designed, not yet tested).** Plastic Boss-style bulkhead jack, so no chassis-isolation problem. Chain is: polyfuse (MF-R050) → series Schottky for reverse polarity (1N5819) → input clamp (P6KE18A) → 220 µF bulk → buck module (Murata OKI-78SR-5/1.5-W36-C, 9–36 V in, fixed 5 V) → output clamp (P6KE6.8A) → OR-ing Schottky (1N5819) → VSYS. USB and 9 V coexist through the Pico's own VBUS/VSYS Schottky; **nothing connects to VBUS.** Testing this is the next hardware step before the cape is ordered.

**Enclosure.** Hammond 1550WJBK die-cast, 275 × 175 × 62 mm. Switch pitch works out at ~55 mm (2.17"), below the 2.5–3" originally wanted — worth standing on a mock-up before committing, since a tight pitch risks a boot firing a prefix and suffix together.

**USB connector.** The Adafruit 4056 panel-mount USB-C-to-micro-B assembly **does not work** — it has no CC resistors, so a USB-C source supplies nothing. Symptom is total silence: no power, no enumeration, no clue. **Decided (2026-10-31): USB-B.** It is what every other box on a pedalboard uses, far more durable underfoot, and removes the CC problem entirely. Ship a C-to-B cable in the box.

---

## The working method

This is worth preserving, because it is what made the unattended runs viable.

**Specs are authoritative and layered.** `PRD.md` holds behaviour; `TASK-LIST.md` holds scope, ordering, branch names, and exact commit messages; `AGENTS.md` holds rules and commands; `QUESTIONS.md` is an async question queue; `PROGRESS.md` is an append-only log.

**Questions are classified, not blocking.** A loop has nobody to wait for. BLOCKER questions (new dependencies, spec contradictions, anything touching OTP fuses) stall the task and move on. ASSUMPTION questions get a documented default and proceed. The default when unsure is ASSUMPTION — a documented wrong guess is cheap to reverse, a stalled loop wastes the window. Q001–Q020 cover v1, Q021–Q025 cover Phase 9; all stand as decisions, none were overridden.

**Host tests are the feedback signal.** The layer separation in PRD §12.1 exists so `input`, `sequencer`, and `ui` compile for the host with no SDK dependency. A fake clock and synthetic events make nearly every timing criterion testable off-target.

**Each phase has human-led bootstrap tasks.** v1: T01 (environment-specific), T02 (the cue table is the single source of truth), T03 (the harness API constrains everything downstream). Phase 9: T24 (API shape) and T25 (the harness extension). The recurring reason is that a harness whose assertions cannot fail reports green for every task after it — T25's completion criteria required *proving* the matcher could fail, and the loop prompt refused to start until `PROGRESS.md` said so.

**`LOOP-STATUS` is scoped to a phase, not the project.** v1 reached `COMPLETE`; Phase 9 reset the line. A new phase resets it again.

**CI checks the loop's self-reporting.** Two jobs on every branch, on a clean checkout. The repo ruleset requires both to pass, so `master` is only reachable by PR — direct pushes are rejected.

---

## Lesson from the bench

**Ninja only rebuilds on source change.** After the Phase 9 merges landed, the `.uf2` in `build/` was still the binary built during the v1.0 release work — it looked current and was weeks stale. An hour went into diagnosing ten dead GPIO pins that were simply never configured, because the firmware on the board predated the LED engine entirely. **Rebuild and reflash before every bench session**, and record the commit in the bench log.

The durable fix is to put `git describe --always --dirty` into the USB `iSerial` string alongside the chip ID, so `lsusb` answers "which firmware is this" directly. That touches PRD §9.1, which enumerates descriptor fields explicitly, so it wants a `QUESTIONS.md` entry and its own small task rather than slipping into another phase.

---

## Assets to attach

| File | Why |
|---|---|
| `docs/PRD.md` | All behaviour, timing, pin assignments, electrical specs, acceptance criteria |
| `docs/TASK-LIST.md` | Task structure, and the template for how the next phase should be scoped |
| `AGENTS.md` | Rules, verified toolchain, commands, layer rules |
| `docs/PROGRESS.md` | What the loops actually did, including "tried and abandoned" notes |
| `docs/TEST-HARNESS.md` | The harness API any new tests must use |
| `docs/QUESTIONS.md` | Assumed decisions not yet reviewed |
| `docs/VALIDATION.md` | Bench results — **currently empty, fill before scoping anything new** |
| KiCad schematic + PCB, or a PDF export | The cape is not otherwise documented anywhere |

Leave the loop and task prompts out of a project's knowledge — they are operational, and a future chat reading them may conclude a finished phase is still pending.

---

## Likely next phases

**Finish Phase 9.** T29 has been run on the bench; after final assembly, a full validation round re-runs T22, T23 and T29 (criteria 61–83) and records all three in VALIDATION.md. A 240 fps camera is needed for 64, 65, 66 and 71. Confirm the two-LED ceiling (78) by observation across a full session, and check brightness uniformity through the finished panel from standing height before settling the series resistor.

**Test the 9 V front-end, then order the cape — with a Pico 2 W fitted.** Decided 2026-10-31. Confirm the Pico 2 W's antenna end sits over the ground-pour keep-out with no harness or standoff crossing it (PRD §11.6).

**iSerial firmware version.** Small, independent, testable, touches only §9.1. Worth doing on its own ahead of anything larger.

**Phase 10 — BLE MIDI.** Scoped in `TASK-LIST.md` as T30–T39; see `CURSOR-GUIDE-PHASE10.md` for the run order. Before any code: confirm Playback receives BLE MIDI on the Mac and through the iPad middleware using any BLE MIDI source, and check the SDK has its `lib/btstack` and `lib/cyw43-driver` submodules.

**Deferred.** Display (not planned — would need I2C moved to GP0/GP1), WiFi, expression pedal input, runtime-editable cue mappings, preset banks, MIDI channels 11–16.

---

## Loose ends

- **`docs/VALIDATION.md` is empty.** T22, T23 and T29 were run; the results exist only in memory until the post-assembly validation round records them.
- **Criterion 34 is claimed by no task.** Walking the `Covers:` lines across T04–T21, every criterion is accounted for except 34 ("enter setup, select nothing, tap 6+9 → exits normally, no flash write"). The behaviour is almost certainly present via T09 and T19, but nothing asserts it. It is a cheap host test and exactly the kind of thing that works in the state machine and breaks at the seam.
- **D3 is mislabelled in KiCad.** The schematic and silkscreen read `SMAJ5.0A`, a surface-mount part, in a DO-41 through-hole footprint. The board is right; the label is not. Change it to `P6KE6.8A` before ordering.
- **GP16 is unpopulated**, so criteria 41–56 cannot be verified on this revision. Now recorded as a board-revision note in PRD §11.4.
- **Series resistor value disagrees across docs.** PRD §11.5.2 specifies 220 Ω (measured on cape v1); this file's Hardware section and `VALIDATION.md`'s T29 block still say 100 Ω with an 82 Ω fallback. Settle it in the post-assembly validation round.
- **v1.0 and v1.1 freeze on any channel change.** `pico_override_flash_size` (Q016) shrinks `PICO_FLASH_SIZE_BYTES`, so `flash_range_erase` hard-asserts and the pedal panics on setup exit. Nothing was ever saved. Found 2026-11-07; fixed in T31 (PRD §10.1). A v1.1 fix release would be a cherry-pick of that fix onto `release/v1.1`, keeping the T31 map (config at `0x3FC000`) so settings survive the later upgrade to Phase 10.
- **BTstack licence** — confirm the Raspberry Pi terms in `$PICO_SDK_PATH/lib/btstack/LICENSE` before the device leaves the bench (PRD §4).
- **PID is a placeholder.** `0xFFFE` under Raspberry Pi's VID. A real allocation needs a PR to `raspberrypi/usb-pid`. pid.codes is not an option — VID 0x1209 requires an open source license, which conflicts with §5.1.
- **Tier 2 security hardening (PRD §5.2) is deliberately unimplemented.** OTP fuses, secure boot signing, and SWD lockdown are irreversible and will brick the board if done without a signed update path. Hard stop in `AGENTS.md`.
- **CI runs every job twice** on PR branches, because `ci.yml` triggers on both `push` and `pull_request`. Harmless; fix by narrowing the push trigger to `master`.
- **ALSA truncates the port name** to `TrackStomp Navigator Navigator` at 32 characters. The descriptor is correct; only the Linux display is affected.
- **Two endpoints are declared, not one.** PRD §9.2 permits this. The device appears in Playback's MIDI output list as well as its input list.
- **`PROGRESS.md` commit fields all read `(this commit)`.** Correct for task entries — a commit cannot contain its own hash — but it means `VALIDATION.md` must carry a tag and artifact hash rather than pointing at the log. v1 is tagged `v1.0`, artifact SHA-256 `7d8012170608...57e0b2`.
