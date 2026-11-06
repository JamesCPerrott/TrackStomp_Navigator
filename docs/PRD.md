# PRD: USB MIDI Foot Controller — v1

**Target hardware:** Raspberry Pi Pico 2 W (RP2350 + CYW43439 radio). The plain Pico 2 remains a supported **USB-only** build until Phase 10 hardware is in hand (§11.1).
**Receiving software:** MultiTracks Playback on **macOS and iPad only** (MIDI cues are fire-and-forget)
**Scope:** 10 momentary footswitches, USB class-compliant MIDI output, **BLE MIDI output (Phase 10, §9.3)**, user-selectable MIDI channel with persistence, ten per-switch status LEDs (§11.5), no display.

**Revision history.** v1 (T01–T21) — USB MIDI. Phase 9 (T24–T29) — per-switch status LEDs, §11.5. **Phase 10 (T30–T39) — BLE MIDI on the Pico 2 W:** §2, §3, §4, §5.4, §7.5, §8.5, §9.3–§9.4, §10.1, §10.3, §11.1, §11.4 (board note), §11.5.3, §11.5.8, §11.6, §11.7, §12, §13, §14 (84–114), §15 (18–31), §16. A display is not planned (§16).

---

## 1. Summary

A USB class-compliant MIDI foot controller that translates two-button tap sequences, single taps, and 2-second holds into MIDI Note On messages. The device drives song-section playback: buttons 1–5 select a section type (Intro, Verse, Chorus, Bridge, Outro), buttons 6–9 select a variant (One, Two, Three, Tag), and every section cue is a two-button sequence.

A hidden setup mode, entered by holding buttons 6 and 9 together for 5 continuous seconds, allows the operator to reassign the MIDI output channel without a computer. The selection persists across power cycles. Factory default is channel 1.

The firmware must be compiled, not interpreted, so that plugging the device into a computer exposes a MIDI endpoint and nothing else — no browsable source, no mass storage volume, no serial console.

---

## 2. Goals

- Enumerate as a class-compliant USB MIDI device on macOS, Windows, Linux, and iOS with no drivers.
- Correctly resolve the tap / hold / chord / two-button-sequence input grammar defined in §6.
- Sub-10 ms latency from button event resolution to MIDI byte on the wire **over USB**. BLE has its own measured budget (§9.3.5), because BLE delivers data only at connection events.
- **BLE MIDI to macOS and iPad (Phase 10)**, with persistent pairing made from Setup Mode, so the pedal can run cable-free to the host from the 9 V supply. USB always wins when both are connected (§9.4).
- MIDI channel selectable on the device and persisted to flash, defaulting to channel 1 out of the box.
- No source code or filesystem visible to a user who plugs in a USB cable.
- Architecture that admits a second MIDI transport (BLE) beneath `midi_out` without touching `input`, `sequencer`, or the `UiEvent` contract.

## 3. Non-goals (this iteration)

- Display. Not planned (§16). Feedback is the ten per-switch LEDs (§11.5) and the panel LED (§11.4).
- WiFi, RTP-MIDI / network MIDI, and Bluetooth Classic. Phase 10 adds **BLE MIDI only**.
- Windows, Linux, or Android as BLE hosts. USB remains class-compliant everywhere; BLE is specified and validated against macOS and iPad only.
- Wireless firmware update, or any BLE service other than BLE-MIDI (§5.4).
- Battery power. Cable-free operation runs from the 9 V input (§11.7).
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

**Toolchain:** Pico SDK (latest), `arm-none-eabi-gcc`, CMake, Ninja. Target boards **`pico2_w`** (primary, Phase 10) and `pico2` (USB-only, retained until the Pico 2 W is fitted). Libraries: `pico_stdlib` (crt0, linker script, runtime — required for a valid UF2), `tinyusb_device` with the audio/MIDI class enabled, `hardware_flash`, `hardware_sync`. `pico_add_extra_outputs()` must be called on the target or no `.uf2` is generated.

**Phase 10 additions, `pico2_w` only:** `pico_cyw43_arch_poll` (radio driver, polled from the main loop — no background IRQ work), `pico_btstack_ble`, `pico_btstack_cyw43`, and the SDK's `pico_btstack_make_gatt_header()` CMake function to compile the GATT database. **Never link `pico_btstack_classic`, any `lwip` variant, or `pico_cyw43_arch_threadsafe_background`.** These are the only new dependencies Phase 10 approves; anything else is a BLOCKER.

**BTstack licence.** BTstack is commercially licensed by BlueKitchen; Raspberry Pi holds a licence covering its use on Raspberry Pi RP-series microcontrollers, which is what makes it usable in closed firmware here. Confirm the terms in `$PICO_SDK_PATH/lib/btstack/LICENSE` before the device leaves the bench — this sits beside the PID item in §9.1.

**SDK sourcing.** The SDK is external, located via the `PICO_SDK_PATH` environment variable — it is not vendored or submoduled into this repo. `pico_sdk_import.cmake` is copied from `$PICO_SDK_PATH/external/` into the repo root as the standard bootstrap shim. Since the repo cannot pin the SDK version this way, `CMakeLists.txt` must assert a minimum `PICO_SDK_VERSION_STRING` and fail at configure time if unmet.

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

### 5.4 Bluetooth posture (Phase 10)

A discoverable radio cannot keep the literal promise of §5.1 — "plug in a cable and see MIDI, nothing else" — because nobody has to plug anything in. Phase 10 restates Tier 1 for the radio: **someone scanning sees at most a named MIDI peripheral, and cannot exchange MIDI without a bond made during a pairing window that needs physical access to the pedal.**

- **One service.** The GATT database contains exactly the mandatory GAP (0x1800) and GATT (0x1801) services and the BLE-MIDI service (§9.3). **No Device Information Service, no Battery Service, no OTA/DFU, no UART-over-BLE, no HID, no vendor service.** The GAP Device Name is `TrackStomp Navigator`; nothing else identifies the firmware, its version, or its author.
- **No Bluetooth Classic.** The radio runs BLE only.
- **Pairing only inside the pairing window.** The window opens only from Setup Mode (§7.5): 5 s on 6+9, then 2 s on 2+4, standing on the pedal. Outside the window every pairing request is **declined**, including from a host that was bonded before.
- **The MIDI characteristic requires encryption.** A central that connects without a bond can discover the services but cannot read, write, or subscribe to MIDI I/O. An unbonded connection is dropped after `BLE_UNBONDED_DROP_MS`.
- **One bond, persistent.** At most `BLE_MAX_BONDS` (1) bonded host. Bonds survive power cycles (§10.1). Opening a new pairing window **clears the existing bond first** (§7.5).
- **Advertising is limited.** With no bond, the pedal does not advertise at all outside a pairing window — a factory-fresh unit is radio-silent. With a bond, it advertises connectable so its host can reconnect; only that host can then do anything (above).
- **Advertising carries** the flags, the 128-bit BLE-MIDI service UUID, and — in the scan response — the complete local name. Nothing else.

**Honest limitation — Just Works.** The pedal has no display and no keypad, so pairing uses LE Secure Connections **Just Works** (no MITM protection). An attacker within radio range *during the pairing window* could interpose. The window is short, opened deliberately on the floor, and shown on the LEDs; this is accepted (§15 row 18). Outside the window the attack surface is a connectable advertiser that refuses to pair.

**Debug builds.** `DEBUG_UART` behaviour is unchanged. BTstack logging, HCI dump, and packet logging go to the UART only in debug builds and are compiled out of release builds; nothing Bluetooth-related may ever expose a console over the air.

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
| 8 | Three | Suffix only | Mute MIDI |
| 9 | Tag | Suffix only | none |
| 10 | Repeat | Repeat | Loop |

Plus one chord: **6 + 9 held together for 5 continuous seconds** → enter MIDI Channel Setup Mode (§7).

The chord uses buttons 6 and 9 specifically because neither has an individual hold command. Nothing needs to be deferred, suppressed, or arbitrated on the single-button hold path — the chord cannot collide with Section −, Section +, or any other hold.

Physically arranged as two banks: **Bank A = buttons 1–5** (section selectors), **Bank B = buttons 6–10** (variant modifiers plus Repeat). *(Assumption — see §15.)*

The single-button grammar is **total**: every button in 1–5 accepts every button in 6–9 as a valid suffix, plus itself. No button in 1–5 has a standalone tap command.

### 6.2 Command table

All performance output is Note On, velocity 1, notes 0–32, on the configured channel.

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
| 32 | Mute MIDI | 8 hold |

This table must be a single `constexpr` array in `config.h`. Nothing else in the codebase may hardcode a note number or a button pairing.

### 6.3 Tap vs hold resolution

A press cannot be classified until either a hold threshold elapses or the button is released.

- On press: record timestamp, mark button active.
- If held for `HOLD_MS` (2000): fire the hold command immediately at the threshold, not on release. This gives the player a defined moment the action occurs.
- If released before `HOLD_MS`: emit a TAP event, subject to §6.5.
- **Hold-capable buttons are 1–5, 8, and 10.**
- **Buttons 6, 7, and 9 have no hold command.** Held past `HOLD_MS`, they are ignored entirely and their tap is suppressed on release. This prevents a resting foot from firing a cue.
- **Button 8 is the exception among the suffix buttons**, holding to Mute MIDI (note 32). It was chosen over 6, 7, or 9 because it is not a chord member and therefore cannot interfere with Setup Mode entry. Note the tradeoff: a foot resting on button 8 for two seconds mutes Playback's outgoing automation, where the same lean on 6, 7, or 9 does nothing.

