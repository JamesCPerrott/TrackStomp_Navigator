# PRD: USB MIDI Foot Controller — v1

**Target hardware:** Raspberry Pi Pico 2 (RP2350)
**Receiving software:** MultiTracks Playback (MIDI cues are fire-and-forget)
**Scope of this iteration:** 10 momentary footswitches, USB class-compliant MIDI output, user-selectable MIDI channel with persistence, no display, no wireless.

---

## 1. Summary

A USB class-compliant MIDI foot controller that translates two-button tap sequences, single taps, and 2-second holds into MIDI Note On messages. The device drives song-section playback: buttons 1–5 select a section type (Intro, Verse, Chorus, Bridge, Outro), buttons 6–9 select a variant (One, Two, Three, Tag), and every section cue is a two-button sequence.

A hidden setup mode, entered by holding buttons 6 and 9 together for 5 continuous seconds, allows the operator to reassign the MIDI output channel without a computer. The selection persists across power cycles. Factory default is channel 1.

The firmware must be compiled, not interpreted, so that plugging the device into a computer exposes a MIDI endpoint and nothing else — no browsable source, no mass storage volume, no serial console.

---

## 2. Goals

- Enumerate as a class-compliant USB MIDI device on macOS, Windows, Linux, and iOS with no drivers.
- Correctly resolve the tap / hold / chord / two-button-sequence input grammar defined in §6.
- Sub-10 ms latency from button event resolution to MIDI byte on the wire.
- MIDI channel selectable on the device and persisted to flash, defaulting to channel 1 out of the box.
- No source code or filesystem visible to a user who plugs in a USB cable.
- Architecture that admits a display layer in v2 without restructuring the sequencer.

## 3. Non-goals (this iteration)

- Display or any visual feedback beyond the onboard LED.
- Bluetooth or WiFi MIDI.
- MIDI input, MIDI DIN, or expression pedal input.
- User-editable cue mappings. The command table is compile-time constant; only the channel is configurable.
- Preset banks.

---

## 4. Platform decision

**Language and SDK: C++ with the Raspberry Pi Pico SDK and TinyUSB.**

This is driven primarily by the security requirement. The alternatives fail it:

| Option | Why not |
|---|---|
| CircuitPython | Source lives as plain `.py` files on a USB mass-storage volume. Readable by anyone who plugs in a cable. `storage.disable_usb_drive()` hides it, but safe mode restores access and `.mpy` bytecode is trivially decompiled. |
| MicroPython | Same exposure via the REPL, plus `os.listdir()` and file read over serial. |
| Arduino core | Compiled, so acceptable — but it sits on top of the Pico SDK anyway and abstracts away both the RP2350 security features (§5) and the raw flash access needed for §10. |

C++ also gives deterministic timing, which matters for a state machine running concurrent 2 s, 5 s, and chord deadlines alongside a debounce filter.

**Cost to acknowledge:** no REPL, no live editing, a compile-and-flash cycle for every change. Iteration will be slower than CircuitPython. This is accepted as the price of the security requirement.

**Toolchain:** Pico SDK (latest), `arm-none-eabi-gcc`, CMake, Ninja. Target board `pico2`. Libraries: `tinyusb_device` with the audio/MIDI class enabled, `hardware_flash`, `hardware_sync`.

---

## 5. Security requirements

### 5.1 Required for v1 (Tier 1)

- **Compiled binary only.** No interpreted source, no scripts, no data files on the device.
- **No USB mass storage.** The device enumerates exactly one interface: USB MIDI. No CDC, no MSC, no vendor interface.
- **No USB CDC serial in release builds.** A CDC console is an information leak and a debugging foothold. Debug builds may enable stdio over **UART only** (GP0/GP1), gated behind a `DEBUG_UART` CMake option defaulting to `OFF`.
- **USB descriptors** must not embed build paths, developer identity, or internal version strings beyond a simple version number.
- **BOOTSEL physical inaccessibility.** Entering the RP2350 UF2 bootloader requires holding BOOTSEL while power is applied. Mount the Pico 2 fully inside the sealed enclosure so the button cannot be reached without disassembly. This alone defeats casual inspection.

Tier 1 means: someone plugs in a USB cable and sees a MIDI device. There is no filesystem to browse and no console to talk to.

### 5.2 Optional hardening (Tier 2)

The RP2350 supports signed boot with keys held in OTP, and permanent disabling of both the UF2 bootloader path and SWD debug access via OTP fuses. Enabling these means a binary cannot be dumped over SWD and unsigned firmware cannot be loaded.

**These fuse operations are irreversible.** Burning them without a working signed-update path permanently ends your ability to reflash the board. Do not enable Tier 2 until v1 is field-proven and a signed DFU mechanism exists — or until you accept that this specific board becomes write-once.

**Interaction with §10:** the config sector is data, not code. It must sit outside any signed image region so that writing it does not invalidate a boot signature. Plan the memory map before enabling Tier 2.

### 5.3 Honest limitation

Tier 1 stops casual inspection, which is the stated threat model. It does not stop someone with SWD access from dumping flash — **disabling SWD is the control that actually matters**, and that is a Tier 2 fuse. Do not store secrets in the firmware under Tier 1.

