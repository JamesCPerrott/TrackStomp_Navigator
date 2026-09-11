# Test harness API

Source of truth for host tests. Imitate [`tests/test_bounce.cpp`](../tests/test_bounce.cpp), [`tests/test_timing.cpp`](../tests/test_timing.cpp), and [`tests/test_capture.cpp`](../tests/test_capture.cpp). Do not invent a parallel scaffolding.

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

After each pipeline step the harness drains `buttons_poll_event`, `sequencer_poll_command`, and `sequencer_poll_ui_event` into capture buffers and appends `ui_lamp()` to the lamp trace (one sample per processed timestamp).

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

0/1 per processed timestamp. Index `t` is the sample taken when `now == t` after a reset (until `harness_clear_captures()`). T10–T13 assert waveforms from this.

```cpp
harness_advance(CUE_FLASH_MS);
const std::vector<uint8_t>& trace = harness_lamp_trace();
REQUIRE(!trace.empty());
```

### `void harness_clear_captures()`

Clear command, UI, button, and lamp-trace buffers. Does not move the clock or change GPIO.

```cpp
harness_clear_captures();
REQUIRE_NO_COMMANDS();
```

---

## T03 only — do not copy

The sequencer is still a stub, so the capture exemplar seeds buffers to prove the matcher. Later tasks emit through the pipeline; do not call these.

```cpp
void harness_debug_push_command(Command command);
void harness_debug_push_ui_event(UiEvent event);
void harness_debug_push_button_event(ButtonEvent event);
```

```cpp
harness_debug_push_command(Command{5});
REQUIRE_COMMANDS(Command{5});
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

`src/ui/ui.h` exposes `ui_tick(now)` and `ui_lamp()`.

---

## Exemplar shapes

Copy these three files, then replace the placeholder assertions.

**Bounce** ([`tests/test_bounce.cpp`](../tests/test_bounce.cpp)): `inject_bounce` then `advance(DEBOUNCE_MS)`. T04 asserts one press event.

**Timing** ([`tests/test_timing.cpp`](../tests/test_timing.cpp)): `press` then `advance_to` on each side of a threshold (`HOLD_MS - 1` vs `HOLD_MS`). T05 asserts the hold command.

**Capture** ([`tests/test_capture.cpp`](../tests/test_capture.cpp)): ordered list and empty assertions on Command / UiEvent / ButtonEvent. T08 fills them from real sequencer output.
