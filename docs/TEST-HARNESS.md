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

`harness_reset()` sets `now = 0`, releases every button, clears captures, and runs one idle pipeline pass at t=0.

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

Return the harness to t=0, all buttons released, empty captures, one idle scan at t=0.

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

---

## Event types (module headers)

Defined in `src/input/buttons.h`:

```cpp
enum class ButtonEventKind : uint8_t { Tap, Hold, ChordHold };
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
