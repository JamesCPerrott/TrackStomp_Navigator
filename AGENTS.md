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

## Commands

```bash
# Host tests — the primary feedback signal
cmake -B build-host -DBUILD_HOST_TESTS=ON
ninja -C build-host
ctest --test-dir build-host --output-on-failure

# Target build — must compile clean
cmake -B build -DPICO_BOARD=pico2
ninja -C build

# Style and static analysis, changed files only
clang-format --dry-run --Werror <files>
clang-tidy -p build-host <files>
```

There is no package manager and no dependency audit. The Pico SDK is a pinned submodule — never change the pin.

## Layer rules (PRD §12.1)

- `sequencer` calls no GPIO, MIDI, or flash function directly.
- Lockout and chord detection live in `input`, not `sequencer`.
- `config_store` is called only at boot and on setup exit.
- All timing constants live in `config.h`. No magic numbers elsewhere.
- No `sleep_ms` in the LED pattern engine — it would stall `tud_task()` and drop USB.

## Verification bar

A task is complete only when:

1. Host tests for it are written and passing.
2. The full `ctest` suite is green — no regressions.
3. The target build compiles clean.
4. `clang-format` and `clang-tidy` are clean on changed files.

Never mark a task complete speculatively or before its tests pass.

Tests must use the harness API in `docs/TEST-HARNESS.md` and follow the shape of the three exemplar tests committed in T03. Consistent test structure is what makes a long run reviewable.

Tasks marked `Verify: hardware` cannot be verified in an agent session. Do not attempt them. Do not simulate them. Mark blocked and move on.
