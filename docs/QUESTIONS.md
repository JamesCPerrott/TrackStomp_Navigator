# Questions

Asynchronous handoff between the loop and me. The loop writes here; I answer here.

**How I answer:** fill in the `ANSWER:` line. Change nothing else — not the status, not the ID. The loop picks it up on its next wake, applies it, and marks the entry `[RESOLVED]` itself.

**Statuses**

| Status | Meaning | Who acts |
|---|---|---|
| `[OPEN]` | BLOCKER. Task is stalled. | Me — needs an answer |
| `[ASSUMED]` | Loop picked a default and proceeded. | Me — review when convenient, override if wrong |
| `[RESOLVED]` | Answered and applied. | Nobody, archive |

Before raising anything, the loop must check PRD §15 — most first-instinct questions are already answered there with reasoning.

An `[ASSUMED]` entry I never respond to stands as the decision. Silence is agreement.

---

## Template — BLOCKER

```
## Q001 — [OPEN] — blocks T09

**Task:** T09 setup-mode-state
**Raised:** 2026-09-08
**Type:** BLOCKER — new dependency

**Question:**
One sentence, answerable without reading the whole diff.

**Why I can't proceed:**
What specifically is undecidable from the PRD, and what I checked first
(including which part of PRD §15 I looked at).

**Options:**
- A: ... — consequence
- B: ... — consequence

**My recommendation:** A, because ...

**ANSWER:**
```

## Template — ASSUMPTION

```
## Q002 — [ASSUMED] — T11

**Task:** T11 led-performance-patterns
**Raised:** 2026-09-08
**Type:** ASSUMPTION — local, reversible

**Assumed:**
What I did.

**Reasoning:**
Why this over the alternatives.

**Cost to reverse:** low / medium — and roughly what changes.

**ANSWER (only if overriding):**
```

---

## Open

_None yet._

---

## Assumed

## Q001 — [ASSUMED] — T04

**Task:** T04 debounce
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
A debounced press (rising edge after `DEBOUNCE_MS` of stability) emits
`ButtonEventKind::Tap`. Releases emit nothing. T05 owns tap/hold classification
and will change when `Tap` / `Hold` are produced.

**Reasoning:**
PRD §12.1 only lists TAP, HOLD, and CHORD_HOLD. Adding a Press kind would be a
new event in the public grammar. The bounce exemplar asks for one press event
after the train settles and does not release the button, so the rising edge is
the observable T04 can emit with the existing enum.

**Cost to reverse:** low — T05 will replace the emission site in `buttons_scan`
and update T04's Tap-on-press assertions.

**ANSWER (only if overriding):**

---

## Q002 — [ASSUMED] — T05

**Task:** T05 tap-hold-classification
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
Hold duration is measured from `candidate_since` (the start of the stable
debounce window), so `harness_press(1); harness_advance_to(HOLD_MS)` fires
`HOLD` at 2000. Hold is not armed while a release debounce is in progress, so
a release in the last `DEBOUNCE_MS` of the window still classifies as `TAP`.

**Reasoning:**
PRD §6.3 says a press is classified at the hold threshold or on release, and
T04 requires classification to consume debounced state only. Using the start of
the stable interval matches the timing exemplar. Ignoring a hold tick while the
candidate is released avoids promoting a late tap into a hold across the 20 ms
filter.

**Cost to reverse:** low — change `press_time` assignment and the
`candidate_pressed` guard in `classify_hold`.

**ANSWER (only if overriding):**

---

## Q003 — [ASSUMED] — T06

**Task:** T06 chord-detection
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
A completed chord emits a single `ButtonEventKind::ChordHold` with
`id = CHORD_BUTTON_A` (6). Chord timer start is the second button's
`candidate_since`, matching Q002. Debounced bounce does not clear the timer.

**Reasoning:**
The public event grammar already has `ChordHold`. One event is enough for T09
to enter setup; the pair is identified by kind, not by emitting two ids.
Lockout on that event is T07.

**Cost to reverse:** low — change the `queue_push` id, or emit two events.

**ANSWER (only if overriding):**

---

## Q004 — [ASSUMED] — T07

**Task:** T07 hold-lockout
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
Lockout lives entirely in `input`. After `LOCKED(CHORD)` ends (both 6 and 9
released) the module returns to unlocked; it does not enter SETUP. T09 owns
setup entry from `ChordHold`.

**Reasoning:**
PRD §6.4 lockout is an input concern. SETUP is T09. Emitting `ChordHold` at
threshold (Q003) is enough for the sequencer to enter setup later.