---

## 6. Input model

### 6.1 Button map

| # | Label | Tap role | Hold role (2 s) |
|---|---|---|---|
| 1 | Intro | Prefix | Section − |
| 2 | Verse | Prefix | Select |
| 3 | Chorus | Prefix | Auto Reset |
| 4 | Bridge | Prefix | Beginning |
| 5 | Outro | Prefix | Section + |
| 6 | One | Suffix only | none |
| 7 | Two | Suffix only | none |
| 8 | Three | Suffix only | none |
| 9 | Tag | Suffix only | none |
| 10 | Repeat | Standalone → Repeat | Loop |

Plus one chord: **6 + 9 held together for 5 continuous seconds** → enter MIDI Channel Setup Mode (§7).

The chord uses buttons 6 and 9 specifically because neither has an individual hold command. Nothing needs to be deferred, suppressed, or arbitrated on the single-button hold path — the chord cannot collide with Section −, Section +, or any other hold.

Physically arranged as two banks: **Bank A = buttons 1–5** (section selectors), **Bank B = buttons 6–10** (variant modifiers plus Repeat). *(Assumption — see §15.)*

The single-button grammar is **total**: every button in 1–5 accepts every button in 6–9 as a valid suffix, plus itself. No button in 1–5 has a standalone tap command.

### 6.2 Command table

All performance output is Note On, velocity 1, notes 0–31, on the configured channel.

| Note | Function | Trigger |
|---|---|---|
| 0 | Intro 1 | 1 → 6 |
| 1 | Intro 2 | 1 → 7 |
| 2 | Intro 3 | 1 → 8 |
| 3 | Intro Tag | 1 → 9 |
| 4 | Verse 1 | 2 → 6 |
| 5 | Verse 2 | 2 → 7 |
| 6 | Verse 3 | 2 → 8 |
| 7 | Verse Tag | 2 → 9 |
| 8 | Chorus 1 | 3 → 6 |
| 9 | Chorus 2 | 3 → 7 |
| 10 | Chorus 3 | 3 → 8 |
| 11 | Chorus Tag | 3 → 9 |
| 12 | Bridge 1 | 4 → 6 |
| 13 | Bridge 2 | 4 → 7 |
| 14 | Bridge 3 | 4 → 8 |
| 15 | Bridge Tag | 4 → 9 |
| 16 | Outro 1 | 5 → 6 |
| 17 | Outro 2 | 5 → 7 |
| 18 | Outro 3 | 5 → 8 |
| 19 | Outro Tag | 5 → 9 |
| 20 | Repeat | 10 tap |
| 21 | Loop | 10 hold |
| 22 | Title − | 1 → 1 |
| 23 | Play | 2 → 2 |
| 24 | Pad | 3 → 3 |
| 25 | Fade | 4 → 4 |
| 26 | Title + | 5 → 5 |
| 27 | Section − | 1 hold |
| 28 | Select | 2 hold |
| 29 | Auto Reset | 3 hold |
| 30 | Beginning | 4 hold |
| 31 | Section + | 5 hold |

This table must be a single `constexpr` array in `config.h`. Nothing else in the codebase may hardcode a note number or a button pairing.

### 6.3 Tap vs hold resolution

A press cannot be classified until either a hold threshold elapses or the button is released.

- On press: record timestamp, mark button active.
- If held for `HOLD_MS` (2000): fire the hold command immediately at the threshold, not on release. This gives the player a defined moment the action occurs.
- If released before `HOLD_MS`: emit a TAP event, subject to §6.5.
- **Buttons 6, 7, 8, 9 have no hold command.** Held past `HOLD_MS`, they are ignored entirely and their tap is suppressed on release. This prevents a resting foot from firing a cue.

Buttons 1–5 and 10 are unaffected by any chord logic. Their holds behave exactly as specified with no deferral or arbitration.

### 6.4 Hold lockout

**Requirement:** once a hold command fires, all button input is ignored until the held button is released.

- At the instant a hold command fires, the device enters `LOCKED`, keyed to the held button. Any pending sequence is discarded.
- While `LOCKED`: all presses and releases of every other button are discarded. No taps, no holds, no MIDI.
- `LOCKED` ends when the originally held button is released. That release itself produces nothing.
- **On exit, any button still physically pressed is marked tap-suppressed**, so its eventual release does not emit a phantom tap. Without this, a player who mashes a second switch during the lockout gets a spurious cue the moment they lift their foot.
- State returns to `IDLE` on exit, never to a pending sequence.

**Single exception — the 6+9 chord.** The suppression rule in §6.3 that kills a held button 6 or 9 at `HOLD_MS` is waived while both are simultaneously down. Those two buttons remain live past 2 seconds so the chord timer can run to 5. This is the only case in which a button in 6–9 does anything on a hold, and the only lockout any of them can produce.

When the chord fires, the device enters `LOCKED(CHORD)`, which ends only when **both** 6 and 9 are released.

### 6.5 Chord detection

Buttons 6 and 9 form the only chord pair.

