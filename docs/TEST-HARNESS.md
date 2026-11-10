# Test harness API

Source of truth for host tests. Imitate [`tests/test_bounce.cpp`](../tests/test_bounce.cpp), [`tests/test_timing.cpp`](../tests/test_timing.cpp), [`tests/test_capture.cpp`](../tests/test_capture.cpp), and, for per-switch LEDs, [`tests/test_switch_leds.cpp`](../tests/test_switch_leds.cpp). Do not invent a parallel scaffolding.

Host build:

```bash
cmake --preset host
ninja -C build-host
ctest --test-dir build-host --output-on-failure
```

The `host` preset configures Ninja into `build-host` with `BUILD_HOST_TESTS=ON` and `CMAKE_EXPORT_COMPILE_COMMANDS=ON`. There is no Pico SDK on this path.

## Clock and press contract

`harness_reset()` sets `now = 0`, releases every button, resets `buttons`, `sequencer`, `ui`, `midi_out`, and `link_state` to their power-up state, clears captures, and runs one idle pipeline pass at t=0. The module resets are unconditional: they do not depend on the clock having moved since the last reset ([`tests/test_reset.cpp`](../tests/test_reset.cpp)).

`harness_press` / `harness_release` / `harness_set_pressed` change the pin and run `buttons_scan` → `sequencer_tick` → `ui_tick` **at the current timestamp** without moving the clock. That is what makes this land on the constant, not one millisecond past it:

```cpp
harness_press(1);
harness_advance_to(HOLD_MS); // now == 2000, not 2001
```

`harness_advance(ms)` does `ms` times `{ now += 1; pipeline(now); }`. `harness_advance(0)` is a no-op. `harness_advance_to(t)` fails the test if `t` is less than `now`.

Buttons are logical **1–10**. Tests never mention GP numbers. The host `buttons_gpio_levels()` mask is active-low using `BUTTON_GPIO_BASE` for T04/T14.

After each pipeline step the harness drains `buttons_poll_event`, `sequencer_poll_command`, and `sequencer_poll_ui_event` into capture buffers and appends `ui_lamp()` to the lamp trace and `ui_switch_leds()` to the switch trace (one sample each per pipeline pass). A press, release, or `set_pressed` at an already-sampled timestamp appends a second sample for that same `now`.

---

## Clock

### `void harness_reset()`

Return the harness to t=0, all buttons released, every module at power-up state, empty captures, one idle scan at t=0. It is a power-up whatever the clock read before: anything started before the call, including `ui_indicate_channel`, is gone, so start a boot indication **after** the reset.

```cpp
harness_reset();
REQUIRE(harness_now_ms() == 0U);
```

### `uint32_t harness_now_ms()`

Current fake millisecond time.

```cpp
harness_advance(20);
REQUIRE(harness_now_ms() == 20U);
```

### `void harness_advance(uint32_t ms)`

Elapse `ms` milliseconds, running the pipeline once per millisecond.

```cpp
harness_press(1);
harness_advance(DEBOUNCE_MS);
```

### `void harness_advance_to(uint32_t t)`

Advance until `now == t`. No-op if already there. Fails if `t < now`.

```cpp
harness_press(1);
harness_advance_to(HOLD_MS - 1U);
REQUIRE_NO_COMMANDS();
harness_advance_to(HOLD_MS);
```

---

## GPIO

### `void harness_set_pressed(uint8_t button, bool pressed)`

Set logical button `button` (1–10) and process the current timestamp.

```cpp
harness_set_pressed(8, true);
REQUIRE(harness_button_pressed(8));
```

### `void harness_press(uint8_t button)`

Press a button at the current time; do not move the clock.

```cpp
harness_press(2);
harness_advance(DEBOUNCE_MS);
```

### `void harness_release(uint8_t button)`

Release a button at the current time; do not move the clock.

```cpp
harness_release(2);
harness_advance(DEBOUNCE_MS);
```

### `bool harness_button_pressed(uint8_t button)`

Logical pressed state after the last injection.

```cpp
REQUIRE(harness_button_pressed(1));
```

### `void harness_inject_bounce(uint8_t button, uint8_t transitions, uint32_t duration_ms)`