**Cost to reverse:** low — T09 can keep lockout extended until setup-exit if
needed.

**ANSWER (only if overriding):**

---

## Q005 — [ASSUMED] — T08

**Task:** T08 sequence-state-machine
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
`queue_push` copies each `ButtonEvent` into a second ring buffer drained by
`buttons_poll_sequencer_event`. `buttons_poll_event` remains the harness drain.
ChordHold clears PENDING and sends no cue; SETUP is T09. Holds emit `Command`
plus `UiEventKind::Locked`.

**Reasoning:**
T03 forbids the sequencer from emptying the harness queue. A copied drain is
the option named in T08. Notes always come from `CUE_TABLE`.

**Cost to reverse:** low — change which poll the sequencer calls, or the
ChordHold/hold UiEvent kinds.

**ANSWER (only if overriding):**

---

## Q006 — [ASSUMED] — T09

**Task:** T09 setup-mode-state
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
Sequencer notifies input of SETUP via `buttons_set_setup_active` so holds
emit no lockout and the 5 s chord threshold is ignored. Setup-exit overlap
is detected by polling `buttons_accepted_pressed` for 6 and 9 after the
entry pair has been released. Channel is RAM-only (`pending_channel` /
`current_channel`); no flash.

**Reasoning:**
Input does not know SETUP (Q004) but must suppress hold lockout and a
second ChordHold while in the mode (PRD §8.3). A new ButtonEvent kind for
overlap-release would break T06 abort silence. Polling accepted state is
not GPIO. Exit is armed only after both entry feet are off so the entry
release does not immediately leave SETUP.

**Cost to reverse:** low — change the notify/poll API, or emit a dedicated
exit event.

**ANSWER (only if overriding):**

---

## Q007 — [ASSUMED] — T10

**Task:** T10 led-pattern-engine
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
UiEvents are copied into a second ring drained by
`sequencer_poll_ui_event_for_engine` so `ui_tick` can consume them without
emptying the harness drain. T10 lights only priority 3 (setup solid) and 7
(idle off). Priorities 1, 2, 4, 5, 6 stay dark until T11–T13.

**Reasoning:**
Same dual-queue pattern as Q005. Criterion 60 is a per-tick bound, not the
performance waveforms. Setup solid is in the §11.4.1 table, so it belongs
in the engine now.

**Cost to reverse:** low — change which poll `ui_tick` uses, or fold later
patterns into `resolve_stack`.

**ANSWER (only if overriding):**

---

## Q008 — [ASSUMED] — T11

**Task:** T11 led-performance-patterns
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
The lockout flash (priority 4) stays armed after `UiEventKind::Locked` while
`buttons_accepted_pressed` is true for the locked button id. There is no
Unlock event. Hold cues emit `Locked` and not `Sent`, so lockout outranks
cue flash without a separate pulse (criterion 49). A `Sent` while the lamp
is lit or a cue flash is already on inserts `CUE_FLASH_GAP_MS` off first.

**Reasoning:**
The sequencer returns to IDLE immediately after a hold (PRD §8.2) and never
sees the eventual release (lockout lives in input, §12.1). Polling accepted
state is the same pattern as Q006 and is not GPIO. A new Unlock UiEvent
would be a new architectural pattern.

**Cost to reverse:** low — emit Unlock from the sequencer, or have input
report lockout duration some other way.

**ANSWER (only if overriding):**

---

## Q009 — [ASSUMED] — T12

**Task:** T12 led-chord-progress
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
The chord-progress layer polls `buttons_chord_armed()` and
`buttons_chord_start()` and phases the blackout/flash from that start time
(the same clock as the 5 s chord timer). No new UiEvent kind.

**Reasoning:**
Chord detection lives in input (PRD §12.1). The sequencer never sees a
part-formed chord, so it cannot emit start/abort/re-form events. Polling
is the same pattern as Q006/Q008 and is not GPIO. The start timestamp is
the second member's `press_time`, so re-form blackout is 500 ms from the
re-press, not from debounce acceptance.

**Cost to reverse:** low — add chord UiEvents, or have the sequencer poll
and forward.

**ANSWER (only if overriding):**

---

## Q010 — [ASSUMED] — T13

**Task:** T13 led-channel-blink
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
The blink primitive is started by `SetupChannel`, `SetupExit`, and
`ui_indicate_channel` (boot, from `main`). It is not started on
`harness_reset` / clock wrap. `Sent` and `Locked` abort it. `SetupEnter`
also aborts so entry stays solid while 6 and 9 are still held.

