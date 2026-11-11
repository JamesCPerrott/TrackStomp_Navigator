# Loop prompt — Phase 10 (BLE MIDI)

Paste the block below after `/loop` in Cursor. No interval — let the agent decide when to wake.

The Phase 9 and v1 prompts are summarised at the bottom of this file for reference. Do not paste those.

---

Work through **Phase 10** of `@docs/TASK-LIST.md` one task at a time, in ID order. The loop's tasks are **T33, T34, T35, T36, and T37**. T30, T31, and T32 are human-led bootstrap and are already done. T38 is human-led and T39 is hardware — do not start either.

Reference `@docs/PRD.md` for all behavioural, timing, and architecture decisions — **read §7.5, §8.5, §9.3, §9.4, §11.5.3, and §11.5.8 in full before starting.** Reference `@AGENTS.md` for project rules and commands. Reference `@docs/TEST-HARNESS.md` for the test API, including its Phase 10 section as committed by T32. Use `/context7-mcp` for Pico SDK, TinyUSB, and CMake documentation.

## Precondition — check this first, every time

Do not begin work unless **all** of these hold:

- T30, T31, and T32 are marked complete in `docs/TASK-LIST.md` and logged in `docs/PROGRESS.md`.
- `docs/PROGRESS.md`'s T32 entry records that the link-request and transport-send matchers were **proven to fail** against deliberately wrong expectations before T32 was closed.
- `docs/PROGRESS.md`'s T31 entry records the RP2350-E10 finding and no BLOCKER is open against it.
- `harness_set_usb_mounted`, `harness_link_event`, `REQUIRE_LINK_REQUESTS`, and `REQUIRE_TRANSPORT_SENDS` exist and are documented in `docs/TEST-HARNESS.md`.
- `ctest` is green on the existing suite, including T10–T13 and T26–T27.
- `cmake --preset pico2 && ninja -C build` and `cmake --preset pico2_w && ninja -C build-w` both produce a `.uf2`.

If any is false, halt immediately with `LOOP-STATUS: HALTED — Phase 10 bootstrap incomplete` and do nothing else.

## Standing constraints for this phase

These apply to every commit.

**Everything the loop writes is host-testable.** T33–T37 touch `input`, `sequencer`, `link`, `ui`, `midi/transport`, and `midi/ble_midi_packet`. None of them includes BTstack, `cyw43_*`, or TinyUSB headers. If a task seems to need one, you are in T38's territory — stop and raise a BLOCKER.

**The `UiEvent` contract does not change.** No new `UiEventKind`, no new field. `ui` reads Bluetooth state by polling `link_state` accessors (QUESTIONS Q032). Proposing a `UiEvent` change is a BLOCKER.

**`ButtonEventKind::PairChordHold` is the only new `ButtonEvent` value**, and it is emitted only while setup is active. If any T04–T09 test changes outcome, the pair chord has leaked into performance input — stop.

**The §11.4 panel engine and its tests are finished work.** Criteria 41–56 must pass at every commit. Bluetooth indications go on the per-switch LEDs only.

**One indication at a time.** The pairing indications are new rows in the §11.5.3 priority stack, not a parallel engine. Every test in T37 asserts `harness_switch_led_count() <= 2`.

**USB wins, per cue, and never both.** Every T34+ test that sends a cue asserts `REQUIRE_TRANSPORT_SENDS` as well as `REQUIRE_COMMANDS`.

**Both firmware targets build.** `pico2` and `pico2_w` must both compile clean at every commit. On `pico2` the link layer reports "no radio".

## Each iteration

1. Read `docs/PROGRESS.md` to find the last completed task.
2. Read `docs/QUESTIONS.md`. Any entry whose `ANSWER:` field is now filled in becomes the governing decision — apply it, mark that entry `[RESOLVED]`, and unblock its task. **Q027–Q032 are the Phase 10 planning assumptions; read them before raising anything new.** New questions start at Q034.
3. Select the lowest-numbered task whose dependencies are met and which is not `[BLOCKED]`.
4. Implement it. Follow the patterns established by tasks already completed. Write tests using the API in `docs/TEST-HARNESS.md`, imitating `tests/test_link.cpp` (T32) for link and transport tests and the existing exemplars for everything else. Do not invent a new test style.
5. Verify per the bar in `AGENTS.md`, **including confirming 41–56 still pass and both firmware targets build.**
6. Commit, push, update `PROGRESS.md`.
7. Continue to the next task, or halt per the conditions below.

## Questions

There is no one to answer you in real time. Do not stop and wait. Classify and route:

**BLOCKER** — write to `docs/QUESTIONS.md`, mark the task `[BLOCKED]`, and move to the next unblocked task. Use this only for:

- A new dependency or architectural pattern not already in the project or approved by the Phase 10 scope boundary.
- Anything touching PRD §5.2, OTP fuses, or the bootloader.
- Anything that would weaken PRD §5.4: another GATT service, pairing outside the window, more than one bond, an over-the-air console.
- A genuine self-contradiction in the PRD.
- Changing a `config.h` value the PRD specifies explicitly.
- Something you cannot write a host test for that is marked `Verify: host`.
- **Any change to `UiEvent`, `ui_lamp()`, `harness_lamp_trace()`, `harness_switch_trace()`, or the flash map.**

**ASSUMPTION** — pick the most reasonable default, write it to `docs/QUESTIONS.md` as an `[ASSUMED]` entry with your reasoning, and keep going. Use this for anything local, reversible, and confined to one task.

Before writing any question, check PRD §15 (rows 18–31 are Phase 10) and `docs/QUESTIONS.md` Q027–Q032.

When in doubt between the two, prefer ASSUMPTION and flag it.

## Scope

Implement only the task currently in scope. If you find a defect in earlier work, note it in `PROGRESS.md` and continue — do not fix it outside its own task. Phase 10 adds one input chord, one sequencer sub-state, one pure link module, one transport policy, one encoder, and two LED indications. It does not restructure anything that already works.

## Branching

Dependencies are linear, so branches stack. **T33 branches from T32's branch** unless T32's commit is already reachable from `master` — check with `git merge-base --is-ancestor`, do not guess. From T34 onward, stack normally:

```
git checkout task/T33-ble-midi-packet               # the predecessor
git checkout -b task/T34-transport-arbitration
# ...work...
git add -A
git commit -m "T34: route each cue to exactly one transport with USB taking priority"
git push -u origin task/T34-transport-arbitration
```

Use the exact branch name and commit message from the task list. One task, one branch, one commit. Never merge to `master`. Open a draft PR against the predecessor's branch.

## Testing

- The full `ctest` suite must be green before moving on — not just the new tests.
- Both firmware targets must compile clean. `clang-format` and `clang-tidy` clean on changed files.
- Assert both edges of every transition and bracket every threshold (`− 1` / exact).
- Write these first, in this order, because they catch the subtle defects: **86** (pair chord must not leak into performance), **97** (never both transports), **88** (setup timeout suspended during the window), **100** (two-LED ceiling across pairing states), **95** (boot link flash waits for the boot channel indication).

## Completion

Mark a task complete in `docs/TASK-LIST.md` only after its tests pass. Append to `docs/PROGRESS.md` after every task: task ID, what was done, what was tried and abandoned, anything that contradicted the PRD, any question raised, and **whether criteria 41–56 still pass and both firmware targets build.**

## Halt

Stop and write `LOOP-STATUS: HALTED — <reason>` at the top of `PROGRESS.md` when any of these is true:

- **T33–T37 are all complete**, `ctest` is green covering criteria 84–101, criteria 4–82 still pass, and both firmware targets build. This is success. T38 is human-led; T39 is hardware.
- **Every remaining task is `[BLOCKED]`.**
- **The same test has failed on three consecutive iterations.** Write what you tried to `PROGRESS.md` and raise a BLOCKER.
- **Any test in T04–T13 or T26–T27 goes red.** That is finished work breaking. Stop rather than working around it.
- You are about to do anything in the Hard Stops section of `AGENTS.md`.

Begin with the next incomplete task.

---

## Appendix — Phase 9 loop prompt (historical)

Retained as a summary. It ran T26–T28 and halted with `LOOP-STATUS: HALTED — T26–T28 complete; T29 is hardware`. Its preconditions were T24 and T25 complete with the switch-LED matcher proven to fail; its standing constraints (panel engine untouched, `UiEvent` unchanged, `harness_lamp_trace()` unchanged, one indication at a time, criterion 78 asserted everywhere) all carry forward above.

## Appendix — v1 loop prompt (historical)

Retained for reference. This ran T04–T21 and completed. Do not paste it.

Its preconditions were T01–T03 complete with `docs/TEST-HARNESS.md` in place; its exit condition was T01–T21 complete with `ctest` green over criteria 4–56 minus the hardware-only ones; and T04 branched from `master` because `master` carried `.github/workflows/ci.yml` and T03's branch did not.

The durable parts — BLOCKER versus ASSUMPTION routing, defaulting to ASSUMPTION, checking PRD §15 before raising anything, append-only `PROGRESS.md`, one task per branch per commit, never merging to `master`, and the three-consecutive-failures halt — are all carried forward above unchanged.