**Overlap rule.** If buttons 6 and 9 are ever down at the same instant, both are flagged `chord_overlap` for the remainder of their respective presses. Neither emits a tap on release.

**Timer rule — 5 seconds of continuous simultaneous hold.**

- The chord timer starts the moment the *second* of the pair goes down.
- Both buttons must remain continuously down for the full `CHORD_HOLD_MS` (5000).
- If either is released at any point, the timer is **cleared immediately**, not paused. Nothing fires.
- If the pair later re-forms while the other is still held, a **fresh 5 seconds** begins from that moment.

This means five seconds of holding button 6 while tapping button 9 partway through does **not** enter setup, which is the explicit requirement. Only five seconds of genuine overlap counts.

**Aborted chord fires nothing at all.** Both buttons are consumed silently — no Verse cue, no orphan suffix, no pending sequence. This is deliberately conservative: a partial chord is ambiguous, and sending a wrong cue mid-service is worse than sending none.

There is no timing window for "simultaneous". Any overlap counts. This tolerates the ±100 ms of slop inherent in hitting two footswitches with one foot, which a fixed simultaneity window would not.

---

## 7. MIDI Channel Setup Mode

### 7.1 Entry

Hold buttons 6 and 9 together for `CHORD_HOLD_MS` (5000) of continuous overlap. On reaching the threshold:

1. Discard any pending sequence.
2. Enter `SETUP` state.
3. Enter `LOCKED(CHORD)` until **both** 6 and 9 are released. Nothing is processed until the operator's foot is off.
4. LED goes solid on at the 5 s mark, transitioning directly from the chord flash (§11.4.3). The flash-to-solid change is the entry confirmation.

The 5-second continuous requirement across two non-adjacent buttons makes accidental entry effectively impossible.

### 7.2 Behaviour while in setup

- **All performance MIDI output is suppressed.** No cue is sent for any reason while in `SETUP`. This is non-negotiable — the mode exists to be used mid-service, and a stray Verse 2 during setup would be worse than the problem it solves.
- **Tap of buttons 1–10 selects MIDI channel 1–10** respectively. The selection is held in RAM as `pending_channel`; it is not written to flash and not applied to the MIDI layer until exit.
- Taps of 6 and 9 are subject to the §6.5 overlap rule: a tap of 6 commits to "channel 6" only on release, and only if button 9 was not down at any point during that press. Any overlap is read as the exit gesture instead.
- Selecting a channel replaces any previous selection. Tapping 3 then 7 leaves you on channel 7.
- **Holds do nothing in setup mode.** No hold commands, no hold lockout. Only taps and the exit chord are meaningful.
- Each selection is confirmed by blinking the channel number on the LED (§11.4), so the operator can verify without a display.

### 7.3 Exit

**Tap 6 and 9 together** — both down at once, then both released. Duration is irrelevant; there is no hold threshold inside setup mode. On the release of the second button:

1. Exit setup mode. **Exit always succeeds, whether or not a channel was selected.** There is no requirement to have made a change.
2. If `pending_channel` differs from the stored channel, write it to flash (§10) and apply it to the MIDI layer.
3. If `pending_channel` is unchanged, skip the flash write. Do not burn an erase cycle for a no-op. This is an optimisation only — it never blocks or delays the exit.
4. Blink the active channel number on the LED (§11.4.4). This fires **whether or not the channel changed** — the operator always leaves setup knowing what channel they are on.
5. Return to `IDLE`.

**Inactivity timeout.** If no button is pressed for `SETUP_TIMEOUT_MS` (30000), setup mode exits and **commits** any selection made, then blinks the active channel exactly as a chord exit does. Rationale: a controller stuck in setup mode is dead weight, and every tap in this mode is explicit operator intent, so committing is safer than silently discarding a change the operator believes they made.

There is no explicit cancel-without-saving gesture. *(See §15.)*

### 7.4 Channel range limitation

Ten buttons map to channels 1–10. **Channels 11–16 are unreachable** by design. This is called out explicitly so it is a known constraint rather than a discovered bug. If those channels are ever needed, the natural extension is a modifier gesture — deferred, not designed.

---

## 8. Sequence state machine

### 8.1 States

```
IDLE
PENDING { prefix_button, deadline_ms }
LOCKED  { held_button | CHORD }
SETUP   { pending_channel, inactivity_deadline_ms }
```

### 8.2 Definitions

- **Prefix buttons:** 1, 2, 3, 4, 5. Tap enters PENDING. No standalone command.
- **Suffix buttons:** 6, 7, 8, 9. Meaningful only while PENDING, or as the chord pair {6, 9}.
- **Standalone:** 10. Tap fires Repeat immediately.
- **Hold-capable:** 1, 2, 3, 4, 5, 10.
- **Chord pair:** {6, 9}.
- A PENDING prefix P accepts as valid suffix: 6, 7, 8, 9, and P itself.

### 8.3 Transitions