Chatter `button` with `transitions` edges spread across `duration_ms` (several edges may share a millisecond; the last edge wins that sample). Leaves the button **pressed**, adding one extra edge if the chatter ended released.

```cpp
harness_inject_bounce(1, 20, 10);
harness_advance(DEBOUNCE_MS);
REQUIRE_NO_BUTTON_EVENTS(); // T04: exactly one press
```

### `uint32_t harness_gpio_transition_count(uint8_t button)`

Level changes on that button since `harness_reset()`. Use this to prove a bounce train actually chattered.

```cpp
harness_inject_bounce(1, 20, 10);
REQUIRE(harness_gpio_transition_count(1) >= 20U);
```

---

## Captures and assertions

Assertions print `file:line` and exit 1 on failure. Empty command/UI/button lists are valid and mean "nothing was emitted".

### `REQUIRE(cond)`

Generic boolean check.

```cpp
REQUIRE(harness_now_ms() == HOLD_MS);
```

### `REQUIRE_COMMANDS(...)` / `REQUIRE_NO_COMMANDS()`

Ordered `Command.note` match. `REQUIRE_COMMANDS()` with no arguments is the same as `REQUIRE_NO_COMMANDS()`.

```cpp
REQUIRE_NO_COMMANDS();
REQUIRE_COMMANDS(Command{5}, Command{20});
```

### `REQUIRE_UI_EVENTS(...)` / `REQUIRE_NO_UI_EVENTS()`

Ordered match of `UiEvent.kind` and `UiEvent.value`.

```cpp
REQUIRE_NO_UI_EVENTS();
REQUIRE_UI_EVENTS(UiEvent{UiEventKind::Pending, 2}, UiEvent{UiEventKind::Sent, 5});
```

### `REQUIRE_BUTTON_EVENTS(...)` / `REQUIRE_NO_BUTTON_EVENTS()`

Ordered match of `ButtonEvent.id` and `ButtonEvent.kind`.

```cpp
REQUIRE_NO_BUTTON_EVENTS();
REQUIRE_BUTTON_EVENTS(ButtonEvent{1, ButtonEventKind::Tap});
```

### `REQUIRE_LAMP(expected)`

Current `ui_lamp()` boolean.

```cpp
REQUIRE_LAMP(false);
```

### `const std::vector<uint8_t>& harness_lamp_trace()`

0/1 per pipeline pass. Index equals `now` only until a press, release, or `set_pressed` runs at an already-sampled timestamp; `harness_clear_captures()` empties the trace. T10–T13 assert waveforms from this.

```cpp
harness_advance(CUE_FLASH_MS);
const std::vector<uint8_t>& trace = harness_lamp_trace();
REQUIRE(!trace.empty());
```

### `void harness_clear_captures()`

Clear command, UI, button, lamp-trace, and switch-trace buffers. Does not move the clock or change GPIO.

```cpp
harness_clear_captures();
REQUIRE_NO_COMMANDS();
```

---

## Per-switch LEDs (PRD §11.5)

A parallel surface to the panel lamp, captured the same way. **`harness_lamp_trace()` and
`REQUIRE_LAMP` are unchanged** — they still refer to the GP16/GP25 panel LED only, and T10–T13
continue to assert against them exactly as written.

State is a **10-bit mask**, bit 0 being LED 1 (above button 1) through bit 9 being LED 10. Tests
name logical buttons, never GP numbers. A mask with any bit at or above `SWITCH_LED_COUNT` fails
the test when a pipeline pass samples it, or when `harness_debug_set_switch_leds` is given one.

### `SWITCH_LED(n)`

Mask for one LED. `n` is a constant expression from 1 to 10; `SWITCH_LED(0)` and `SWITCH_LED(11)`
do not compile. Combine with `|`.

```cpp
REQUIRE_SWITCH_LEDS(SWITCH_LED(1) | SWITCH_LED(6));
```

### `uint16_t harness_switch_leds()`

Current mask. Each pipeline pass copies `ui_switch_leds()` into it.

```cpp
REQUIRE(harness_switch_leds() == SWITCH_LED(3));
```