**Reasoning:**
Starting a boot blink on every sequencer/UI reset would occupy priority 1
(including the 400 ms off gap) and hide PENDING in T11 tests. Cue abort
covers both tap (`Sent`) and hold (`Locked`) transmissions. The same
elapsed-time primitive is reused for all four sites; “return to” is
whatever sits beneath on the stack.

**Cost to reverse:** low — emit a dedicated blink UiEvent, or start boot
from sequencer init.

**ANSWER (only if overriding):**

---

## Q011 — [ASSUMED] — T14

**Task:** T14 gpio-input
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
`buttons_init()` configures GP6–GP15 as inputs with internal pull-ups and
is called from `main` at boot. It is a no-op under `HOST_TEST`.
`buttons_gpio_levels()` is a single masked `gpio_get_all()`. The debounce
scan loop is still T20. On-device confirmation is hardware (T22).

**Reasoning:**
T20 owns the main loop. Init must happen before any scan, so boot is the
right site (same as T13's `ui_indicate_channel`). Host tests keep the
harness `buttons_gpio_levels()` in `gpio_host.cpp`.

**Cost to reverse:** low — move init into `buttons_scan` first tick, or
fold it into T20.

**ANSWER (only if overriding):**

---

## Q012 — [ASSUMED] — T15

**Task:** T15 usb-midi-descriptors
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
MIDI descriptors use TinyUSB's AC + MS pair (standard USB-MIDI 1.0) with
one virtual cable. An OUT bulk endpoint is declared because the TinyUSB
MIDI template requires it; PRD §9.2 allows that. The embedded IN jack
string is `Navigator Cues` via `TUD_MIDI_DESC_JACK_DESC`, not
`TUD_MIDI_DESCRIPTOR` (which hardcodes jack string index 0).
`tusb_init` / `tud_task` remain T20. PID is the `0xFFFE` placeholder.

**Reasoning:**
A single Audio/MIDI function is “MIDI only” — no CDC, MSC, or vendor.
Host enumeration is hardware (criteria 1, 2).

**Cost to reverse:** low — drop the OUT endpoint if a custom descriptor is
written, or start USB in T15.

**ANSWER (only if overriding):**

---

## Q013 — [ASSUMED] — T16

**Task:** T16 midi-output
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
`midi_out` owns the wire channel and sends a full 3-byte Note On via
`tud_midi_stream_write`. The sequencer still does not call MIDI
(PRD §12.1); T20 will drain `Command` into `midi_out_send`. SETUP/LOCKED
emit no extra Commands already, so midi_out does not gate on those
states. Sequencer RAM `current_channel` remains for setup UI events
until T19/T20 copy it into midi_out on setup exit.

**Reasoning:**
A dual Command queue (like Q005) would be needed if midi_out consumed
`sequencer_poll_command` inside the harness pipeline. Direct `midi_out_send`
keeps T08–T09 tests green.

**Cost to reverse:** low — dual-queue drain, or have T20 call
`midi_out_set_channel` from SetupExit.

**ANSWER (only if overriding):**

---

## Q014 — [ASSUMED] — T17

**Task:** T17 led-driver
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
`ui_led_init()` at boot sets GP16 and GP25 as outputs, 4 mA drive on
GP16 only, both low. Each `ui_tick` writes `g_lamp` to both pins with no
inversion. Host is a no-op. The main scan/tick loop remains T20, so the
pins stay off until then.

**Reasoning:**
Thin shim in the existing UI module; no new layer. Pattern logic stays in
`resolve_stack`. On-device tracking of both LEDs is T22.

**Cost to reverse:** low — split a `led.cpp` driver, or set GP25 drive too.

**ANSWER (only if overriding):**

---

## Q015 — [ASSUMED] — T18

**Task:** T18 config-store-read
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
CRC32 is CRC-32/ISO-HDLC (polynomial `0xEDB88320`, init `0xFFFFFFFF`,
final xor `0xFFFFFFFF`), computed over the eight bytes preceding `crc32`.

**Reasoning:**
PRD §10.1 says “crc32 over all preceding bytes” without naming a
polynomial. IEEE/ISO-HDLC is the usual embedded CRC-32 and matches
erased `0xFF` failing validation.

**Cost to reverse:** medium — stored records would need a version bump or
a rewrite on next setup exit.

**ANSWER (only if overriding):**

---

## Q016 — [ASSUMED] — T18

**Task:** T18 config-store-read
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
`pico_override_flash_size` shrinks the linker FLASH length to
`(4 * 1024 * 1024) - 4096` (Pico 2 4 MB minus one sector). That also
redefines `PICO_FLASH_SIZE_BYTES`, which `config_store` uses as the
XIP offset of the reserved last sector.

**Reasoning:**
SDK 2.3.1 documents this as the way to exclude a tail region from the
image without a custom memmap. The config then lives at
`XIP_BASE + PICO_FLASH_SIZE_BYTES`.

**Cost to reverse:** low — different offset math if flash size is not
overridden, or a dedicated `CONFIG_FLASH_OFFSET` constant.

**ANSWER (only if overriding):**

---

## Q017 — [ASSUMED] — T18

**Task:** T18 config-store-read
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
Boot in `main` calls `config_store_read_channel()`, applies it with
`midi_out_set_channel` and `ui_indicate_channel`, and never writes.
Sequencer RAM channel is unchanged (T19/T20). Host tests inject a RAM
copy of `ConfigRecord`; `config_store_host_write_count()` stays 0
because T18 has no write path.

**Reasoning:**
PRD §12.1 allows `config_store` at boot. Applying the channel here
covers criteria 39/54 on a factory-fresh read of 1. T20 still owns the
scan loop.

**Cost to reverse:** low — defer midi_out/LED apply to T20, or dual-queue
the sequencer channel.

**ANSWER (only if overriding):**

---

## Q018 — [ASSUMED] — T19

**Task:** T19 config-store-write
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
`config_store_write_channel` skips erase/program when the argument equals
`config_store_read_channel()` (validated/fallback value) or is outside
1–10. The RAM helper uses `__no_inline_not_in_flash_func` so it cannot
be inlined back into flash, and it wraps `flash_range_erase` /
`flash_range_program` with `save_and_disable_interrupts`. T20 will call
this on setup exit; T19 does not touch the sequencer or main loop. Host
tests count programs via `config_store_host_write_count`.

**Reasoning:**
PRD §7.3 compares pending to the stored channel; after T18, that is the
read API (erased flash reports 1). A noinline RAM function is the SDK
way to keep XIP-off code off the flash image. Wiring the call site is
T20 so sequencer still never calls flash.

**Cost to reverse:** low — call from sequencer, or use `flash_safe_execute`.

**ANSWER (only if overriding):**

---

## Q019 — [ASSUMED] — T20

**Task:** T20 main-loop-integration
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
`loop_dispatch` runs after `sequencer_tick` and before `ui_tick`. Commands
are copied into a MIDI drain (`sequencer_poll_command_for_midi`) so the
harness Command capture stays intact. Setup exit latches a channel via
`sequencer_poll_setup_commit` rather than stealing `UiEvent`. USB is
started with `tusb_init(BOARD_TUD_RHPORT, &dev_init)`. Clock is
`to_ms_since_boot(get_absolute_time())`. `sleep_us(500)` matches §12.2.
The harness pipeline calls `loop_dispatch`; `harness_reset` also calls
`midi_out_reset`. Core 1 is not started.

**Reasoning:**
PRD §12.1 forbids the sequencer from calling MIDI or flash. Dual Command
queue is the Q005/Q013 pattern. A setup-commit latch avoids a third UI
queue. TinyUSB 0.17 in SDK 2.3.1 rejects `tusb_init(void)` unless
`CFG_TUSB_RHPORT0_MODE` is set.

**Cost to reverse:** low — drain UI SetupExit instead, or init with
`CFG_TUSB_RHPORT0_MODE`.

**ANSWER (only if overriding):**

---

## Q020 — [ASSUMED] — T21

**Task:** T21 release-build-hardening
**Raised:** 2026-09-11
**Type:** ASSUMPTION — local, reversible

**Assumed:**
Release path checks use `strings -d` on the ELF (loadable sections) and
`strings -a` on the UF2. `-ffile-prefix-map` rewrites `__FILE__`/DWARF
to `.` and `pico-sdk`. `DEBUG_UART` defines a compile flag and calls
`stdio_init_all()` so UART stdio on GP0/GP1 actually starts. USB stdio
stays off. Tier 2 is untouched.

**Reasoning:**
DWARF in the unstripped ELF still has compiler paths; the flashed image
does not. Prefix-map plus a post-build grep matches T21's "grep of the
built binary" bar without stripping debug info needed for local dumps.
`pico_enable_stdio_uart(1)` is inert without `stdio_init_all`.

**Cost to reverse:** low — also map `/usr/include`, or skip `stdio_init_all`.

**ANSWER (only if overriding):**

---

## Resolved

_None yet._