```
on PRESS(B):
    if state == LOCKED:
        B.suppress_tap = true;  return
    B.press_time = now
    if B in CHORD_PAIR and other_chord_member.pressed:
        B.chord_overlap = true
        other.chord_overlap = true
        chord_start = now                     # second of the pair went down
        chord_armed = true

on HOLD_THRESHOLD(B):                         # press_time + HOLD_MS, still pressed
    if state == LOCKED or state == SETUP: return
    if B in {6,7,8,9}:
        if B in CHORD_PAIR and chord_armed:
            return                            # §6.4 exception: stay live for the chord
        B.suppress_tap = true                 # no hold command, no lockout
        return
    send(hold_note[B]);  B.hold_fired = true
    state = LOCKED(B)                         # discards any PENDING

on CHORD_THRESHOLD:                           # chord_start + CHORD_HOLD_MS, BOTH still down
    if state == SETUP: return                 # no hold semantics inside setup
    enter_setup_mode()
    state = LOCKED(CHORD)                     # until BOTH 6 and 9 released

on RELEASE(B):
    if B in CHORD_PAIR:
        chord_armed = false                   # timer cleared, not paused

    if state == LOCKED:
        if lockout is CHORD and both chord members now released:
            suppress_taps_on_all_pressed_buttons()
            state = SETUP(pending_channel = current_channel)
        elif B == locked_button:
            suppress_taps_on_all_pressed_buttons()
            state = IDLE
        return

    if B.chord_overlap:
        clear B.chord_overlap
        if state == SETUP and other_chord_member also released with overlap:
            exit_setup_mode(commit = true)    # the 6+9 exit tap, always succeeds
        return                                # aborted chord: fires nothing

    if B.hold_fired or B.suppress_tap:
        clear flags;  return
    emit TAP(B)

on TAP(B):
    if state == SETUP:
        pending_channel = B                   # buttons 1-10 -> channels 1-10
        blink_channel(B)
        reset inactivity_deadline
        return

    if state == PENDING(P):
        if B in {6,7,8,9} or B == P:          # always a valid pair
            send(pair_note[P][B]);  state = IDLE
        elif B in {1..5}:
            state = PENDING(B, now + SEQUENCE_TIMEOUT_MS)
        elif B == 10:
            send(REPEAT);  state = IDLE
    else:                                     # IDLE
        if B in {1..5}:
            state = PENDING(B, now + SEQUENCE_TIMEOUT_MS)
        elif B == 10:
            send(REPEAT)
        else:
            ignore                            # orphan suffix

on TIMEOUT:
    if state == PENDING: state = IDLE         # silent
    if state == SETUP and inactivity elapsed: exit_setup_mode(commit = true)
```

### 8.4 Behaviours this produces

- `2, 7` → Verse 2 (note 5). Clean.
- `3, 3` → Pad (note 24). The self-pair check precedes prefix reassignment.
- `2, 4, 8` → Bridge 3 (note 14) only. The abandoned 2 sends nothing.
- Hold 1 alone to 2 s → Section − (note 27) and lockout. Entirely unaffected by chord logic.
- Hold 3 for 2 s while pending on 2 → Auto Reset (note 29) only.
- Press 6, press 9 at +200 ms, hold both to 5.2 s → Setup Mode.
- Press 6, press 9 at +1 s, release 9 at +1.5 s, keep holding 6 to +7 s → **nothing**. Timer cleared on the release of 9; button 6 alone has no hold command.
- Press 6, press 9 at +1 s, release 9 at +1.5 s, press 9 again at +2 s, hold both to +7 s → Setup Mode at +7 s. A fresh 5 seconds ran from the re-form.
- Press 6+9, hold 3 s, release both → **nothing**. Aborted chord, no Verse cue, no orphan.
- Pending on 2, then press 6+9 and hold 5 s → Setup Mode. No Verse 1 sent, pending discarded.
- In setup: tap 4, tap 9, tap 2 → channel 2 pending. Tap 6+9 → channel 2 committed.
- In setup: tap nothing, tap 6+9 → exits cleanly, no flash write.

---

## 9. MIDI output

- **Message:** Note On, velocity 1.
- **Channel:** runtime-configurable, 1–10, loaded from flash at boot, **default 1**. Held in a single variable owned by the MIDI layer; nothing else may cache it.
- **Note Off: not sent.** Playback treats MIDI cues as fire-and-forget triggers. A `SEND_NOTE_OFF` constant (default `false`) is retained so this can be flipped without restructuring, should another host be added later.
- **No output at all while in `SETUP` or `LOCKED`.**
- **No running status optimisation.** Message volume is trivial; clarity beats efficiency.
- **Repeated identical commands** are sent every time, with no suppression.

**USB device identity:**

- Manufacturer, product string, and a stable serial number set explicitly in the descriptors — do not rely on SDK defaults, which leak board identifiers.
- Product string appears in the host's MIDI device list; choose something recognisable (e.g. `Section Controller`).
- One MIDI interface, one virtual cable, one IN endpoint. An OUT endpoint is not required in v1 but may be declared for future use.

---

## 10. Persistent configuration

The RP2350 has no EEPROM. Configuration is stored in a reserved flash sector using `hardware_flash`.

### 10.1 Layout

- Reserve the **last 4 KB sector** of on-board flash. Exclude it from the linker's image region so a firmware update does not clobber it.
- Record format:

```cpp
struct ConfigRecord {
    uint32_t magic;      // 0x4D494449 ('MIDI')
    uint16_t version;    // 1
    uint8_t  channel;    // 1..10
    uint8_t  reserved;
    uint32_t crc32;      // over all preceding bytes
};
```

### 10.2 Read and factory default

At boot, read the sector. If `magic` mismatches, `version` is unknown, `crc32` fails, or `channel` is outside 1–10, **fall back to channel 1 and do not write**.

A device fresh from the factory has erased flash (all `0xFF`), which fails validation cleanly and yields channel 1. This is the specified out-of-the-box behaviour, and it is achieved by never writing at boot — only on an explicit setup exit. A brand-new unit therefore has an unwritten config sector and still behaves correctly.

### 10.3 Write

Writes occur only on exit from Setup Mode, and only when the channel actually changed.

Implementation constraints that will bite if ignored:

- Flash erase and program require **XIP to be disabled**, so the code performing them must run from RAM. Mark those functions `__not_in_flash_func`.
- **Disable interrupts** for the duration (`save_and_disable_interrupts()` / `restore_interrupts()`). Any ISR that executes from flash during the operation will hard fault.
- Core 1 must be idle or parked. It is unused in v1, but this becomes a real constraint in v2 when the display may run there — use `flash_safe_execute` rather than raw calls once that happens.
- USB will stall for the few milliseconds the operation takes. Acceptable because it happens only on setup exit, never during performance. Do not attempt a flash write from any performance path.

### 10.4 Wear

One erase cycle per channel change. Flash endurance is on the order of 100,000 cycles, so this is a non-issue at human interaction rates — provided the no-op skip in §7.3 is implemented and no code path writes on every boot.

---

## 11. Hardware interface

### 11.1 Pin assignment

| Signal | GPIO |
|---|---|
| Buttons 1–10 | GP6 – GP15 (contiguous, in order) |
| Reserved: I2C0 SDA / SCL (v2 display) | GP4 / GP5 |
| Reserved: ADC (future expression) | GP26 / GP27 / GP28 |
| Status LED | GP25 (onboard) |
| Debug UART (debug builds only) | GP0 / GP1 |

Contiguous button pins allow a single masked `gpio_get_all()` read per scan.

### 11.2 Electrical

- Each switch wires common → GND, one throw → its GPIO.
- **Internal pull-ups only.** The RP2350 has a known erratum affecting pull-downs: a GPIO input pad can leak enough current that an internal pull-up cannot hold it high, and external pull-downs would need to be 8.2 kΩ or lower. Pull-up plus switch-to-ground sidesteps this entirely. Do not design any input around a pull-down.
- Active-low logic: pressed reads 0.

### 11.3 Debounce

- Poll every `SCAN_INTERVAL_MS` (1 ms).
- A state change is accepted only after the new level is stable for `DEBOUNCE_MS` (default **20**). Footswitches bounce far worse than fingers; do not reduce below 15 without measuring.
- Debounce is per-button and independent. Chord detection (§6.5) operates on debounced state, never raw. In particular, `chord_armed` must not be cleared by a contact bounce on 6 or 9 — this is precisely the failure that would make a 5-second chord unreachable in practice.

### 11.4 LED feedback

The single LED on GP25 is the only feedback available before the display exists. Setup Mode is unusable without it, so these patterns are a requirement, not a nicety.

#### 11.4.1 Priority stack

Higher entries override lower ones completely. The LED shows exactly one thing at a time.

| Priority | Source | Pattern |
|---|---|---|
| 1 | Channel blink sequence (§11.4.4) | See below |
| 2 | Chord progress (§11.4.3) | See below |
| 3 | Setup Mode | **Solid on** |
| 4 | Hold lockout | Fast flash, 100 ms period (50 on / 50 off) |
| 5 | Cue sent (§11.4.2) | Single 150 ms flash |
| 6 | PENDING sequence | Slow flash, 1000 ms period (500 on / 500 off) |
| 7 | IDLE | Off |

**Solid on now means Setup Mode and nothing else.** Each pattern is distinguishable by rate alone: 100 ms lockout, 250 ms chord, 1000 ms pending. No two are within a factor of two of each other, which matters when the only thing you can see is a small LED on a dark floor.

#### 11.4.2 Cue confirmation

Every transmitted cue produces **exactly one visible flash** of `CUE_FLASH_MS` (150 ms).

- **A tap of button 10** (Repeat) is the case this matters most for. It is the only command that fires from IDLE with no preceding PENDING state, so the flash is its sole confirmation that anything happened. One tap, one flash.
- **Sequence completions** use the same flash. The slow PENDING flash stops and a single brighter-reading pulse replaces it, so the end of a sequence is unambiguous.
- **If the LED is lit at the moment the cue fires**, insert a forced-off gap of `CUE_FLASH_GAP_MS` (100 ms) before the flash. Without this, a cue landing mid-pulse during a PENDING slow flash would merge into it and read as nothing at all. The gap costs nothing — it is visual only and never delays the MIDI byte.
- **Hold-triggered cues** are the exception: they immediately enter lockout, which outranks the cue flash. The fast lockout flash is the confirmation in that case.