Buttons 1–5 and 10 are unaffected by any chord logic. Their holds behave exactly as specified with no deferral or arbitration.

### 6.4 Hold lockout

**Requirement:** once a hold command fires, all button input is ignored until the held button is released.

- At the instant a hold command fires, the device enters `LOCKED`, keyed to the held button. Any pending sequence is discarded.
- While `LOCKED`: all presses and releases of every other button are discarded. No taps, no holds, no MIDI.
- `LOCKED` ends when the originally held button is released. That release itself produces nothing.
- **On exit, any button still physically pressed is marked tap-suppressed**, so its eventual release does not emit a phantom tap. Without this, a player who mashes a second switch during the lockout gets a spurious cue the moment they lift their foot.
- State returns to `IDLE` on exit, never to a pending sequence.

**Single exception — the 6+9 chord.** The suppression rule in §6.3 that kills a held button 6 or 9 at `HOLD_MS` is waived while both are simultaneously down. Those two buttons remain live past 2 seconds so the chord timer can run to 5. Buttons 6 and 9 have no hold command of their own, so the chord is the only thing their hold can trigger.

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

### 7.5 Bluetooth pairing (Phase 10, `pico2_w` only)

Pairing lives inside Setup Mode so that it inherits Setup Mode's protection against accidental entry.

**Pair chord.** While in `SETUP` (after the entry pair has been released), hold **buttons 2 and 4 together for `PAIR_CHORD_HOLD_MS` (2000) of continuous overlap.** The rules are the §6.5 rules applied to a second pair:

- The timer starts when the second of 2 and 4 goes down, requires continuous overlap, is **cleared** (not paused) if either is released, and restarts in full if the pair re-forms.
- Any overlap of 2 and 4 flags both `pair_overlap` for the rest of their presses: **neither selects a channel on release**, whether or not the chord completed. To select channel 2 or 4, tap it cleanly. This is the same trade-off §15 row 2 accepted for 6 and 9.
- **The pair chord exists only in `SETUP`.** Outside it, 2 and 4 behave exactly as §6 says — two prefixes with holds (Select, Beginning). Nothing about performance input changes.

**On reaching the threshold:**

1. **Clear the stored bond**, if any (§10.1). The previously bonded host can no longer exchange MIDI. Clearing is the explicit intent of the gesture — it is also how the pedal is made to forget a host.
2. Open the **pairing window** for `PAIRING_WINDOW_MS` (60000) and begin advertising for pairing.
3. LEDs 2 and 4 show the pairing indication (§11.5.8).

**While the pairing window is open:**

- **Taps of 1–10 select nothing.** The window owns the surface; a channel change would be invisible.
- The `SETUP_TIMEOUT_MS` inactivity timeout is **suspended**. It restarts from the moment the window closes.
- A pairing request from a central is accepted (Just Works, §5.4). While pairing is in progress, the indication changes to *bonding*.
- A failed pairing returns to *advertising*. The window deadline is not extended.
- **The 2+4 chord again restarts the window** (and clears any bond made in it).
- **The 6+9 exit gesture closes the window** and exits Setup Mode exactly as §7.3 says. Advertising for pairing stops.

**The window closes on:**

| Event | Result |
|---|---|
| Bond succeeds | Bond persisted. LEDs 2 and 4 give one long flash (§11.5.8), then the Setup channel indication resumes from its on phase. Setup inactivity timer restarts. Still in `SETUP`. |
| `PAIRING_WINDOW_MS` elapses with no bond | Advertising stops. **The pedal is left unpaired** (the bond was cleared at step 1). Setup channel indication resumes; inactivity timer restarts. |
| 6+9 exit | As §7.3. Unpaired unless a bond completed first. |

**Host side.** Clearing the bond on the pedal does not clear the host's record. After a re-pair, the operator must "Forget" the old TrackStomp entry on the Mac (System Settings → Bluetooth) or in the iPad's Bluetooth settings, then pair again from Audio MIDI Setup (Mac) or the BLE MIDI middleware app (iPad). This belongs in the operator notes, not the firmware.

**Plain Pico 2 builds** have no radio. The 2+4 chord is still detected in `SETUP` (the input layer is board-independent) but the link layer reports "no radio", so the window closes immediately with no indication and taps resume. This keeps the host tests board-independent.

---

## 8. Sequence state machine

### 8.1 States

```
IDLE
PENDING { prefix_button, deadline_ms }
LOCKED  { held_button | CHORD }
SETUP   { pending_channel, inactivity_deadline_ms, pairing_window_open }
```

`pairing_window_open` is owned by the link layer (§8.5, §12). The sequencer reads it; it does not time it.

### 8.2 Definitions

- **Prefix buttons:** 1, 2, 3, 4, 5. Tap enters PENDING. No standalone command.
- **Suffix buttons:** 6, 7, 8, 9. Meaningful only while PENDING, or as the chord pair {6, 9}.
- **Standalone:** 10. Tap fires Repeat immediately.
- **Hold-capable:** 1, 2, 3, 4, 5, 8, 10.
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
    if B in {6,7,9}:                          # no hold command
        if B in CHORD_PAIR and chord_armed:
            return                            # §6.4 exception: stay live for the chord
        B.suppress_tap = true                 # no lockout
        return
    send(hold_note[B]);  B.hold_fired = true  # buttons 1-5, 8, 10
    chord_armed = false                       # a lockout cancels a part-formed chord
    state = LOCKED(B)                         # discards any PENDING

on CHORD_THRESHOLD:                           # chord_start + CHORD_HOLD_MS, BOTH still down
    if state == SETUP or state == LOCKED: return
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
- Tap 10 → Repeat (note 20) immediately on release.
- Hold 10 to 2 s → Loop (note 21) at the 2 s mark, then lockout.
- Hold 8 to 2 s → Mute MIDI (note 32) at the 2 s mark, then lockout. Holding 6, 7, or 9 still does nothing.
- Tap 2, then tap 10 → Repeat only, pending Verse discarded.
- In setup: tap 4, tap 9, tap 2 → channel 2 pending. Tap 6+9 → channel 2 committed.
- In setup: tap nothing, tap 6+9 → exits cleanly, no flash write.
- In setup: hold 2+4 for 2 s → bond cleared, pairing window opens. Tap 7 → nothing selected. Host pairs → long flash on 2 and 4, back to the channel blink.
- In setup: press 2+4, release 4 at 1.5 s → nothing. Neither 2 nor 4 selects a channel.
- Outside setup: hold 2 and 4 together → Select (note 28) fires at 2 s on button 2 exactly as before, then lockout. No pairing.

### 8.5 Pairing transitions (Phase 10)

```
on PAIR_CHORD_HOLD:                           # input, only while setup is active
    if state != SETUP: return                 # unreachable by construction
    request link: BEGIN_PAIRING               # link clears bond, opens window, advertises

on TAP(B) while state == SETUP:
    if link.pairing_window_open: return       # select nothing; do not reset inactivity
    ...as §8.3

on TIMEOUT while state == SETUP:
    if link.pairing_window_open: return       # inactivity suspended
    ...as §8.3

on link window closed (bond ok, timeout):
    reset inactivity_deadline                 # fresh SETUP_TIMEOUT_MS

on exit_setup_mode():
    if link.pairing_window_open: request link: END_PAIRING
    ...as §7.3
```

The link layer, not the sequencer, owns the window deadline and the radio. The sequencer sees only `pairing_window_open` and the window-closed edge, by polling (the Q006/Q008 pattern), and issues requests through a drain the loop forwards (the Q019 pattern). `UiEvent` is unchanged.

## 9. MIDI output

- **Message:** Note On, velocity 1.
- **Channel:** runtime-configurable, 1–10, loaded from flash at boot, **default 1**. Held in a single variable owned by the MIDI layer; nothing else may cache it.
- **Note Off: not sent.** Playback treats MIDI cues as fire-and-forget triggers. A `SEND_NOTE_OFF` constant (default `false`) is retained so this can be flipped without restructuring, should another host be added later.
- **No output at all while in `SETUP` or `LOCKED`**, on any transport.
- **Transport** is chosen per message by §9.4. A cue is sent on exactly one transport, never both.
- **No running status optimisation.** Message volume is trivial; clarity beats efficiency.
- **Repeated identical commands** are sent every time, with no suppression.

### 9.1 USB device identity

All descriptor values are fixed here. Do not rely on SDK defaults for any of them.

| Descriptor | Value |
|---|---|
| `idVendor` | `0x2E8A` (Raspberry Pi) |
| `idProduct` | Allocated via a PR to `raspberrypi/usb-pid`. Until merged, use `0xFFFE` as a development placeholder — **must be replaced before the device leaves the bench.** |
| `bcdDevice` | `0x0100` (v1.00). Bump to `0x0200` at the v2 display iteration. |
| `iManufacturer` | `TrackStomp` |
| `iProduct` | `TrackStomp Navigator` |
| `iSerialNumber` | 16 uppercase hex characters from `pico_get_unique_board_id_string()` (`pico/unique_id.h`) |
| MIDI embedded IN jack string | `Navigator Cues` |

