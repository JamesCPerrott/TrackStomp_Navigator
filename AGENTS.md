# Agent rules — MIDI Foot Controller

Applies to every agent session in this repo, looped or interactive.

## Sources of truth

| Concern | File | Rule |
|---|---|---|
| Requirements, behaviour, timing | `docs/PRD.md` | Authoritative. Never contradict it. |
| Work breakdown, branches, commits | `docs/TASK-LIST.md` | Authoritative for scope and ordering. |
| Test harness API | `docs/TEST-HARNESS.md` | Use these functions. Do not invent new test scaffolding. |
| Unanswered decisions | `docs/QUESTIONS.md` | Check before asking anything new. |
| History of what was tried | `docs/PROGRESS.md` | Append-only. Never rewrite. |

PRD §15 lists decisions already made with reasoning. Read it before raising a question — most first-instinct questions are answered there.

Use `/context7-mcp` for Pico SDK, TinyUSB, and CMake documentation. Do not rely on recalled API signatures.

## Hard stops

**Never perform RP2350 OTP fuse operations, secure boot signing, or bootloader/SWD lockdown.** PRD §5.2 is explicitly out of scope for v1. These operations are irreversible and permanently brick the board. If a task appears to require them, that is a BLOCKER question, not a thing to attempt.

Also out of scope, do not build or research: display/I2C driver, Bluetooth, WiFi, BLE MIDI, expression pedal input, runtime-editable cue mappings, preset banks, MIDI channels 11–16.

## Scope discipline

Implement only the task currently in scope. Do not modify, implement, or refactor work belonging to another task, even when related items are visible in the task list. Encountering a defect in earlier work is a note in `PROGRESS.md`, not a fix.

Do not introduce a dependency or an architectural pattern not already in the project. That is a BLOCKER question.

Follow established patterns from tasks already completed in the codebase.

## Verified environment

This toolchain is known-good. Do not change versions or suggest alternatives without asking.

| Component | Version |
|---|---|
| OS | Debian Trixie |
| Pico SDK | 2.3.1 (`~/pico/pico-sdk`, `PICO_SDK_PATH` set) |
| picotool | 2.3.1, built by the SDK into `~/pico/picotool-build` |
| arm-none-eabi-gcc | 14.2.1 |
| clang-format / clang-tidy | 19 (CI pins the same major version) |
| cmake / ninja | 3.31.x / Trixie default |
| Platform | `rp2350-arm-s`, compiler `pico_arm_cortex_m33_gcc` |

**picotool here is built without libusb.** It does ELF-to-UF2 conversion only. `picotool load`, `picotool info`, and `picotool reboot` are unavailable — never suggest them. Flashing is drag-and-drop: hold BOOTSEL while plugging in, the board mounts as `RP2350`, copy the `.uf2` across.

## Commands

**Always use the presets.** `CMakePresets.json` defines `pico2` and `host`; they fix the generator (Ninja) and the cache variables. Invoking `cmake -B build` directly is not equivalent — without `-G Ninja` you get Unix Makefiles and the subsequent `ninja -C build` fails. CI runs exactly these commands, so a command that works here works there.

```bash
# Host tests — the primary feedback signal
cmake --preset host
ninja -C build-host
ctest --test-dir build-host --output-on-failure

# Target build — must compile clean and produce a .uf2
cmake --preset pico2
ninja -C build

# Style and static analysis, changed files only
clang-format --dry-run --Werror <files>
clang-tidy -p build-host <files>
```

A build directory remembers the generator and cache variables from when it was configured. If a preset refuses to configure an existing directory, delete it (`rm -rf build` or `rm -rf build-host`) and reconfigure — do not work around it by dropping the preset.

`PICO_BOARD` is set by the `pico2` preset; the picotool fetch path is set inside `CMakeLists.txt`. No extra flags are ever needed.

**If a documented command here does not work, that is a BLOCKER question, not something to route around.** These are also the commands CI runs; silently using a different invocation makes local green and CI red diverge.

`clang-format` and `clang-tidy` read `.clang-format` and `.clang-tidy` at the repo root. **Never edit, regenerate, or override these files.** If a check fires, fix the code — suppressing the check is a BLOCKER question, not a decision to make alone.

`clang-tidy` requires `compile_commands.json`, which the `host` preset generates via `CMAKE_EXPORT_COMPILE_COMMANDS=ON`. Always pass `-p build-host`; the ARM compilation database from the `pico2` build will fail to resolve host headers.

There is no package manager and no dependency audit.

**The Pico SDK is NOT a submodule of this repo.** It lives at `~/pico/pico-sdk` (SDK 2.3.1) and is located via the `PICO_SDK_PATH` environment variable, per the T00 entry in `PROGRESS.md`. TinyUSB is a submodule *of the SDK*, pinned by the SDK tag — never modify anything under `$PICO_SDK_PATH`.

Because the SDK is external, `CMakeLists.txt` asserts a minimum `PICO_SDK_VERSION_STRING` so a stale or missing SDK fails loudly at configure time instead of producing a subtly wrong binary. Do not weaken or remove that check.

## Continuous integration

`.github/workflows/ci.yml` runs on every branch. Two jobs: **Host tests** (configure, build, `ctest`, `clang-format`, `clang-tidy`) and **Firmware build (pico2)** (cross-compile, confirm a `.uf2` is produced).

CI is an independent check on self-reported results, run on a clean checkout with only committed files present. It catches the case where something works locally because of an uncommitted or ignored file.

Keep the workflow and the commands above in agreement. Changing how the project builds means updating `CMakePresets.json`, this file, and `ci.yml` together — leaving them to drift produces failures that look like code defects and are not.

## Layer rules (PRD §12.1)

- `sequencer` calls no GPIO, MIDI, or flash function directly.
- Lockout and chord detection live in `input`, not `sequencer`.
- `config_store` is called only at boot and on setup exit.
- All timing constants live in `config.h`. No magic numbers elsewhere.
- No `sleep_ms` in the LED pattern engine — it would stall `tud_task()` and drop USB.
- `buttons_poll_event` is the harness drain for `ButtonEvent`. Consuming that queue without leaving a copy turns every `ButtonEvent` assertion in T04–T07 red, and the failures will look like regressions in finished work rather than a defect in the current task.

## Verification bar

A task is complete only when:

1. Host tests for it are written and passing.
2. The full `ctest` suite is green — no regressions, not just the new tests.
3. The target build compiles clean.
4. `clang-format` and `clang-tidy` are clean on changed files.

Never mark a task complete speculatively or before its tests pass.

**If any part of this bar cannot be run, the task is BLOCKED, not complete.** A missing tool, an unbuildable check, or a command that fails for environmental reasons goes in `docs/QUESTIONS.md` as a BLOCKER and the task is left unfinished. Recording an unmet check honestly in `PROGRESS.md` while still marking the task complete defeats the purpose of the bar — every task after it inherits an unverified foundation.

Tests must use the harness API in `docs/TEST-HARNESS.md` and follow the shape of the three exemplar tests committed in T03. Consistent test structure is what makes a long run reviewable.

Tasks marked `Verify: hardware` cannot be verified in an agent session. Do not attempt them. Do not simulate them. Mark blocked and move on.