### `REQUIRE_SWITCH_LEDS(mask)` / `REQUIRE_NO_SWITCH_LEDS()`

Exact match on the current mask, the value `harness_switch_leds()` returns. `REQUIRE_NO_SWITCH_LEDS()` is the same as passing 0. On failure the message names both masks as LED numbers and as a 10-bit field with LED 1 at the right, then exits 1.

These are **exact**, not subset, matches. PRD §11.5.3 permits only one indication at a time, so a
test that passes while an unexpected LED is also lit would hide precisely the defect criterion 78
exists to catch.

```cpp
harness_press(1);
harness_release(1);
harness_advance(DEBOUNCE_MS);
REQUIRE_SWITCH_LEDS(SWITCH_LED(1));   // pending flash, on phase
```

### `const std::vector<uint16_t>& harness_switch_trace()`

One mask per pipeline pass, appended in the same pass as the lamp sample, so the two traces stay
the same length. Index equals `now` only until a press, release, or `set_pressed` runs at an
already-sampled timestamp; that call appends a second sample while `now` stays put.
`harness_clear_captures()` empties the trace. Waveform assertions in T26–T27 read this. The index
in the example below is the timestamp only because the trace was filled by reset and `advance`,
with no press at an already-sampled time.

```cpp
harness_advance(LED_CONFIRM_MS);
const std::vector<uint16_t>& trace = harness_switch_trace();
REQUIRE(trace.at(LED_SELF_PAIR_ON_MS + 1U) == 0U);   // inside the self-pair notch
```

### `uint8_t harness_switch_led_count()`

Number of bits set in the current mask. The cheap guard for criterion 78.

```cpp
REQUIRE(harness_switch_led_count() <= 2U);
```

### Shape for waveform tests

Assert both edges of every transition, the same way `test_timing.cpp` brackets a threshold. A test
that only checks the lit state will pass against an engine that never turns the LED off.

```cpp
harness_advance_to(LED_CONFIRM_MS - 1U);
REQUIRE_SWITCH_LEDS(SWITCH_LED(1) | SWITCH_LED(6));
harness_advance_to(LED_CONFIRM_MS);
REQUIRE_NO_SWITCH_LEDS();
```

---

## Link and transport (Phase 10, PRD §7.5, §8.5, §9.4)

> **Final, as built by T32.** The loop follows this section as committed. Everything above this
> heading is unchanged.

A third surface beside the lamp and switch traces. The harness is the **host implementation of
[`src/midi/ble_midi.h`](../src/midi/ble_midi.h)**, the BLE transport boundary, so nothing reaches
TinyUSB or BTstack on the host and `loop_dispatch` is the same code that ships. Each pipeline pass
runs `buttons_scan` → `sequencer_tick` → `link_tick` → `loop_dispatch` → `ui_tick`.

`loop_dispatch` carries every hop, in this order (PRD §12.2):

1. Commands → `midi_out_send`, which asks `transport_select` per cue.
2. The sequencer's link requests → `link_on_request`. **T35 adds this hop**, draining
   `bool sequencer_poll_link_request(LinkRequest* out)`. Nothing produces a request before then.
3. `link_poll_request` → `ble_midi_request`. The fake transport records each request, and
   `REQUIRE_LINK_REQUESTS` reads that record.
4. `ble_midi_poll_event` → `link_on_event`. The fake transport hands over what `harness_link_event`
   queued.

**Tests never call `link_on_request`, `link_on_event`, `link_poll_request`, or a `ble_midi_*`
function.** `loop_dispatch` is their only caller. `link_poll_request` has a single queue, so a test
that polls it takes the request away from the transport.

Events arrive in step 4, after the commands of step 1, so an event affects cues resolved on later
passes. `harness_link_event` runs a pass itself: when it returns, the event has been applied.

`harness_reset()` calls `midi_out_reset()` and `link_reset()` beside `buttons_reset()`,
`sequencer_reset()`, and `ui_reset()`, and empties the fake transport: USB **mounted**, BLE **not ready**, window closed, no request or event queued, and
"power-up" at t=0 (the boot-connect window of §11.5.8 is measured from the reset). The fake holds no
bond; a bonded host is modelled by injecting `Ready`.