**Why not pid.codes.** VID `0x1209` requires every project using it to carry a recognised open source license. PRD §5.1 deliberately keeps this firmware closed, so that VID is not available to this project on its own terms.

**Product string length.** `TrackStomp Navigator` is 20 characters. Legacy Windows MIDI APIs truncate device names at 31, so this has headroom. Do not lengthen it past 31.

**Jack string matters more than it looks.** Many DAWs display the *port* name — taken from the MIDI jack string descriptor — rather than `iProduct`. Leaving the jack string unset produces a well-named device exposing a generically-named port. `Navigator Cues` is self-describing when shown alone, and degrades acceptably when a host concatenates device and port.

**Serial number stability.** Hosts key remembered MIDI port assignments on the VID/PID/serial tuple. An absent or regenerating serial makes MultiTracks Playback lose its input assignment between sessions. On RP2350 the unique ID is read from the **flash chip, not the MCU** — stable per board, but it changes if the flash is replaced, and Playback will then see a new device.

**macOS caches MIDI device names.** If any of these strings change after a first connection, clear the stale entry in Audio MIDI Setup or the old name persists.

### 9.2 Interface structure

One MIDI interface, one virtual cable, one IN endpoint. No CDC, no MSC, no vendor interface (§5.1). An OUT endpoint is not required in v1 but may be declared for future use.

No build paths, developer identity, or internal version strings appear in any descriptor beyond `bcdDevice`.

**Phase 10 changes nothing in §9.1 or §9.2.** The Pico 2 W enumerates identically to the Pico 2: same VID/PID, strings, serial derivation, and a single MIDI interface. `bcdDevice` stays `0x0100`; firmware versioning is the separate iSerial task (HANDOFF). The radio adds no USB interface.

### 9.3 BLE MIDI transport (Phase 10, `pico2_w` only)

#### 9.3.1 Service

Standard *MIDI over Bluetooth Low Energy* (BLE-MIDI 1.0):

| Item | UUID |
|---|---|
| BLE-MIDI service | `03B80E5A-EDE8-4B33-A751-6CE34EC4C700` |
| MIDI I/O characteristic | `7772E5DB-3868-4112-A1A9-F2669D106BF3` — read, write without response, notify; **encryption required** (§5.4) |

- A **read** returns an empty payload, as the specification requires.
- **Writes are accepted and discarded.** The pedal has no MIDI input (§3).
- Output is a **notification**, sent only when the host has subscribed and the link is encrypted with the bonded key. The transport reports this as *BLE ready* to §9.4.

#### 9.3.2 Packet format

One cue per notification — the same "clarity beats efficiency" rule as §9:

```
byte 0  header      1 0 t12 t11 t10 t9 t8 t7      (timestamp bits 12..7)
byte 1  timestamp   1 t6 t5 t4 t3 t2 t1 t0         (timestamp bits 6..0)
byte 2  status      0x90 | (channel - 1)            Note On
byte 3  note        0..32
byte 4  velocity    MIDI_VELOCITY (1)
```

The timestamp is the low 13 bits of milliseconds since boot at the moment the cue was resolved, so it wraps every 8.192 s; that is what the specification intends. No running status. If `SEND_NOTE_OFF` is ever enabled, Note Off is a separate notification with its own header and timestamp.

The encoder is a pure function and is host-tested (criterion 99).

#### 9.3.3 Advertising and connection

| State | Advertising |
|---|---|
| No bond, window closed | **None** — radio idle |
| Pairing window open | Connectable undirected, `BLE_ADV_INTERVAL_MS` |
| Bonded, not connected | Connectable undirected, `BLE_ADV_INTERVAL_MS`, so the host can reconnect |
| Connected | None (one connection at a time) |

- Advertising data: flags, 128-bit BLE-MIDI service UUID. Scan response: complete local name `TrackStomp Navigator` (it does not fit beside a 128-bit UUID in 31 bytes).
- **One connection at a time.**
- The pedal requests a connection interval of `BLE_CONN_INTERVAL_MIN_UNITS`–`BLE_CONN_INTERVAL_MAX_UNITS` (11.25–15 ms) once connected; the host decides. Apple hosts commonly grant 11.25–15 ms to MIDI peripherals.
- A connection that is not encrypted with the bonded key within `BLE_UNBONDED_DROP_MS` (5000) is disconnected by the pedal.

#### 9.3.4 Security Manager policy

- IO capability **NoInputNoOutput**; LE Secure Connections; bonding.
- **Inside the pairing window:** a Just Works request is confirmed.
- **Outside it:** every pairing request is declined (`sm_bonding_decline` or equivalent), including re-pairing from a host whose bond was cleared.
- Bond storage: BTstack's TLV store over the flash bank of §10.1, `NVM_NUM_DEVICE_DB_ENTRIES` = `BLE_MAX_BONDS` (1).

#### 9.3.5 Latency

BLE delivers a notification at the next connection event, so latency from cue resolution to the host is bounded by the connection interval rather than by the firmware. Requirements:

- **Firmware:** a resolved cue is handed to BTstack (`att_server_request_can_send_now` / notify) in the same main-loop iteration it is resolved. Nothing batches, delays, or waits for a previous notification beyond BTstack's own flow control.
- **End to end, measured on the bench (criterion 108):** median ≤ 20 ms and worst case ≤ 40 ms over 100 cues from button release to Playback receiving the note, with the pedal in its sealed enclosure on the floor.
- USB is untouched: criterion 11's 10 ms still applies whenever USB is the active transport.

### 9.4 Transport arbitration (Phase 10)

**USB wins.** For each cue:

| USB host mounted (`tud_mounted()`) | BLE ready (§9.3.1) | Sent on |
|---|---|---|
| yes | either | **USB only** |
| no | yes | **BLE only** |
| no | no | nothing — dropped silently |

- Arbitration is evaluated **per cue**, not latched. Pulling the USB cable mid-service fails over to BLE on the next cue if BLE is ready; plugging it in moves the next cue to USB.
- The BLE link is **kept up** while USB is mounted — it is just not used — so failover needs no reconnect.
- A USB cable to a charger or a powered hub with no host does not mount, so BLE stays active. That is intended.
- The policy is a pure function and is host-tested (criterion 97). A cue is **never** sent on both: the host would see two ports and fire every cue twice.
- On the plain Pico 2 build, BLE is never ready, so behaviour is exactly v1.

---

## 10. Persistent configuration

The RP2350 has no EEPROM. Configuration is stored in a reserved flash sector using `hardware_flash`.

### 10.1 Layout

- Reserve the **last 4 KB sector** of on-board flash for the config record. Exclude it from the linker's image region so a firmware update does not clobber it. **Its physical location never moves** — units in the field keep their channel across the Phase 10 upgrade (criterion 114).

**Flash map (Phase 10).** Both boards carry 4 MB.

| Region | Offset | Size | Owner |
|---|---|---|---|
| Firmware image (linker `FLASH` region) | `0x000000` | 4 MB − 12 KB | linker |
| Bluetooth bond store (BTstack flash bank) | `0x3FD000` | 8 KB (2 sectors) | BTstack, via `pico_btstack_flash_bank` |
| Config record | `0x3FF000` | 4 KB (1 sector) | `config_store` |

- `CONFIG_FLASH_OFFSET`, `BOND_FLASH_OFFSET`, `BOND_FLASH_SIZE`, and `IMAGE_FLASH_LIMIT` are constants in `config.h` (§13). `PICO_FLASH_BANK_STORAGE_OFFSET` and `PICO_FLASH_BANK_TOTAL_SIZE` are passed to the SDK as compile definitions **from the same values**, and a static assertion ties them together.
- **The SDK's default bond-store offset must not be relied on.** On RP2350 boards with A2 support (the `pico2_w` board header enables it) BTstack defaults to `PICO_FLASH_SIZE_BYTES − 4 KB − 8 KB`, deliberately keeping the last sector free for the RP2350-E10 workaround. v1 shrinks `PICO_FLASH_SIZE_BYTES` by one sector (QUESTIONS Q016), so the default would silently land somewhere else. Set it explicitly.
- **`config_store` must address its sector by `CONFIG_FLASH_OFFSET`, not by `PICO_FLASH_SIZE_BYTES`.** v1 derives the address from the overridden flash size (Q016). Shrinking the image region to make room for the bond store would otherwise move the config sector, and every unit would lose its channel on upgrade.
- **RP2350-E10.** The SDK keeps the last sector free on A2 silicon for an erratum workaround; v1 already stores config there. T31 must read the E10 entry in the RP2350 datasheet and confirm that a single-family UF2 drag-and-drop cannot touch the last sector. If it can, that is a BLOCKER — the config sector would have to move, which is a migration, not a task detail.
- The plain `pico2` build uses the same map, with the bond region simply unused.
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

Config writes occur only on exit from Setup Mode, and only when the channel actually changed.

**Phase 10 adds a second flash writer: BTstack's bond store.** It is permitted to write in exactly two circumstances, both inside Setup Mode:

- **Bond creation**, when pairing succeeds inside the pairing window.
- **Bond clearing**, when the 2+4 pair chord fires.

