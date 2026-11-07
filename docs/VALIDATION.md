# Validation results

Bench validation of the acceptance criteria in PRD §14. Host-testable criteria are covered by
`ctest` and are not repeated here; this file records what was verified **on hardware**.

One row per criterion. `Result` is one of `PASS`, `FAIL`, `TUNED`, `N/A` or `BLOCKED`. `TUNED` means
the criterion passes but a constant was adjusted from its PRD value to get there — record the old and
new value in Notes, and raise it as a question if the PRD specifies the value explicitly. `BLOCKED`
means the hardware cannot express the criterion at all; say why.

---

## Run record

| Field | T22 / T23 (v1) | T29 (Phase 9) | T39 (Phase 10) |
|---|---|---|---|
| Firmware tag | `v1.0` | *TBD* | *TBD* |
| Firmware commit | `47556d433863082e9f1b942dcd624cb18cf6d65b` | `796ff88` *(confirm)* | *TBD* |
| `git describe` | `v1.0` | `v1.0-14-g796ff88` | *TBD* |
| Artifact SHA-256 | `7d80121706085a8244155f47c5d5037dc547b8703f66418a9dd807239657e0b2` | *TBD* | *TBD* |
| Board revision | Breadboard / pre-cape | Cape v1 | *TBD* |
| Date of run | *TBD* | *TBD* | *TBD* |
| Host OS | *TBD* | *TBD* | *TBD* |
| Playback version | *TBD* | *TBD* | *TBD* |
| Camera for LED timing | *TBD — 240 fps needed for 45–53, 64–66, 71* | *TBD* | *TBD* |
| Board | | | Pico 2 W, `pico2_w` build |
| Mac / iPadOS versions | | | *TBD* |
| iPad BLE MIDI middleware | | | *TBD* |

**Rebuild and reflash before every run.** Ninja only rebuilds on source change, and a stale `.uf2`
in `build/` has already cost one debugging session. Record the commit above from `git describe`,
not from `PROGRESS.md` (every task entry reads `(this commit)` by design).

---

## Summary

| Result | v1 (1–60) | Phase 9 (61–83) | Phase 10 (102–114) |
|---|---|---|---|
| PASS | | | |
| FAIL | | | |
| TUNED | | | |
| N/A | | | |
| BLOCKED | | | |

---

## Known hardware limitation — read before filling in 41–56 and 81

**GP16 is not populated on cape v1.** The firmware drives the pin; nothing is connected to it. The
panel LED of PRD §11.4 therefore cannot be observed on this board, and **criteria 41–56 and 81 are
`BLOCKED`, not `PASS`.** Mark them so rather than leaving them blank, and record whether they were
previously verified on the pre-cape wiring during T22.

---

## T22 — bench validation (v1)


### Enumeration

| # | Criterion | Result | Notes |
|---|---|---|---|
| 1 | Device appears as a class-compliant MIDI input on macOS, Windows, and iOS with no drivers. | | |
| 2 | No mass storage volume mounts. No serial port appears. Release build only. | | |
| 3 | MultiTracks Playback receives and triggers on all 33 cues. | | |

### Sequences

| # | Criterion | Result | Notes |
|---|---|---|---|
| 4 | Tap 1 then 6 → note 0 only. | | |
| 5 | Tap 5 then 9 → note 19 only. | | |
| 6 | Tap 3 then 3 → note 24 only (self-pair precedes prefix reassignment). | | |
| 7 | Tap 2 then 4 then 8 → note 14 only. No Verse note. | | |
| 8 | Tap 2, wait 6 s → nothing sent, state returns to IDLE. | | |
| 9 | Tap 2, wait 6 s, tap 6 → nothing sent. | | |
| 10 | Tap 6 with no pending prefix → nothing sent, no state change. | | |
| 11 | Tap 10 → note 20 within 10 ms of release. | | |
| 12 | Tap 2 then tap 10 → note 20 only, pending Verse discarded. | | |

### Holds (unaffected by chord logic)