What is real after T32, and which task fills in the rest:

| Piece | After T32 | Filled in by |
|---|---|---|
| `link_ready()` | Real. `Ready` sets it, `Lost` clears it. | — |
| `link_on_request` | Forwards the request to the transport unchanged. | T36 puts the window in front |
| Window, `link_pairing_phase()` | Never opens; `Closed`. Pairing events and `NoRadio` are ignored. | T36 |
| `link_boot_connected()` | Always false. | T37 |
| `transport_select` | Always `Transport::Usb`. `midi_out_send` already calls it per cue. | T34 |
| Pair chord → request | Nothing. | T35 |

Types. `harness.h` includes both module headers.

```cpp
// src/link/link_state.h
enum class LinkEventKind : uint8_t {
    Ready,             // encrypted with the bonded key and subscribed (§9.3.1)
    Lost,              // disconnected, or no longer ready
    PairingStarted,    // a central began pairing inside the window
    PairingSucceeded,  // bond stored
    PairingFailed,
    NoRadio            // the transport has no radio
};
struct LinkEvent { LinkEventKind kind; };

enum class LinkRequestKind : uint8_t {
    BeginPairing,      // clear bond, advertise for pairing (§7.5 steps 1–2)
    EndPairing         // stop pairing advertising
};
struct LinkRequest { LinkRequestKind kind; };

enum class LinkPairingPhase : uint8_t { Closed, Advertising, Bonding, BondOk };

// src/midi/transport.h
enum class Transport : uint8_t { None, Usb, Ble };

// tests/harness/harness.h
struct TransportSend { Transport transport; uint8_t note; };
```

### `void harness_set_usb_mounted(bool mounted)`

What `tud_mounted()` reports to the §9.4 policy. Takes effect for the next cue. Runs one pipeline
pass at the current timestamp, like `harness_press`, so it appends one lamp and one switch sample
and does not move the clock.

```cpp
harness_set_usb_mounted(false);
```

### `void harness_link_event(LinkEventKind kind)`

Queue a transport event in the fake transport and run one pipeline pass at the current timestamp.
`loop_dispatch` polls it and delivers it to `link_on_event` on that pass. The harness does not call
`link_on_event` itself.

```cpp
harness_link_event(LinkEventKind::Ready);
REQUIRE(link_ready());
```

### `REQUIRE_LINK_REQUESTS(...)` / `REQUIRE_NO_LINK_REQUESTS()`

Ordered, exact match of the `LinkRequest` kinds that reached the transport since the last clear.
`REQUIRE_LINK_REQUESTS()` with no arguments is the same as `REQUIRE_NO_LINK_REQUESTS()`.

```cpp
REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::BeginPairing});
```

### `REQUIRE_TRANSPORT_SENDS(...)` / `REQUIRE_NO_TRANSPORT_SENDS()`

Ordered, exact match of every cue that reached a wire, with which one. This is the §9.4 assertion.
`REQUIRE_COMMANDS` is unchanged and still asserts what the sequencer emitted; this asserts where it
went. A cue the policy dropped appears in `REQUIRE_COMMANDS` and not here.

The list is **decoded from the bytes on the two fake wires**, by the harness's own parser. It is not
a log kept by `midi_out`, and it shares no code with `midi_out` or the BLE encoder. Both wires feed
one ordered list, so a cue that reaches both appears twice and the match fails.

- **USB:** every byte `midi_out` writes, three bytes per message. `{Usb, note}` is byte 1 of a
  Note On.
- **BLE:** every packet given to `ble_midi_send`. It must be five bytes: header `10xxxxxx`,
  timestamp with bit 7 set, then the same three-byte message. `{Ble, note}` is byte 3.
- **Bytes the parser cannot decode fail the test**, at a `tests/harness/harness.cpp` line, with the
  wire, the timestamp, and the bytes. That covers a status that is not Note On, a data byte with
  bit 7 set, a short message, and a BLE packet of the wrong length or framing.
- A Note Off is accepted only when `SEND_NOTE_OFF` is true, and is not a send.
- `Transport::None` never appears in the list.