#### 11.4.3 Chord progress

While the 6+9 chord timer is armed:

- **0 – 500 ms (`CHORD_LED_DELAY_MS`): LED forced off.** This overrides whatever sits below it, including a slow PENDING flash. Nothing is displayed until the operator has held long enough to mean it — a brief accidental graze of both buttons must not twitch the LED.
- **500 – 5000 ms: flash** at `CHORD_LED_FLASH_MS` (125 ms on, 125 ms off — a 250 ms period).
- **At 5000 ms:** straight to solid on as Setup Mode is entered. The flash-to-solid transition *is* the entry confirmation; there is no separate flourish.
- **On abort** (either button released before 5000 ms): the override is released immediately and the LED returns to whatever state sits beneath it.
- **On re-form** (§6.5): the 500 ms blackout restarts along with the timer. Off, then flashing again. This makes a restarted timer visible rather than silent.

#### 11.4.4 Channel blink sequence

A reusable primitive that blinks a channel number N.

```
400 ms off  →  N × (150 ms on / 150 ms off)  →  400 ms off  →  return to underlying state
```

Longest case, channel 10, runs about 3.8 seconds.

**Invoked under all four circumstances in which the channel is established or confirmed:**

| Trigger | Returns to |
|---|---|
| Boot, showing the stored channel | IDLE (off) |
| Channel selected in Setup Mode (reprogram) | Setup solid |
| Exit by 6+9 chord | IDLE (off) |
| Exit by 30 s inactivity timeout | IDLE (off) |

Rules:

- A new channel blink **cancels any blink in progress** and restarts. Rapid taps of 4 then 7 in setup show only the 7.
- The blink is **purely visual and never gates the state machine.** Input handling, sequence timing, and MIDI transmission continue normally underneath it. Do not block on it.
- **Aborted by any cue send.** If the operator resumes playing during an exit blink, the sequence is dropped and normal indication takes over. Musical feedback outranks a confirmation.

The boot-time blink means the operator always knows the current channel without entering Setup Mode at all. On a factory-fresh unit this is a single blink.

---

## 12. Architecture

```
src/
  main.cpp            Init, main loop, tick dispatch
  config.h            ALL tunables and the command table. Single source of truth.
  input/
    buttons.h/.cpp    GPIO scan, debounce, tap/hold/chord classification, lockout
                      Emits: ButtonEvent { uint8_t id; enum { TAP, HOLD, CHORD_HOLD }; }
  sequencer/
    sequencer.h/.cpp  The §8 state machine, including SETUP
                      Consumes ButtonEvent, emits Command and UiEvent
  midi/
    midi_out.h/.cpp   TinyUSB MIDI send; owns the active channel
    usb_descriptors.c Explicit descriptors, MIDI-only
  storage/
    config_store.h/.cpp  Flash read/write/validate per §10
  ui/
    ui.h/.cpp         v1: LED patterns. v2: display driver plugs in here.
                      Consumes UiEvent { IDLE, PENDING(b), LOCKED, SENT(note),
                                         SETUP_ENTER, SETUP_CHANNEL(n), SETUP_EXIT }
```

### 12.1 Layer rules

- `sequencer` must not call MIDI, GPIO, or flash functions directly. It consumes `ButtonEvent` and emits `Command` and `UiEvent`. This is what makes the v2 display a drop-in.
- The hold lockout (§6.4) and chord detection (§6.5) live in `input`, not `sequencer` — they are input-classification concerns, and keeping them there means the sequencer never sees the discarded events at all.
- `input` knows nothing about the command table or about channels. It reports taps, holds, and chord-holds; meaning is the sequencer's job.
- `config_store` is called only at boot (read) and on setup exit (write). No other call site.
- All timing constants live in `config.h`. No magic numbers anywhere else.

### 12.2 Main loop

Single-threaded cooperative loop, no interrupts required at this event rate:

```
while (true) {
    tud_task();                    // TinyUSB housekeeping — must run every iteration
    uint32_t now = time_ms();
    buttons_scan(now);             // → ButtonEvents
    sequencer_tick(now);           // handles events + all deadline expiry
    ui_tick(now);                  // LED pattern engine, non-blocking
    sleep_us(500);
}
```

The LED pattern engine must be a non-blocking state machine. Do not implement blink patterns with `sleep_ms` — that would stall `tud_task()` and drop USB.

Core 1 is unused in v1 and reserved for the display in v2.

---

## 13. Configuration constants