| # | Criterion | Result | Notes |
|---|---|---|---|
| 13 | Hold 1 for 2 s → note 27 at the 2 s mark, while still held. Release sends nothing. | | |
| 14 | Hold 5 for 2 s → note 31. Confirms buttons 1 and 5 have no residual chord behaviour. | | |
| 15 | Hold 10 for 2 s → note 21 at the 2 s mark, then lockout. No note 20. | | |
| 15b | Hold 8 for 2 s → note 32 at the 2 s mark, then lockout. Nothing else. | | |
| 15c | Tap 2, then hold 8 for 2 s → note 32 only. No Verse 3 note, pending discarded. | | |
| 15d | Hold 6 for 3 s, hold 7 for 3 s, hold 9 for 3 s (each alone) → nothing sent in any case, no lockout. | | |
| 16 | Tap 2 then hold 5 for 2 s → note 31 only. No Verse note. | | |
| 17 | Tap 2, then press 2 and hold past 2 s → note 28 only. No note 23. | | |
| 18 | Hold 7 for 3 s and release → nothing sent, no lockout entered. | | |

### Lockout

| # | Criterion | Result | Notes |
|---|---|---|---|
| 19 | Hold 1 for 2 s (note 27 fires). While still holding, press and release 6, 7, 3 → nothing further sent. Release 1 → nothing sent. | | |
| 20 | Hold 1 for 2 s. While still holding 1, press and hold 3. Release 1, then release 3 → nothing sent from 3. | | |
| 21 | After any lockout ends, tap 2 then 7 → note 5. | | |

### Chord timing — the strict 5-second rule

| # | Criterion | Result | Notes |
|---|---|---|---|
| 22 | Press 6 and 9 together, hold 5 s → setup entered. Nothing else sent. | | |
| 23 | Press 6, press 9 at +1 s, hold both → setup entered at +6 s, not +5 s. Timer runs from the second press. | | |
| 24 | **Press 6, press 9 at +1 s, release 9 at +1.5 s, keep holding 6 to +7 s → no setup, nothing sent.** The explicit requirement. | | |
| 25 | Hold 6 alone for 10 s → nothing sent, no setup. | | |
| 26 | Press 6, press 9 at +1 s, release 9 at +1.5 s, press 9 again at +2 s, hold both → setup at +7 s. Fresh timer on re-form. | | |
| 27 | Press 6+9, hold 4 s, release both → nothing sent. No Verse cue, no orphan suffix, no pending sequence. | | |
| 28 | Pending on 2, then press 6+9 and hold 5 s → setup entered, no Verse 1 sent. | | |
| 29 | Contact bounce on button 9 during a 5 s chord hold does not clear the timer (verify with an instrumented debug build). | | |
| 29b | Press 6+9 and hold; at +1 s also press and hold 8 past its 2 s threshold → note 32 fires and lockout engages; the chord is cancelled and setup is NOT entered even if 6 and 9 stay down past 5 s. | | |

### Setup mode

| # | Criterion | Result | Notes |
|---|---|---|---|
| 30 | On entry, release both 6 and 9 → slow blink begins. No MIDI sent throughout. | | |
| 31 | While in setup, tap 2 then 7 → **no MIDI sent**. LED blinks 2, then 7. | | |
| 32 | While in setup, hold 3 for 3 s → nothing happens. No note 29. Same for a 3 s hold of 8 — no note 32. | | |
| 33 | While in setup, tap 8 → LED blinks 8 times. Tap 6+9 and release → exit pattern. Subsequent cues transmit on channel 8. | | |
| 34 | **Enter setup, select nothing, tap 6+9 → exits normally.** No flash write, no error, returns to IDLE. | | |
| 35 | Enter setup, tap 6 alone → channel 6 selected. Enter setup again, tap 9 alone → channel 9 selected. Confirms the chord buttons still work as individual channel selectors. | | |
| 36 | Power cycle after selecting channel 8 → boot blink shows 8. Cues transmit on channel 8. | | |
| 37 | Enter setup, change nothing, exit → no flash erase occurs (instrumented debug build). | | |
| 38 | Enter setup, tap 4, wait 30 s untouched → auto-exit, channel 4 committed. | | |

### Factory default

| # | Criterion | Result | Notes |
|---|---|---|---|
| 39 | Erase flash entirely and boot → boot blink shows 1 (single blink). Cues transmit on channel 1. No write occurs at boot. | | |
| 40 | Corrupt the config sector's CRC and boot → falls back to channel 1 without hanging. | | |

### LED indication