```cpp
harness_press(10);
harness_advance(DEBOUNCE_MS);
harness_release(10);
harness_advance(DEBOUNCE_MS);
REQUIRE_COMMANDS(Command{20});
REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Usb, 20});
```

The BLE shape, which passes from T34 on:

```cpp
harness_set_usb_mounted(false);
harness_link_event(LinkEventKind::Ready);
// ...tap 10...
REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Ble, 20});
```

### `const std::vector<std::vector<uint8_t>>& harness_ble_packets()`

The raw packets given to `ble_midi_send`, one entry per call, in order (T33/T34).

```cpp
REQUIRE(harness_ble_packets().back().size() == 5U);
```

### Read-only link accessors

These are the module's real accessors, and tests call them directly. There are no `harness_link_*`
wrappers. The sequencer, `ui`, and `midi_out` poll the same ones (QUESTIONS Q032).

```cpp
bool link_pairing_window_open();
LinkPairingPhase link_pairing_phase();   // Closed, Advertising, Bonding, BondOk
bool link_ready();
bool link_boot_connected();              // §11.5.8: host first became ready inside the boot window
```

`link_pairing_phase()` can be `BondOk` while `link_pairing_window_open()` is false: the window
closes when the bond succeeds and the long flash plays after it (§7.5).

`link_boot_connected()` latches when the host first becomes ready within `BOOT_CONNECT_WINDOW_MS`
of power-up and is never cleared. `link_state` cannot know whether the flash has played or was
dropped by a press, so `ui` tracks that itself.

```cpp
REQUIRE(!link_pairing_window_open());
REQUIRE(link_pairing_phase() == LinkPairingPhase::Closed);
```

### `Transport transport_select(bool usb_mounted, bool ble_ready)`

The §9.4 policy, in `src/midi/transport.h`. Pure, so T34 tests the table by calling it directly as
well as through the pipeline.

```cpp
REQUIRE(transport_select(true, true) == Transport::Usb);
```

### `void harness_clear_captures()`

Also clears the link-request, transport-send, and BLE-packet buffers. It does not change USB
mounted, link state, or an event still queued in the fake transport.

```cpp
harness_clear_captures();
REQUIRE_NO_LINK_REQUESTS();
REQUIRE_NO_TRANSPORT_SENDS();
```

### Host seams the harness uses — not for tests

```cpp
void midi_out_set_usb_mounted(bool mounted);        // behind harness_set_usb_mounted
bool midi_out_poll_byte_for_harness(uint8_t* out);  // the harness's copy of the USB wire
bool midi_out_harness_bytes_lost();                 // that copy overflowed; the harness fails the test
void link_reset();                                  // behind harness_reset
void buttons_reset();                               // behind harness_reset
void sequencer_reset();                             // behind harness_reset
void ui_reset();                                    // behind harness_reset, both LED engines
```

`midi_out_poll_byte` is unchanged and stays the drain for tests that assert raw USB bytes
(`test_midi_out.cpp`, `test_loop.cpp`). The harness reads its own copy and never consumes it.

### Shape

Imitate [`tests/test_link.cpp`](../tests/test_link.cpp). Assert `REQUIRE_COMMANDS` and
`REQUIRE_TRANSPORT_SENDS` together for every cue. Bracket every window threshold the way
`test_timing.cpp` brackets `HOLD_MS`. The window shape, which passes from T36 on:

```cpp
harness_advance_to(window_start + PAIRING_WINDOW_MS - 1U);
REQUIRE(link_pairing_window_open());
REQUIRE_NO_LINK_REQUESTS();
harness_advance_to(window_start + PAIRING_WINDOW_MS);
REQUIRE(!link_pairing_window_open());
REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::EndPairing});
```

---

## T03 only — do not copy

These seed capture buffers so a matcher can be proven before the producer exists. Later tasks emit through the pipeline; do not call these. `harness_debug_set_switch_leds` exists because `ui_switch_leds()` returns 0 until the per-switch engine is implemented. It stores the current mask and, when the trace is non-empty, replaces the last sample. It does not append and does not move the clock. The next pipeline pass overwrites it from `ui_switch_leds()`.