```cpp
constexpr uint32_t SCAN_INTERVAL_MS     = 1;
constexpr uint32_t DEBOUNCE_MS          = 20;
constexpr uint32_t HOLD_MS              = 2000;
constexpr uint32_t CHORD_HOLD_MS        = 5000;
constexpr uint32_t SEQUENCE_TIMEOUT_MS  = 5000;
constexpr uint32_t SETUP_TIMEOUT_MS     = 30000;
constexpr uint8_t  DEFAULT_MIDI_CHANNEL = 1;     // 1-based; wire value is n-1
constexpr uint8_t  MAX_MIDI_CHANNEL     = 10;    // limited by button count
constexpr uint8_t  MIDI_VELOCITY        = 1;
constexpr bool     SEND_NOTE_OFF        = false; // Playback cues are fire-and-forget
constexpr uint8_t  BUTTON_GPIO_BASE     = 6;
constexpr uint8_t  BUTTON_COUNT         = 10;
constexpr uint8_t  CHORD_BUTTON_A       = 6;
constexpr uint8_t  CHORD_BUTTON_B       = 9;

// LED timing
constexpr uint32_t CHORD_LED_DELAY_MS   = 500;   // silent window at chord start
constexpr uint32_t CHORD_LED_FLASH_MS   = 125;   // on and off; 250 ms period
constexpr uint32_t LOCKOUT_BLINK_MS     = 50;    // on and off; 100 ms period
constexpr uint32_t PENDING_FLASH_MS     = 500;   // on and off; 1000 ms period
constexpr uint32_t CUE_FLASH_MS         = 150;
constexpr uint32_t CUE_FLASH_GAP_MS     = 100;   // forced off before a cue flash
constexpr uint32_t CHANNEL_BLINK_GAP_MS = 400;
constexpr uint32_t CHANNEL_BLINK_ON_MS  = 150;
constexpr uint32_t CHANNEL_BLINK_OFF_MS = 150;
```

---

## 14. Acceptance criteria

**Enumeration**
1. Device appears as a class-compliant MIDI input on macOS, Windows, and iOS with no drivers.
2. No mass storage volume mounts. No serial port appears. Release build only.
3. MultiTracks Playback receives and triggers on all 32 cues.

**Sequences**
4. Tap 1 then 6 → note 0 only.
5. Tap 5 then 9 → note 19 only.
6. Tap 3 then 3 → note 24 only (self-pair precedes prefix reassignment).
7. Tap 2 then 4 then 8 → note 14 only. No Verse note.
8. Tap 2, wait 6 s → nothing sent, state returns to IDLE.
9. Tap 2, wait 6 s, tap 6 → nothing sent.
10. Tap 6 with no pending prefix → nothing sent, no state change.
11. Tap 10 → note 20 within 10 ms of release.
12. Tap 2 then tap 10 → note 20 only, pending Verse discarded.

**Holds (unaffected by chord logic)**
13. Hold 1 for 2 s → note 27 at the 2 s mark, while still held. Release sends nothing.
14. Hold 5 for 2 s → note 31. Confirms buttons 1 and 5 have no residual chord behaviour.
15. Hold 10 for 2 s → note 21 at the 2 s mark.
16. Tap 2 then hold 5 for 2 s → note 31 only. No Verse note.
17. Tap 2, then press 2 and hold past 2 s → note 28 only. No note 23.
18. Hold 7 for 3 s and release → nothing sent, no lockout entered.

**Lockout**
19. Hold 1 for 2 s (note 27 fires). While still holding, press and release 6, 7, 3 → nothing further sent. Release 1 → nothing sent.
20. Hold 1 for 2 s. While still holding 1, press and hold 3. Release 1, then release 3 → nothing sent from 3.
21. After any lockout ends, tap 2 then 7 → note 5.

**Chord timing — the strict 5-second rule**
22. Press 6 and 9 together, hold 5 s → setup entered. Nothing else sent.
23. Press 6, press 9 at +1 s, hold both → setup entered at +6 s, not +5 s. Timer runs from the second press.
24. **Press 6, press 9 at +1 s, release 9 at +1.5 s, keep holding 6 to +7 s → no setup, nothing sent.** The explicit requirement.
25. Hold 6 alone for 10 s → nothing sent, no setup.
26. Press 6, press 9 at +1 s, release 9 at +1.5 s, press 9 again at +2 s, hold both → setup at +7 s. Fresh timer on re-form.
27. Press 6+9, hold 4 s, release both → nothing sent. No Verse cue, no orphan suffix, no pending sequence.
28. Pending on 2, then press 6+9 and hold 5 s → setup entered, no Verse 1 sent.
29. Contact bounce on button 9 during a 5 s chord hold does not clear the timer (verify with an instrumented debug build).

**Setup mode**
30. On entry, release both 6 and 9 → slow blink begins. No MIDI sent throughout.
31. While in setup, tap 2 then 7 → **no MIDI sent**. LED blinks 2, then 7.
32. While in setup, hold 3 for 3 s → nothing happens. No note 29.
33. While in setup, tap 8 → LED blinks 8 times. Tap 6+9 and release → exit pattern. Subsequent cues transmit on channel 8.
34. **Enter setup, select nothing, tap 6+9 → exits normally.** No flash write, no error, returns to IDLE.
35. Enter setup, tap 6 alone → channel 6 selected. Enter setup again, tap 9 alone → channel 9 selected. Confirms the chord buttons still work as individual channel selectors.
36. Power cycle after selecting channel 8 → boot blink shows 8. Cues transmit on channel 8.
37. Enter setup, change nothing, exit → no flash erase occurs (instrumented debug build).
38. Enter setup, tap 4, wait 30 s untouched → auto-exit, channel 4 committed.

**Factory default**
39. Erase flash entirely and boot → boot blink shows 1 (single blink). Cues transmit on channel 1. No write occurs at boot.
40. Corrupt the config sector's CRC and boot → falls back to channel 1 without hanging.

**LED indication**
41. Press 6+9. LED stays **off** for 500 ms, then flashes at a 250 ms period until 5 s, then goes solid. Verify with a scope or slow-motion video.
42. Press 6+9 while a sequence is PENDING (LED slow-flashing). LED goes hard off at chord start, not straight to the faster chord flash.
43. Press 6+9, release at 300 ms → LED returns to its prior state with no flash ever shown.
44. Press 6+9, release 9 at 2 s, re-press 9 at 2.5 s → LED goes off again for 500 ms from the re-press, then resumes flashing.
45. Tap 2 then 7. LED slow-flashes at a 1000 ms period while pending, then shows one 150 ms flash on the cue. Solid on never appears.
46. Tap 10 from idle → exactly one 150 ms flash, with no pending flash before or after.
47. Tap 10 twice in quick succession → two discrete flashes separated by the forced-off gap. They must not merge into one long pulse.
48. Complete a sequence at the instant the PENDING flash is lit → the forced-off gap fires first, so the cue flash is still readable as a separate event.
49. Hold 1 for 2 s → lockout fast flash appears; no separate cue flash is shown.
50. LED is solid throughout Setup Mode, including while waiting for 6 and 9 to be released after entry. Solid on appears nowhere else in the system.
51. Tap 7 in setup → gap, 7 blinks, gap, back to solid. Tap 4 then 7 quickly → only 7 is blinked; the 4 sequence is cancelled.
52. Exit setup by 6+9 with **no** channel change → channel blink still plays, then LED off.
53. Exit setup by 30 s timeout → channel blink plays identically to a chord exit.
54. Boot on a factory-fresh unit → exactly one blink, then off.
55. Send a cue during an exit channel blink → blink aborts immediately, normal indication resumes, and the cue is transmitted with no added latency.
56. Channel 10 blink completes in roughly 3.8 s and does not delay or block any button handling during that window.

**Robustness**
57. Rapid repeated presses of a single button produce exactly one event per physical press (50+ presses).
58. Two buttons pressed simultaneously do not produce a spurious sequence.
59. 30-minute soak with randomised input, including partial chord attempts, produces no hang and no unexpected notes.
60. LED pattern playback never delays a cue by more than 10 ms (verifies the non-blocking pattern engine).

---

## 15. Open questions and assumptions

Each was decided to unblock implementation. Flag any that are wrong.

| # | Question | Assumed |
|---|---|---|
| 1 | Should the chord flash accelerate as it approaches 5 s, rather than running at a steady 250 ms? | Steady. Acceleration would communicate progress better but was not requested. |
| 2 | Buttons 6 and 9 are both chord members *and* channel selectors (channels 6 and 9). Any press of one that overlaps the other reads as the exit gesture, not a channel selection. Acceptable? | Yes. Deterministic and LED-confirmed, but the operator must tap 6 or 9 cleanly to select those channels. |
| 3 | Should an aborted chord fire anything? | No. Silent. A wrong cue mid-service is worse than none. |
| 4 | Setup auto-exit at 30 s — commit or discard? | **Commit.** A controller stuck in setup is dead weight, and taps are explicit intent. |
| 5 | Is a cancel-without-saving gesture needed? | Not implemented. It would be a third gesture with no display to prompt for it. Reconsider in v2. |
| 6 | Channels 11–16 are unreachable. Acceptable? | Yes. Called out in §7.4. |
| 7 | While a sequence is pending, does tapping 10 fire Repeat and clear the pending prefix? | Yes. |
| 8 | Should buttons 7 and 8 do anything when held past 2 s? | No — ignored, tap suppressed, no lockout. Only 6 and 9 have the chord exception. |
| 9 | Is the bank split 1–5 / 6–10 as assumed? | Yes. Affects documentation and v2 display layout only. |
| 10 | Are 2 s (hold) and 5 s (chord) right under a shoe? | Per spec. Both tunable. |
| 11 | Security tier target? | Tier 1 for v1. Tier 2 fuses are irreversible; wait for a signed update path. |

---

## 16. Future iterations

**v2 — Display.** I2C on GP4/GP5, reserved. Consumes the existing `UiEvent` stream. Must show pending-sequence state (e.g. `VERSE → ?`), lockout state, the last cue sent, and — importantly — replace the LED blink-counting in Setup Mode with a legible channel readout. If the display runs on core 1, switch flash writes to `flash_safe_execute` (§10.3).

**v3 — Wireless.** The Pico 2 has no radio; Bluetooth or WiFi MIDI requires a Pico 2 W. Confirm the board variant before committing. BLE MIDI is not class-compliant USB and will need a separate transport beneath `midi_out`.

**Deferred.** Expression pedal input (ADC pins reserved), runtime-editable cue mappings, preset banks, channels 11–16 via a modifier gesture.