| # | Criterion | Result | Notes |
|---|---|---|---|
| 41 | Press 6+9. LED stays **off** for 500 ms, then flashes at a 250 ms period until 5 s, then goes solid. Verify with a scope or slow-motion video. | | |
| 42 | Press 6+9 while a sequence is PENDING (LED slow-flashing). LED goes hard off at chord start, not straight to the faster chord flash. | | |
| 43 | Press 6+9, release at 300 ms → LED returns to its prior state with no flash ever shown. | | |
| 44 | Press 6+9, release 9 at 2 s, re-press 9 at 2.5 s → LED goes off again for 500 ms from the re-press, then resumes flashing. | | |
| 45 | Tap 2 then 7. LED slow-flashes at a 1000 ms period while pending, then shows one 150 ms flash on the cue. Solid on never appears. | | |
| 46 | Tap 10 from idle → exactly one 150 ms flash, with no pending flash before or after. | | |
| 47 | Tap 10 twice in quick succession → two discrete flashes separated by the forced-off gap. They must not merge into one long pulse. | | |
| 48 | Complete a sequence at the instant the PENDING flash is lit → the forced-off gap fires first, so the cue flash is still readable as a separate event. | | |
| 49 | Hold 1 for 2 s → lockout fast flash appears; no separate cue flash is shown. | | |
| 50 | LED is solid throughout Setup Mode, including while waiting for 6 and 9 to be released after entry. Solid on appears nowhere else in the system. | | |
| 51 | Tap 7 in setup → gap, 7 blinks, gap, back to solid. Tap 4 then 7 quickly → only 7 is blinked; the 4 sequence is cancelled. | | |
| 52 | Exit setup by 6+9 with **no** channel change → channel blink still plays, then LED off. | | |
| 53 | Exit setup by 30 s timeout → channel blink plays identically to a chord exit. | | |
| 54 | Boot on a factory-fresh unit → exactly one blink, then off. | | |
| 55 | Send a cue during an exit channel blink → blink aborts immediately, normal indication resumes, and the cue is transmitted with no added latency. | | |
| 56 | Channel 10 blink completes in roughly 3.8 s and does not delay or block any button handling during that window. | | |

### Robustness

| # | Criterion | Result | Notes |
|---|---|---|---|
| 57 | Rapid repeated presses of a single button produce exactly one event per physical press (50+ presses). | | |
| 60 | LED pattern playback never delays a cue by more than 10 ms (verifies the non-blocking pattern engine). | | |

---

## T23 — 30-minute soak (v1)

| # | Criterion | Result | Notes |
|---|---|---|---|
| 58 | Two buttons pressed simultaneously do not produce a spurious sequence. | | |
| 59 | 30-minute soak with randomised input, including partial chord attempts, produces no hang and no unexpected notes. | | |

**Soak observations**

- Total runtime:
- Cues sent:
- Dropped, stuck or unexpected notes:
- USB re-enumerations or disconnects:
- Stuck LED states:
- Anything that degraded over time:

---

## T29 — per-switch LED bench validation (Phase 9)

A 240 fps camera is required for 64, 65, 66 and 71. Confirm criterion 78 by observation across a
full session rather than from a single capture.

| # | Criterion | Result | Notes |
|---|---|---|---|
| 61 | Tap 1 → LED 1 flashes at a 250 ms period; no other switch LED is lit at any point. | | |
| 62 | Pending window expires with no second input → LED 1 goes off at exactly `SEQUENCE_TIMEOUT_MS` after the tap. | | |
| 63 | Tap 2, then tap 4 → LED 2 extinguishes and LED 4 begins flashing on the same tick; LED 4 runs a fresh `SEQUENCE_TIMEOUT_MS`. | | |
| 64 | Tap 1 then 6 → LEDs 1 and 6 both solid for exactly 5000 ms, then both off. | | |
| 65 | Tap 3 then 3 → LED 3 on 2000, off 1000, on 2000, then off. Distinguishable from criterion 64 by waveform alone. | | |
| 66 | Tap 10 from IDLE → LED 10 alternates at 1000 ms for five phases, ending lit, then off. | | |
| 67 | Tap 2 then tap 10 → LED 2 extinguishes on the same tick LED 10 begins the standalone waveform. | | |
| 68 | Hold 1 to `HOLD_MS`, release at 2100 ms → LED 1 solid from 2000 ms to 7000 ms. The envelope is honoured past release. | | |
| 69 | Hold 1 to `HOLD_MS`, keep holding to 9000 ms → LED 1 stays solid until release. The envelope never truncates while the button is down. | | |
| 70 | Any accepted button press during a running confirmation extinguishes it on that tick. A press discarded by lockout does not. | | |
| 71 | Chord armed → all switch LEDs dark for `CHORD_LED_DELAY_MS`, then 6 and 9 flash in phase at 125/125. A live pending flash is suppressed for the duration. | | |
| 72 | Chord aborted before 5000 ms → the suppressed pending flash resumes on the same tick. | | |
| 73 | Chord re-formed per §6.5 → the 500 ms blackout restarts. | | |
| 74 | Setup entered → the stored channel's LED blinks 1000/1000, including while waiting for 6 and 9 to be released. | | |
| 75 | Tap 3 in setup → the previous channel's LED goes off and LED 3 begins blinking from its **on** phase, on the same tick. | | |
| 76 | Exit setup by 6+9, and separately by 30 s timeout → all ten switch LEDs off. | | |
| 77 | Boot → the stored channel's LED is solid for 2000 ms, then off. A factory-fresh unit lights LED 1. | | |
| 78 | At most two switch LEDs are lit at any instant, in any state, across the full suite. | | |
| 79 | Holding 6, 7, or 9 alone past `HOLD_MS` lights no switch LED. | | |
| 80 | An orphan suffix tap from IDLE lights no switch LED. | | |
| 81 | Criteria 41–56 all still pass unchanged. The panel LED on GP16 and its GP25 mirror are unaffected by this phase. | | |
| 82 | Switch LED playback never delays a cue by more than 10 ms. | | |
| 83 | *(hardware)* All ten LEDs read at comparable brightness from standing height through the finished panel. Any spread is corrected with `SWITCH_LED_DUTY`, not by changing resistors. | | |