**No bond-store write may occur during performance**, including when the bonded host reconnects. BTstack's LE device database keeps a sequence number for eviction and may store on other events; T38 must verify with an instrumented debug build (a flash-bank write counter) that a reconnect and a full cue session cause zero erases or programs. If BTstack writes on reconnect, that is a BLOCKER — §10.3's "never during performance" rule outranks convenience.

BTstack's flash bank uses `flash_safe_execute`. `config_store` keeps its existing `save_and_disable_interrupts` path, which remains correct while core 1 is unused.

Implementation constraints that will bite if ignored:

- Flash erase and program require **XIP to be disabled**, so the code performing them must run from RAM. Mark those functions `__not_in_flash_func`.
- **Disable interrupts** for the duration (`save_and_disable_interrupts()` / `restore_interrupts()`). Any ISR that executes from flash during the operation will hard fault.
- Core 1 must be idle or parked. It is unused in v1, but this becomes a real constraint in v2 when the display may run there — use `flash_safe_execute` rather than raw calls once that happens.
- USB will stall for the few milliseconds the operation takes. Acceptable because it happens only on setup exit, never during performance. Do not attempt a flash write from any performance path.
- The radio stalls too. With `pico_cyw43_arch_poll` nothing services the CYW43 while interrupts are off, and a BLE connection survives a few milliseconds' gap comfortably (the supervision timeout is seconds). Same justification: setup only.

### 10.4 Wear

One erase cycle per channel change. Flash endurance is on the order of 100,000 cycles, so this is a non-issue at human interaction rates — provided the no-op skip in §7.3 is implemented and no code path writes on every boot.

---

## 11. Hardware interface

### 11.1 Pin assignment

| Signal | GPIO |
|---|---|
| Buttons 1–10 | GP6 – GP15 (contiguous, in order) |
| Per-switch status LEDs 1–10 | GP5, GP4, GP3, GP2, GP22, GP21, GP20, GP19, GP18, GP17 (§11.5.1) |
| Reserved: ADC (future expression) | GP26 / GP27 / GP28 |
| Panel status LED | GP16 *(not populated on cape v1 — §11.4)* |
| Onboard LED (bench debug mirror) | **`pico2_w`: CYW43 `WL_GPIO0`** (`CYW43_WL_GPIO_LED_PIN`). `pico2`: GP25 |
| Debug UART (debug builds only) | GP0 / GP1 |
| **Radio, `pico2_w` only — internal, never touched by application code** | GP23 `WL_ON`, GP24 `WL_D` (data/IRQ), GP25 `WL_CS`, GP29 `WL_CLK` (shared with the VSYS/3 ADC sense) |

**Pico 2 W pin changes (Phase 10).** The Pico 2 W has the same footprint and 40-pin header as the Pico 2, and **every header pin this design uses is unchanged**. The differences are all off-header:

- **GP25 becomes the radio's SPI chip-select.** v1 drives GP25 as the onboard-LED mirror (T17). On a Pico 2 W that write would fight the radio. The `pico2_w` build must never configure, read, or write GP23, GP24, GP25, or GP29 — only the CYW43 driver does.
- **The onboard LED moves to the radio chip.** It is `WL_GPIO0` on the CYW43439, driven with `cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on)` (or the SDK's `pico_status_led`). Each call is an SPI transaction, so the shim writes it **only when the lamp value changes**, never every loop. It is dark until `cyw43_arch_init()` returns, a few hundred ms after power-up; that is acceptable for a bench-only mirror. It remains a mirror of the §11.4 panel engine output, unchanged in meaning.
- **VBUS sense** moves to CYW43 `WL_GPIO2`. Nothing in this design reads it — §9.4 uses `tud_mounted()`, which means "a USB host has configured us", not "5 V is present".

Contiguous button pins allow a single masked `gpio_get_all()` read per scan. The LED pins are **not** contiguous and must be driven through a bit-position table — see §11.5.1.

**GP4 and GP5 are no longer reserved for I2C0.** They carry LEDs 2 and 1. The v2 display of §16 is superseded by §11.5.

**GP25 is not exposed on the Pico 2 header.** The 40-pin header brings out GP0–GP22 and GP26–GP28 only; GP25 is wired solely to the onboard LED. The panel LED therefore uses GP16. Both are driven from the same pattern-engine output, so the onboard LED remains usable as a bench-debug mirror while the enclosure is open.

### 11.2 Electrical

- Each switch wires common → GND, one throw → its GPIO.
- **Internal pull-ups only.** The RP2350 has a known erratum affecting pull-downs: a GPIO input pad can leak enough current that an internal pull-up cannot hold it high, and external pull-downs would need to be 8.2 kΩ or lower. Pull-up plus switch-to-ground sidesteps this entirely. Do not design any input around a pull-down.
- Active-low logic: pressed reads 0.

**Panel LED (GP16) — blue, switched by an NPN from VBUS.**

A blue LED has a forward voltage of 3.0–3.4 V, which leaves essentially no headroom on the 3.3 V rail. It is therefore driven from **VBUS (pin 40, raw USB 5 V)** through a low-side NPN switch rather than directly from the GPIO.

```
VBUS (5V) ──▶|── LED (blue) ──[ 470Ω ]── C
                                          │
                                     NPN (2N3904)
                                          │
GP16 ──[ 2.2kΩ ]── B                      E ── GND
```

| Part | Value |
|---|---|
| LED | Blue 5 mm panel-mount, Vf ≈ 3.2 V |
| LED series resistor | 470 Ω (collector leg) |
| Transistor | 2N3904 (TO-92) or MMBT3904 (SOT-23) |
| Base resistor | 2.2 kΩ |
| GP16 pad drive strength | 4 mA |

**Current budget.** 5 V − 3.2 V (LED) − 0.2 V (V_CE saturated) leaves 1.6 V across the series resistor, so 470 Ω yields ≈ 3.4 mA. Drop to 330 Ω (≈ 4.8 mA) or 180 Ω (≈ 8.9 mA) only if the finished panel reads dim. Blue is perceptually brighter than its drive current suggests, and a dark stage is precisely where an over-driven indicator becomes a distraction.

**Base drive.** (3.3 V − 0.8 V V_BE) / 2.2 kΩ ≈ 1.1 mA of base current, giving hard saturation with wide margin at these collector currents, while drawing only ~1 mA from the pin — well inside the 4 mA pad setting.

**Logic is non-inverting.** GP16 high turns the LED on. The pattern engine (§11.4) requires no change and no inversion anywhere in software.

**Do not omit the transistor by sinking into GP16 from a 5 V anode.** During reset and early boot, GPIOs are inputs and the pin floats up toward 5 V through the LED. RP2350 pins are not 5 V tolerant, so this forward-biases the pad's ESD diode into the 3.3 V rail on every power-up. It will appear to work for months.

If a MOSFET is preferred, a BSS138 with a 10 kΩ gate-to-source pull-down (so it stays off while GP16 is an input during boot) is equivalent. The NPN is cheaper and simpler at this current, and is what v1 specifies.

**Onboard LED (GP25 on `pico2`; CYW43 `WL_GPIO0` on `pico2_w`, §11.1).** Driven from the same pattern-engine output as GP16, no external parts — it already has its own series resistor on the board. Bench debugging only; invisible once the enclosure is closed.

### 11.3 Debounce

- Poll every `SCAN_INTERVAL_MS` (1 ms).
- A state change is accepted only after the new level is stable for `DEBOUNCE_MS` (default **20**). Footswitches bounce far worse than fingers; do not reduce below 15 without measuring.
- Debounce is per-button and independent. Chord detection (§6.5) operates on debounced state, never raw. In particular, `chord_armed` must not be cleared by a contact bounce on 6 or 9 — this is precisely the failure that would make a 5-second chord unreachable in practice.

### 11.4 LED indication patterns

The single panel LED on GP16 (mirrored to the onboard LED, §11.1) was the only feedback available before the per-switch LEDs existed. Setup Mode was unusable without it, so these patterns were a requirement, not a nicety.

**Board-revision note — cape v1.** GP16 is **not populated** on cape v1: the firmware drives the pin and nothing is connected. Criteria 41–56 and 81 are therefore `BLOCKED` on this revision (observable only on the onboard mirror with the enclosure open, or on pre-cape wiring). The engine stays specified and tested on the host. Phase 10 adds no pattern to this engine; Bluetooth indications are on the per-switch LEDs only (§11.5.8).

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

### 11.5 Per-switch status LEDs

Ten LEDs, one directly above each footswitch, are the primary feedback surface. **The panel LED of §11.4 is unchanged.** The two surfaces run independent engines, never share state, and are never required to agree — §11.4 remains the fallback indication and its acceptance criteria (41–56) continue to apply exactly as written.

Throughout this section, *LED N* means the LED above button N. The correspondence is fixed and is never remapped at runtime.

#### 11.5.1 Pin assignment

| Button | Button GPIO | LED GPIO |
|---|---|---|
| 1 | GP6 | GP5 |
| 2 | GP7 | GP4 |
| 3 | GP8 | GP3 |
| 4 | GP9 | GP2 |
| 5 | GP10 | GP22 |
| 6 | GP11 | GP21 |
| 7 | GP12 | GP20 |
| 8 | GP13 | GP19 |
| 9 | GP14 | GP18 |
| 10 | GP15 | GP17 |

The LED pins are deliberately **not** contiguous — they run GP5 down to GP2, then GP22 down to GP17 — because that ordering minimises trace crossings between the Pico header and a switch row laid out left to right. A single `gpio_put_masked()` still drives all ten in one write; the engine holds a bit-position table rather than assuming an ordered range. **Do not assume `LED_GPIO[n] == LED_GPIO[0] - n`.**

**GP4 and GP5 are no longer reserved for I2C0.** The v2 display described in §16 is superseded by this section. If a display is ever revived, I2C0 moves to GP0/GP1 and the debug UART moves or is dropped; that is a v3 decision, not a reservation carried here.

#### 11.5.2 Electrical

Topology is identical to §11.2 — a low-side NPN per LED, non-inverting, GPIO high means lit. The LED is not driven from the 3.3 V rail, and the transistor is not optional: the ESD-diode argument in §11.2 applies unchanged to all ten pins.

**The anodes are fed from VSYS (pin 39), not VBUS.** VBUS is dead whenever the unit runs from the 9 V input alone, so LEDs on VBUS would simply not light. VSYS is fed by whichever source is higher and therefore works on either, and the two paths land within a few tens of millivolts of each other (see the budget below), so brightness does not change with the power source.

```
VSYS (pin 39) ──▶|── LED (blue) ──[ 220Ω ]── C
                                              │
                                         NPN (2N3904)
                                              │
LED_GPIO ──[ 2.2kΩ ]── B                      E ── GND
```

| Part | Value |
|---|---|
| LED | Blue 5 mm panel-mount, 14" pigtails (Dialight) |
| LED series resistor | 220 Ω (collector leg) |
| Transistor | 2N3904 (TO-92) ×10 |
| Base resistor | 2.2 kΩ ×10 |
| Pad drive strength | 4 mA on all ten pins |

**Current budget — measured on cape v1, not calculated.**

| | 100 Ω (first attempt) | 220 Ω (specified) |
|---|---|---|
| VSYS | 4.63 V | 4.63 V |
| Drop across the resistor | 1.55 V | 1.77 V |
| LED current | 15.5 mA | 8.0 mA |
| Implied LED Vf | ≈ 2.95 V | ≈ 2.78 V |

§11.5.3 guarantees **at most two LEDs are ever lit simultaneously**, so the whole array draws under 16 mA. This is why no external supply, no output-enable line, and no enumeration-time blanking are required.

100 Ω was built first and read as uncomfortably bright through the panel. 220 Ω was chosen by looking at the finished panel, not by arithmetic, and that is the correct method for an indicator.

**Do not size this resistor from the datasheet forward voltage.** The part is quoted at 3.6 V, which is a figure at its rated current; at 8–15 mA it sits 0.6–0.8 V below that, and the two measurements above trace a consistent diode curve. Using the datasheet value underestimates available headroom by roughly 50% and produces a resistor that runs the LED far harder than intended.

**Headroom and trim.** 1.77 V across the resistor makes current tolerant of forward-voltage spread between parts: a ±0.2 V spread moves current about 11%, which is not perceptible. Buying all ten from one reel remains sensible, but the per-LED PWM duty constants (§13) are expected to stay uniform. If brightness does read uneven through the finished panel, correct it with those constants rather than by changing resistors — the series resistor sets the brightness of the whole array, duty corrects differences between individual LEDs. Those constants are a **brightness-matching mechanism only**: duty is never used to signal anything, and every pattern in this section is plain on or off.

Raise the series resistor further if the panel still reads bright in the room it will be used in; §11.2's reasoning about over-driven indicators applies with ten times the force. 330 Ω would give roughly 5 mA.

#### 11.5.3 Indication model

**Exactly one indication is visible at any instant.** It owns one or two LEDs; every other switch LED is off. There is no compositing, no per-LED independence, and no state in which three or more LEDs are lit. This is the same discipline as §11.4.1 — the surface shows one thing at a time — and it is what makes the whole engine testable as a single waveform.

| Priority | Source | LEDs | Pattern |
|---|---|---|---|
| 0 | **Pairing window (§11.5.8)** — Phase 10 | 2 and 4 | Advertising 500/500 · bonding 100/100 · bond OK solid 2000 |
| 1 | Setup channel (§11.5.5) | N | 1000 ms on / 1000 ms off |
| 2 | Chord progress (§11.5.6) | 6 and 9 | Dark 500 ms, then 125 / 125 |
| 3 | Hold confirmation (§11.5.4) | Held button | Solid |
| 4 | Cue confirmation (§11.5.4), **and the boot link confirmation (§11.5.8)** | 1 or 2 | Per waveform |
| 5 | Pending sequence | Prefix | 125 ms on / 125 ms off |
| 6 | Idle | — | All off |

Priority 0 is numbered so that 1–6 keep their Phase 9 numbers; every existing test and criterion still refers to the same rows.

Two different mechanisms end an indication, and they are not interchangeable:

- **Override.** Chord progress (priority 2) *suspends* what sits beneath it. A pending flash underneath survives and resumes on the same tick the chord aborts. This mirrors §11.4.3 exactly.
- **Cancel.** Any accepted `ButtonEvent` destroys a running confirmation (priorities 3 and 4) immediately, on the tick it arrives. A cancelled confirmation never resumes. The rationale is §11.4.4's: musical feedback outranks confirmation, and with confirmations running a full five seconds, a cue arriving mid-confirmation is the common case rather than the exception.

A press discarded by the §6.4 lockout is not an accepted event and cancels nothing.

**The pending flash and the chord flash share a 125 ms rate.** This is deliberate and accepted: the two never mean the same thing because position disambiguates them, and the case where both would be live at once (pending on a prefix, then 6+9 pressed) is precisely the case where chord progress overrides the pending flash and only 6 and 9 are lit. The panel LED's simultaneous 1000 ms pending flash remains the rate-distinct signal §11.4.1 requires.

#### 11.5.4 Confirmation waveforms

Every confirmation runs a **5000 ms envelope** (`LED_CONFIRM_MS`). The three cue classes differ only in what happens inside it.

| Cue class | Example | LEDs | Waveform |
|---|---|---|---|
| Pair | Tap 1, tap 6 → note 0 | Prefix and suffix | Solid for 5000 ms |
| Self-pair | Tap 3, tap 3 → note 24 | Prefix only | 2000 on, 1000 off, 2000 on |
| Standalone | Tap 10 → Repeat | LED 10 | 1000 on, 1000 off, 1000 on, 1000 off, 1000 on |
| Hold | Hold 1 to 2 s → note 27 | Held button | Solid — see below |
| Boot channel | Power-on | Stored channel | Solid for 2000 ms |

- A **pair** lights both buttons that produced the cue, so the operator sees the sequence they actually played rather than a generic acknowledgement.
- A **self-pair** lights one LED and must therefore be distinguishable from a pair by waveform alone, which is what the 1000 ms notch in the middle provides.
- A **standalone** tap of 10 uses the same waveform whether it fires from IDLE or from PENDING. When it fires from PENDING, the abandoned prefix's LED extinguishes on the same tick (§8.3 discards the pending sequence).
- A **hold** is solid from the instant the hold command fires. The envelope is a **minimum, not a maximum**: the LED stays lit for at least `LED_CONFIRM_MS`, and never goes dark while the button remains physically pressed. §6.4 discards all input during lockout, so an LED that went dark at 5 s while the operator was still standing on the switch would give no indication that everything they do is being swallowed. The LED therefore means *this button is still doing something*.

Holds of buttons 6, 7, and 9 fire no command (§6.3) and light no LED. An orphan suffix tap from IDLE lights no LED.

#### 11.5.5 Setup Mode

On entry at the 5000 ms chord threshold, chord progress ends and the channel indication begins immediately — including while `LOCKED(CHORD)` waits for both 6 and 9 to be released. The flash-to-blink transition is the entry confirmation on this surface, exactly as flash-to-solid is on the panel LED.

- The LED for the **currently selected channel** blinks at 1000 ms on / 1000 ms off for as long as Setup Mode is active.
- Tapping button N selects channel N (§7.2). The previous channel's LED extinguishes and LED N begins blinking **from its on phase** on the same tick, so a selection is always acknowledged by light rather than by darkness.
- Holds do nothing in Setup Mode, and no MIDI is sent, so no confirmation waveform can occur here.
- **On exit — by 6+9 tap or by 30 s inactivity — all ten switch LEDs go off.** There is no exit confirmation on this surface. The panel LED's channel blink (§11.4.4) is unchanged and remains the exit confirmation, which is why duplicating it here would add nothing.

Because the channel and the LED share a number, Setup Mode needs no blink-counting on this surface at all. Channel 7 is simply the lit LED above button 7.

#### 11.5.6 Chord progress

While the 6+9 chord timer is armed, this indication owns the whole surface and mirrors §11.4.3's timing:

- **0 – 500 ms (`CHORD_LED_DELAY_MS`):** every switch LED is forced off, including a live pending flash. An accidental graze of both buttons must not twitch anything.
- **500 – 5000 ms:** LEDs 6 and 9 flash together, in phase, at `CHORD_LED_FLASH_MS` (125 on / 125 off).
- **At 5000 ms:** straight to the Setup Mode channel blink (§11.5.5).
- **On abort:** the override releases on the same tick and any indication beneath it resumes where it would have been.
- **On re-form (§6.5):** the 500 ms blackout restarts along with the timer, so a restarted chord is visible rather than silent.

#### 11.5.7 Boot

At boot, after `config_store` reports the stored channel (§10), LED N is lit solid for `LED_BOOT_CHANNEL_MS` (2000 ms), then extinguished. A factory-fresh unit lights LED 1. Any button press cancels it, per the §11.5.3 cancel rule.

This runs alongside the §11.4.4 boot channel blink on the panel LED. The two are independent and neither gates the other.

#### 11.5.8 Bluetooth indications (Phase 10)

All Bluetooth indications use **LEDs 2 and 4 together, in phase** — the pair chord's own buttons, so the indication sits where the operator's foot just was. They obey §11.5.3: one indication at a time, at most two LEDs.

**Pairing window (priority 0, Setup Mode only).** Replaces the Setup channel blink for as long as the window is open.

| Window sub-state | Pattern on LEDs 2 and 4 |
|---|---|
| Advertising, waiting for a host | Slow flash, `LED_ADVERTISING_MS` on / off (500 / 500) |
| Bonding — pairing in progress | Fast flash, `LED_BONDING_MS` on / off (100 / 100) |
| Bond succeeded | **One long flash:** solid `LED_BOND_OK_MS` (2000), then off |

- The window starts in the *advertising* on phase on the tick the pair chord fires.
- Pairing failure returns to *advertising*, starting from its on phase.
- After the bond-OK flash, or when the window times out, the **Setup channel blink resumes from its on phase** (as §11.5.5 does on a selection).
- 6+9 exit during any sub-state, including mid bond-OK flash: all ten off, per §11.5.5.
- Taps ignored by §7.5 do not cancel or restart anything; there is no "accepted event" for them to cancel with.
- Chord progress for 6+9 (priority 2) cannot occur inside Setup Mode — the exit gesture has no hold — so there is no override interaction.
- 500/500 is deliberately distinct from Setup's 1000/1000 and from pending's 125/125; position (two LEDs, 2 and 4) also distinguishes it.

**Boot link confirmation (priority 4, the cue-confirmation row).** If the bonded host reaches *BLE ready* (§9.3.1) within `BOOT_CONNECT_WINDOW_MS` (30000) of power-up, LEDs 2 and 4 give **one flash of `LED_LINK_CONNECTED_MS` (1000)**, then off.

- It **waits for the boot channel indication (§11.5.7) to finish** rather than interrupting it; if the link becomes ready during those 2000 ms, the flash plays when they end.
- Like any confirmation, an **accepted `ButtonEvent` cancels it** — including one that arrives before it has started, which drops it. Playing outranks a status flash.
- It happens **at most once per power cycle**. Later reconnects, and connections after the 30 s window, show nothing.
- No indication is given for a disconnect, and none for "no host found". A pedal that boots without the flash is not connected; that is the signal.

**Plain Pico 2 build:** none of these indications can occur (BLE is never ready, the window closes immediately).

---

### 11.6 Radio and antenna (Phase 10)

- The Pico 2 W antenna is the PCB trace at the **end of the module opposite the USB connector**. That end must sit over the copper keep-out already cut into the cape's ground pour, opening toward the board edge. No copper on either layer, no harness wires, standoffs, or mounting hardware within the keep-out or routed across the antenna end.
- **Enclosure.** The Hammond 1550WJBK is die-cast aluminium and will attenuate 2.4 GHz heavily. **Decision (2026-10-31): measure range in the sealed enclosure first, then decide on shielding changes** (a non-metallic window over the antenna end, a different enclosure, or a module with an external antenna). Criterion 110 is that measurement. Nothing in firmware depends on the outcome.

### 11.7 Power (Phase 10)

- Cable-free BLE operation requires the **9 V input** (HANDOFF: polyfuse → 1N5819 → P6KE18A → 220 µF → OKI-78SR-5/1.5 → P6KE6.8A → 1N5819 → VSYS). The 9 V input is no longer optional on a unit expected to run on BLE; it is also what lets older iPads that cannot power a USB device use the pedal.
- The CYW43439's BLE current is small against the 1.5 A buck; the §11.5.2 LED budget (under 16 mA) is unchanged.
- USB and 9 V continue to coexist through the Pico's VBUS/VSYS Schottky. Nothing connects to VBUS.

## 12. Architecture

```
pico_sdk_import.cmake   SDK bootstrap shim, copied from $PICO_SDK_PATH/external/
CMakeLists.txt          Target build plus the BUILD_HOST_TESTS host path
CMakePresets.json       Generators and cache vars for the pico2, pico2_w (Phase 10), and host presets
tests/                  Host test sources. Added in T02, restructured by T03.
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
    midi_out.h/.cpp   MIDI send; owns the active channel; routes each cue per §9.4
    transport.h/.cpp  Phase 10. Pure §9.4 policy: (usb_mounted, ble_ready) → USB | BLE | none
    ble_midi_packet.h/.cpp  Phase 10. Pure §9.3.2 encoder. Host-tested.
    ble_midi.h/.cpp   Phase 10, pico2_w only. BTstack GATT server, advertising,
                      Security Manager policy, bond store. Feeds link events.
    trackstomp.gatt   Phase 10. GATT database: GAP, GATT, BLE-MIDI. Nothing else.
    btstack_config.h  Phase 10. BTstack compile-time config (1 bond, BLE only).
    usb_descriptors.c Explicit descriptors, MIDI-only
    tusb_config.h     TinyUSB compile-time config. Stubbed disabled in T01 so
                      the skeleton links; owned and enabled by T15.
  link/
    link_state.h/.cpp Phase 10. Pure BLE session policy, host-buildable: pairing
                      window and its deadline, sub-state (advertising / bonding /
                      bond OK), BLE ready, boot-connect window. Consumes LinkEvent
                      from the transport, emits LinkRequest to it.
  board/
    board_led.h/.cpp  Phase 10. Onboard LED shim: GP25 on pico2, CYW43 WL_GPIO0 on
                      pico2_w, written only on change.
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
- The per-switch LED engine (§11.5) is a **second, independent engine inside `ui`**. It shares no state with the §11.4 panel engine and must not alter it. `ui_lamp()` keeps its exact current meaning and behaviour.
- The per-switch engine recovers the button(s) behind a cue by reverse lookup in `CUE_TABLE`, never by inspecting `ButtonEvent`. This keeps §6.2's single source of truth intact and leaves the `UiEvent` contract unchanged.
- Neither LED engine may call `sleep_ms`. Both are advanced by their tick function only.

**Phase 10 layer rules**

- **`input`, `sequencer`, and `ui` stay board-independent and host-buildable.** No `cyw43_*`, `btstack`, or `tud_*` call in them.
- `input` detects the 2+4 pair chord only while setup is active (it already knows this through `buttons_set_setup_active`, Q006) and emits it as a new `ButtonEventKind::PairChordHold`. This is an **additive** enum value: it never occurs outside Setup Mode, so no existing ordered `ButtonEvent` assertion can see it.
- `link_state` is pure and owns the pairing window. It knows nothing of BTstack. `ble_midi` translates between BTstack events and `LinkEvent` / `LinkRequest`, and is the only file that includes BTstack headers.
- The sequencer reads `link_pairing_window_open()` and the window-closed edge **by polling**, and issues `BEGIN_PAIRING` / `END_PAIRING` through a drain that `loop_dispatch` forwards — the Q006 / Q019 patterns. It never calls `link_*` mutators or `ble_*`.
- **`UiEvent` does not change.** `ui` reads Bluetooth state by polling `link_state` accessors, exactly as it polls `buttons_chord_start()` (Q009). No new `UiEventKind`, no new field.
- `midi_out` is the only caller of both transports. It asks `transport` which one to use, per cue.
- **Only `board_led` touches the onboard LED.** Nothing on `pico2_w` may name GP23, GP24, GP25, or GP29.
- `config_store` is unchanged in role. BTstack's flash bank is the only other flash writer, within §10.3's limits.

### 12.2 Main loop

Single-threaded cooperative loop, no interrupts required at this event rate:

```
while (true) {
    tud_task();                    // TinyUSB housekeeping — must run every iteration
    cyw43_arch_poll();             // pico2_w only: CYW43 driver + BTstack run loop
    uint32_t now = time_ms();
    buttons_scan(now);             // → ButtonEvents
    sequencer_tick(now);           // handles events + all deadline expiry
    link_tick(now);                // pairing window deadline, boot-connect window
    loop_dispatch();               // Commands → midi_out (§9.4); sequencer link requests → link;
                                   // link requests → ble_midi; ble_midi events → link
    ui_tick(now);                  // LED pattern engines, non-blocking
    sleep_us(500);
}
```

`cyw43_arch_poll()` is compiled only for `pico2_w`. The host harness pipeline gains `link_tick` and the link half of `loop_dispatch` with a fake transport (TEST-HARNESS.md).

The LED pattern engine must be a non-blocking state machine. Do not implement blink patterns with `sleep_ms` — that would stall `tud_task()` and drop USB.

Core 1 is unused. It is not reserved for anything; a display is not planned.

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

// Per-switch LED array (§11.5). Index 0 is button 1.
constexpr uint8_t  SWITCH_LED_COUNT     = 10;
constexpr uint8_t  SWITCH_LED_GPIO[SWITCH_LED_COUNT] =
    { 5, 4, 3, 2, 22, 21, 20, 19, 18, 17 };

// Per-switch LED timing (§11.5.3–§11.5.7)
constexpr uint32_t LED_PENDING_FLASH_MS = 125;   // on and off; 250 ms period
constexpr uint32_t LED_CONFIRM_MS       = 5000;  // confirmation envelope
constexpr uint32_t LED_SELF_PAIR_ON_MS  = 2000;
constexpr uint32_t LED_SELF_PAIR_OFF_MS = 1000;
constexpr uint32_t LED_STANDALONE_MS    = 1000;  // on and off; five phases
constexpr uint32_t LED_SETUP_BLINK_MS   = 1000;  // on and off; 2000 ms period
constexpr uint32_t LED_BOOT_CHANNEL_MS  = 2000;

// Brightness matching only (§11.5.2). Never used to signal state.
constexpr uint8_t  SWITCH_LED_DUTY[SWITCH_LED_COUNT] =
    { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };

// Flash map (§10.1). Both boards carry 4 MB.
constexpr uint32_t FLASH_TOTAL_BYTES    = 4u * 1024u * 1024u;
constexpr uint32_t FLASH_SECTOR_BYTES   = 4096u;
constexpr uint32_t CONFIG_FLASH_OFFSET  = FLASH_TOTAL_BYTES - FLASH_SECTOR_BYTES;      // 0x3FF000, never moves
constexpr uint32_t BOND_FLASH_SIZE      = 2u * FLASH_SECTOR_BYTES;                     // 8 KB
constexpr uint32_t BOND_FLASH_OFFSET    = CONFIG_FLASH_OFFSET - BOND_FLASH_SIZE;       // 0x3FD000
constexpr uint32_t IMAGE_FLASH_LIMIT    = BOND_FLASH_OFFSET;                           // linker FLASH length

// Bluetooth pairing (§7.5) and indications (§11.5.8)
constexpr uint8_t  PAIR_CHORD_BUTTON_A  = 2;
constexpr uint8_t  PAIR_CHORD_BUTTON_B  = 4;
constexpr uint32_t PAIR_CHORD_HOLD_MS   = 2000;  // continuous overlap, setup only
constexpr uint32_t PAIRING_WINDOW_MS    = 60000;
constexpr uint32_t LED_ADVERTISING_MS   = 500;   // on and off; 1000 ms period
constexpr uint32_t LED_BONDING_MS       = 100;   // on and off; 200 ms period
constexpr uint32_t LED_BOND_OK_MS       = 2000;  // single long flash
constexpr uint32_t LED_LINK_CONNECTED_MS = 1000; // boot link confirmation
constexpr uint32_t BOOT_CONNECT_WINDOW_MS = 30000;

// BLE transport (§9.3)
constexpr uint8_t  BLE_MAX_BONDS        = 1;
constexpr uint32_t BLE_ADV_INTERVAL_MS  = 100;
constexpr uint16_t BLE_CONN_INTERVAL_MIN_UNITS = 9;   // ×1.25 ms = 11.25 ms
constexpr uint16_t BLE_CONN_INTERVAL_MAX_UNITS = 12;  // ×1.25 ms = 15 ms
constexpr uint32_t BLE_UNBONDED_DROP_MS = 5000;
constexpr uint32_t BLE_MIDI_TIMESTAMP_MASK = 0x1FFF;  // 13-bit ms timestamp
```

---

## 14. Acceptance criteria

**Enumeration**
1. Device appears as a class-compliant MIDI input on macOS, Windows, and iOS with no drivers.
2. No mass storage volume mounts. No serial port appears. Release build only.
3. MultiTracks Playback receives and triggers on all 33 cues.

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
15. Hold 10 for 2 s → note 21 at the 2 s mark, then lockout. No note 20.
15b. Hold 8 for 2 s → note 32 at the 2 s mark, then lockout. Nothing else.
15c. Tap 2, then hold 8 for 2 s → note 32 only. No Verse 3 note, pending discarded.
15d. Hold 6 for 3 s, hold 7 for 3 s, hold 9 for 3 s (each alone) → nothing sent in any case, no lockout.
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
29b. Press 6+9 and hold; at +1 s also press and hold 8 past its 2 s threshold → note 32 fires and lockout engages; the chord is cancelled and setup is NOT entered even if 6 and 9 stay down past 5 s.

**Setup mode**
30. On entry, release both 6 and 9 → slow blink begins. No MIDI sent throughout.
31. While in setup, tap 2 then 7 → **no MIDI sent**. LED blinks 2, then 7.
32. While in setup, hold 3 for 3 s → nothing happens. No note 29. Same for a 3 s hold of 8 — no note 32.
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

**Per-switch LEDs (§11.5)**
61. Tap 1 → LED 1 flashes at a 250 ms period; no other switch LED is lit at any point.
62. Pending window expires with no second input → LED 1 goes off at exactly `SEQUENCE_TIMEOUT_MS` after the tap.
63. Tap 2, then tap 4 → LED 2 extinguishes and LED 4 begins flashing on the same tick; LED 4 runs a fresh `SEQUENCE_TIMEOUT_MS`.
64. Tap 1 then 6 → LEDs 1 and 6 both solid for exactly 5000 ms, then both off.
65. Tap 3 then 3 → LED 3 on 2000, off 1000, on 2000, then off. Distinguishable from criterion 64 by waveform alone.
66. Tap 10 from IDLE → LED 10 alternates at 1000 ms for five phases, ending lit, then off.
67. Tap 2 then tap 10 → LED 2 extinguishes on the same tick LED 10 begins the standalone waveform.
68. Hold 1 to `HOLD_MS`, release at 2100 ms → LED 1 solid from 2000 ms to 7000 ms. The envelope is honoured past release.
69. Hold 1 to `HOLD_MS`, keep holding to 9000 ms → LED 1 stays solid until release. The envelope never truncates while the button is down.
70. Any accepted button press during a running confirmation extinguishes it on that tick. A press discarded by lockout does not.
71. Chord armed → all switch LEDs dark for `CHORD_LED_DELAY_MS`, then 6 and 9 flash in phase at 125/125. A live pending flash is suppressed for the duration.
72. Chord aborted before 5000 ms → the suppressed pending flash resumes on the same tick.
73. Chord re-formed per §6.5 → the 500 ms blackout restarts.
74. Setup entered → the stored channel's LED blinks 1000/1000, including while waiting for 6 and 9 to be released.
75. Tap 3 in setup → the previous channel's LED goes off and LED 3 begins blinking from its **on** phase, on the same tick.
76. Exit setup by 6+9, and separately by 30 s timeout → all ten switch LEDs off.
77. Boot → the stored channel's LED is solid for 2000 ms, then off. A factory-fresh unit lights LED 1.
78. At most two switch LEDs are lit at any instant, in any state, across the full suite.
79. Holding 6, 7, or 9 alone past `HOLD_MS` lights no switch LED.
80. An orphan suffix tap from IDLE lights no switch LED.
81. Criteria 41–56 all still pass unchanged. The panel LED on GP16 and its GP25 mirror are unaffected by this phase.
82. Switch LED playback never delays a cue by more than 10 ms.
83. *(hardware)* All ten LEDs read at comparable brightness from standing height through the finished panel, and the array as a whole is comfortable to look at on a dark stage. The series resistor sets the level for every LED (220 Ω as specified in §11.5.2, arrived at on the bench); **spread between individual LEDs is corrected with `SWITCH_LED_DUTY`, never by changing one LED's resistor.**

**Phase 10 — pair chord and pairing window (§7.5, §8.5) — host**
84. In setup, press 2 and 4 and hold both → at `PAIR_CHORD_HOLD_MS − 1` nothing; at `PAIR_CHORD_HOLD_MS` one `BEGIN_PAIRING` request is issued (bond clear + window open). No MIDI.
85. In setup, press 2+4, release 4 at 1500 ms → no request, and **neither 2 nor 4 selects a channel** on release.
86. Outside setup, press 2, press 4 at +100 ms, hold both → note 28 at 2000 ms (button 2's hold) and lockout; nothing from 4, per §6.4. No pairing request ever.
87. In setup, press 2+4, release 4 at 1000 ms, re-press 4 at 1500 ms, hold → request at 3500 ms (fresh timer on re-form). Releasing 4 does not pause the timer.
88. During the window, taps of 1–10 select nothing and emit no `SetupChannel`; 31 s of inactivity does **not** exit setup.
90. Bond succeeds → window closes, setup remains, and setup exits by inactivity exactly `SETUP_TIMEOUT_MS` after the window closed.
91. Pairing fails → window stays open with its original deadline.
92. No bond by `PAIRING_WINDOW_MS` → window closes, an `END_PAIRING` request is issued, setup remains, inactivity timer restarts.
93. 6+9 exit during the window → `END_PAIRING`, then a normal §7.3 exit (channel commit and flash-write rules unchanged).
94. The 2+4 chord during an open window issues a second `BEGIN_PAIRING` and restarts the deadline.

**Phase 10 — indications (§11.5.8) — host**
89. Window open → LEDs 2 and 4 in phase at 500/500, from the on phase. *Bonding* → 100/100. Bond OK → both solid 2000 ms, then the setup channel LED blinks from its on phase on the next tick.
95. Bonded host becomes ready at 1000 ms after boot → boot channel LED solid to 2000 ms, then LEDs 2 and 4 solid 2000–3000 ms, then off. An accepted press before 2000 ms drops the flash; one during it extinguishes it on that tick.
96. Host becomes ready at 31 s after boot, or reconnects after an earlier ready → no switch LED lights.
100. Criterion 78 (at most two LEDs) holds across every pairing and link test.

**Phase 10 — transport (§9.3, §9.4) — host**
97. USB mounted → every cue goes to USB only. USB not mounted and BLE ready → BLE only. Neither → nothing, no error. Toggling USB mid-sequence moves the very next cue. No cue is ever sent on both.
98. Nothing is sent on either transport in `SETUP` or `LOCKED`, or while the pairing window is open.
99. BLE-MIDI encoder: note 5 on channel 3 at t = 0x1ABC ms → `B5 BC 92 05 01`; at t = 0x2000 the timestamp wraps to 0 → `80 80 …`. Header and timestamp bytes always have bit 7 set. Velocity is `MIDI_VELOCITY`.
101. Criteria 4–82 still pass unchanged on the host build. Criteria 41–56 still pass (panel engine untouched).

**Phase 10 — hardware (`pico2_w`, T39)**
102. *(hardware)* Pico 2 W enumerates exactly as criteria 1 and 2: same name, port, serial, no CDC/MSC.
103. *(hardware)* A GATT scan (nRF Connect or LightBlue) shows only GAP, GATT, and BLE-MIDI. No Device Information, Battery, DFU, or UART service. Advertising and scan response contain only flags, the MIDI service UUID, and the name.
104. *(hardware)* Outside a pairing window, a new phone or Mac cannot pair; the MIDI characteristic cannot be subscribed or read without a bond. An unbonded connection is dropped within ~5 s. A factory-fresh unit does not advertise.
105. *(hardware)* Pair from Audio MIDI Setup on the Mac and from the iPad middleware. Playback receives and triggers on all 33 cues over BLE on both (criterion 3 over BLE). Record the middleware used.
106. *(hardware)* Power-cycle the pedal → the Mac reconnects without re-pairing. Record exactly what the iPad needs to reconnect (automatic, or a tap in the middleware).
107. *(hardware)* Run the pair chord, let the window time out → the previously bonded host can no longer exchange MIDI. Re-pair after "Forget" on the host works.
108. *(hardware)* BLE latency, sealed enclosure on the floor, 100 cues: median ≤ 20 ms, worst ≤ 40 ms from button release to Playback. Record the connection interval granted by each host. USB still meets criterion 11.
109. *(hardware)* With BLE connected, plug USB into the Mac → cues arrive on USB only; Playback shows no double triggers. Unplug USB → the next cue arrives over BLE.
110. *(hardware)* Range: sealed enclosure on the floor, operator standing on it, host at 3 m, 5 m, and 10 m. Record RSSI and dropouts. 30-minute BLE soak at the intended stage distance: no dropped, stuck, or duplicated cues.
111. *(hardware)* The onboard LED (WL_GPIO0) mirrors the panel engine. A review of every `gpio_*` call site in the `pico2_w` build finds none that can receive 23, 24, 25, or 29 (T30's recorded grep).
112. *(hardware)* Instrumented debug build: zero flash erases or programs during a 10-minute cue session and across five reconnects of the bonded host. Writes occur only on bond and on the pair chord.
113. *(hardware)* 9 V only, no USB: unit boots, reconnects to its host, and plays. USB only, no 9 V: unchanged from v1.
114. *(hardware)* Flash a unit holding channel 8 under v1 firmware with the Phase 10 firmware → it still boots on channel 8. The config sector did not move.

Criteria are numbered by topic; 89 sits with the indication criteria by design.

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
| 8 | Should buttons 6, 7, and 9 do anything when held past 2 s? | No — ignored, tap suppressed, no lockout. Button 8 is the sole suffix button with a hold (Mute MIDI); 6 and 9 have the chord exception instead. |
| 9 | Is the bank split 1–5 / 6–10 as assumed? | Yes. Affects documentation and v2 display layout only. |
| 10 | Are 2 s (hold) and 5 s (chord) right under a shoe? | Per spec. Both tunable. Note that at 2 s, a foot resting on button 8 mutes MIDI relatively easily — the shortest hold duration is a direct tradeoff against that. |
| 11 | Security tier target? | Tier 1 for v1. Tier 2 fuses are irreversible; wait for a signed update path. |
| 12 | Does the panel LED (§11.4) change now that per-switch LEDs exist? | **No.** Unchanged, and criteria 41–56 still apply. It is the fallback surface and its engine is already verified. |
| 13 | Pending flash and chord flash share a 125 ms rate. Confusing? | No. Position disambiguates, and chord progress overrides pending, so the two are never lit at once. |
| 14 | Should the valid suffix buttons light dimly during PENDING as an affordance? | Not implemented. It would put three to six LEDs on at once and break the one-indication rule of §11.5.3. Reconsider only with measured need. |
| 15 | Should Setup Mode exit confirm on the switch LEDs? | No — they go off. The §11.4.4 channel blink on the panel LED is already the exit confirmation. |
| 16 | Per-LED PWM: signalling dimension or brightness trim? | **Trim only.** Every pattern in §11.5 is plain on or off. Duty exists to compensate Vf spread. |
| 17 | GP4/GP5 now carry LEDs, retiring the I2C reservation. Acceptable? | Yes. §16's v2 display is superseded; I2C0 could move to GP0/GP1 if a display is ever revived. |
| 18 | BLE pairing is Just Works, with no MITM protection. Acceptable? | **Yes — decided by James, 2026-10-31.** The window is short, physical, and visible (§5.4). |
| 19 | What is BLE for? | **Cable-free operation from the 9 V supply, and older iPads** — decided 2026-10-31. Playback runs on Mac and iPad only; iPad needs BLE MIDI middleware. |
| 20 | USB and BLE both connected? | **USB wins** — decided 2026-10-31 (§9.4). BLE link kept up for failover. |
| 21 | Where do pairing and advertising live? | **Inside Setup Mode, on a 2+4 chord** that clears the bond and starts advertising; pairing persistent — decided 2026-10-31 (§7.5). |
| 22 | Pairing indications? | **LEDs 2 and 4: slow flash advertising, fast flash bonding, one long flash on success; a flash on 2 and 4 if connected at boot** — decided 2026-10-31 (§11.5.8). Rates and durations are assumptions (QUESTIONS Q027). |
| 23 | Latency over BLE? | **Separate, measured BLE budget; 10 ms stays the USB goal** — decided 2026-10-31 (§9.3.5). |
| 24 | Enclosure shielding? | **Deferred until range is measured in the sealed box** — decided 2026-10-31 (§11.6, criterion 110). |
| 25 | Builds? | **`pico2_w` primary; CI keeps building `pico2` until the new board is in hand** — decided 2026-10-31. |
| 26 | Pair chord duration, window length, taps during the window? | 2000 ms; 60 s; taps ignored; inactivity suspended; 6+9 exit closes the window. *Assumed — QUESTIONS Q028.* |
| 27 | Clear the old bond at chord time, or only when a new bond succeeds? | **At chord time**, as James specified — the chord doubles as "forget this host". A timed-out window leaves the pedal unpaired. *Recorded as QUESTIONS Q029 for review.* |
| 28 | Restrict connections by accept-list to the bonded host? | **No.** Apple hosts use resolvable private addresses; filter-list behaviour depends on controller privacy support. Security rests on encryption-required MIDI plus declined pairing outside the window (§5.4). *Assumed — QUESTIONS Q030.* |
| 29 | Indicate later reconnects or disconnects? | **No.** Boot link flash only, once per power cycle, as specified. *Assumed — QUESTIONS Q031.* |
| 30 | Does the panel LED (§11.4) show Bluetooth state? | **No.** It is unpopulated on cape v1 and its engine is finished work. |
| 31 | How does `ui` learn Bluetooth state without changing `UiEvent`? | **By polling `link_state` accessors** — the Q009 pattern. *Assumed — QUESTIONS Q032.* |

---

## 16. Future iterations

**v2 — Per-switch status LEDs. Superseded the display; see §11.5.** Ten LEDs above the ten footswitches deliver the pending, confirmation, lockout, and channel indication a display was going to provide, without an I2C bus, a driver, a framebuffer, or a failure-degradation path. Setup Mode needs no channel readout because the channel *is* the lit LED.

**A display is not planned (decided 2026-10-31).** If one were ever revived, GP4/GP5 carry LEDs, so I2C0 would have to move to GP0/GP1 and the debug UART move or be dropped; it would consume the existing `UiEvent` stream and, on core 1, require `flash_safe_execute` (§10.3).

**Phase 10 — BLE MIDI.** Specified in §5.4, §7.5, §8.5, §9.3, §9.4, §10.1, §11.5.8, §11.6, §11.7. Requires the Pico 2 W.

**Deferred.** WiFi / network MIDI. Expression pedal input (ADC pins reserved). Runtime-editable cue mappings, preset banks, channels 11–16 via a modifier gesture. Retiring the plain `pico2` build once the Pico 2 W is fitted.