```cpp
void harness_debug_push_command(Command command);
void harness_debug_push_ui_event(UiEvent event);
void harness_debug_push_button_event(ButtonEvent event);
void harness_debug_set_switch_leds(uint16_t mask);
```

```cpp
harness_debug_push_command(Command{5});
REQUIRE_COMMANDS(Command{5});
```

```cpp
harness_debug_set_switch_leds(SWITCH_LED(1));
REQUIRE_SWITCH_LEDS(SWITCH_LED(1));
```

## T32 only — do not copy

These exist so the link-request and BLE-packet captures could be proven before anything produced
either. From T34 `midi_out` produces the packets, and from T35 the pair chord produces the requests.
Later tasks drive buttons and `harness_link_event`; do not call these. There is no seeder for a
USB send: a tap already produces one.

```cpp
void harness_debug_link_request(LinkRequestKind kind);
void harness_debug_ble_packet(std::initializer_list<uint8_t> bytes);
```

`harness_debug_link_request` calls `link_on_request` and runs one pipeline pass at the current
timestamp, so the request travels through `link_state` and `loop_dispatch` to the fake transport.

```cpp
harness_debug_link_request(LinkRequestKind::BeginPairing);
REQUIRE_LINK_REQUESTS(LinkRequest{LinkRequestKind::BeginPairing});
```

`harness_debug_ble_packet` gives the bytes to the fake transport's `ble_midi_send`, where the
harness parser reads them. It runs no pipeline pass and does not move the clock.

```cpp
harness_debug_ble_packet({0x80, 0x80, 0x90, 5, MIDI_VELOCITY});
REQUIRE_TRANSPORT_SENDS(TransportSend{Transport::Ble, 5});
```

---

## Event types (module headers)

Defined in `src/input/buttons.h`:

```cpp
enum class ButtonEventKind : uint8_t { Tap, Hold, ChordHold };
// T35 adds PairChordHold (Phase 10, setup only). It is not in the code before T35.
struct ButtonEvent {
    uint8_t id;
    ButtonEventKind kind;
};
```

Defined in `src/sequencer/sequencer.h`:

```cpp
struct Command {
    uint8_t note;
};

enum class UiEventKind : uint8_t {
    Idle, Pending, Locked, Sent, SetupEnter, SetupChannel, SetupExit
};
struct UiEvent {
    UiEventKind kind;
    uint8_t value; // button, note, or channel; 0 when unused
};
```

`src/ui/ui.h` exposes `ui_tick(now)`, `ui_lamp()`, and `ui_switch_leds()`.

`ui_lamp()` returns the panel LED boolean (§11.4). `ui_switch_leds()` returns the 10-bit per-switch mask (§11.5). The two engines are independent; neither reads the other.

---

## Exemplar shapes

Imitate the exemplar for the shape you need.

**Bounce** ([`tests/test_bounce.cpp`](../tests/test_bounce.cpp)): `inject_bounce` then `advance(DEBOUNCE_MS)`. T04 asserts one press event.

**Timing** ([`tests/test_timing.cpp`](../tests/test_timing.cpp)): `press` then `advance_to` on each side of a threshold (`HOLD_MS - 1` vs `HOLD_MS`). T05 asserts the hold command.

**Capture** ([`tests/test_capture.cpp`](../tests/test_capture.cpp)): ordered list and empty assertions on Command / UiEvent / ButtonEvent. T08 fills them from real sequencer output.

**Switch LEDs** ([`tests/test_switch_leds.cpp`](../tests/test_switch_leds.cpp)): exact mask match and one sample per pipeline pass. T26 and T27 imitate this shape. Do not copy `harness_debug_set_switch_leds`.

**Link and transport** ([`tests/test_link.cpp`](../tests/test_link.cpp)): `REQUIRE_COMMANDS` beside `REQUIRE_TRANSPORT_SENDS` for a cue, `harness_link_event` then a link accessor, and ordered `REQUIRE_LINK_REQUESTS`. T34–T37 imitate this shape. Do not copy `harness_debug_link_request` or `harness_debug_ble_packet`.