**Brightness uniformity (criterion 83)**

Measured from standing height through the finished panel, on a dark stage if possible.

| LED | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|---|
| Reads even? | | | | | | | | | | |
| `SWITCH_LED_DUTY` set | | | | | | | | | | |

Series resistor as built: 100 Ω. Fallback if dim: 82 Ω. **Correct spread with duty, not resistors.**

---

## Failures and follow-up

Any `FAIL`, `TUNED` or `BLOCKED` row above gets an entry here: what was observed, what was changed,
and whether a PRD amendment or a `docs/QUESTIONS.md` entry was raised. `None` if none.

---

## Criteria claimed by no task

**34** — "Enter setup, select nothing, tap 6+9 → exits normally. No flash write, no error, returns to
IDLE." No task's `Covers:` line claims it. The behaviour is almost certainly present via T09
(unconditional exit) and T19 (no-op write skip), but nothing asserts it. Verify it explicitly here,
and consider adding the host test.

---

## T39 — BLE MIDI bench validation (Phase 10)

Host-testable Phase 10 criteria (84–101) are covered by `ctest`. These are the hardware ones. Run 110
in the sealed enclosure, standing on it, **before** deciding anything about shielding (PRD §11.6).

| # | Criterion | Result | Notes |
|---|---|---|---|
| 102 | Pico 2 W enumerates as criteria 1 and 2 | | |
| 103 | GATT scan shows only GAP, GATT, BLE-MIDI; adv data minimal | | Scanner app: |
| 104 | No pairing outside the window; no MIDI without a bond; unbonded drop ~5 s; fresh unit silent | | |
| 105 | All 33 cues over BLE on Mac and iPad | | Middleware: |
| 106 | Mac reconnects after power cycle; iPad reconnect procedure recorded | | iPad needs: |
| 107 | Pair chord + timeout forgets the old host; re-pair after "Forget" works | | |
| 108 | BLE latency median ≤ 20 ms, worst ≤ 40 ms (100 cues); USB still ≤ 10 ms | | Interval granted — Mac: · iPad: |
| 109 | USB plugged in → USB only, no double triggers; unplugged → BLE next cue | | |
| 110 | Range in sealed enclosure; 30-min soak at stage distance | | See table below |
| 111 | Onboard LED (WL_GPIO0) mirrors the panel engine; no GP23/24/25/29 use | | |
| 112 | Zero flash writes during play and across five reconnects | | Instrumented build commit: |
| 113 | 9 V only works; USB only unchanged | | |
| 114 | Channel change saves without freezing; survives power cycle and reflash (both boards, T31+) | | v1.0/v1.1 froze on every config write — PRD §10.1 |

**Range (criterion 110)** — sealed enclosure on the floor, operator standing on it.

| Distance | Host | RSSI (dBm) | Dropouts in 5 min | Notes |
|---|---|---|---|---|
| 3 m | Mac | | | |
| 5 m | Mac | | | |
| 10 m | Mac | | | |
| 3 m | iPad | | | |
| 5 m | iPad | | | |
| 10 m | iPad | | | |

**BLE soak observations (criterion 110)**

- Distance and host:
- Total runtime:
- Cues sent:
- Dropped, duplicated, or late cues:
- Disconnects and how long reconnect took:
- Shielding decision:

