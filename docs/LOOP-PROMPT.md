# Loop prompt — Phase 9 (per-switch status LEDs)

Paste the block below after `/loop` in Cursor. No interval — let the agent decide when to wake.

The v1 prompt (T04–T21) is preserved at the bottom of this file for reference. Do not paste that one.

---

Work through **Phase 9** of `@docs/TASK-LIST.md` one task at a time, in ID order. Phase 9 is tasks **T26, T27, and T28**. T24 and T25 are human-led bootstrap and are already done; T29 is hardware.

Reference `@docs/PRD.md` for all behavioural, timing, and architecture decisions — **§11.5 in particular, read it in full before starting.** Reference `@AGENTS.md` for project rules and commands. Reference `@docs/TEST-HARNESS.md` for the test API. Use `/context7-mcp` for Pico SDK, TinyUSB, and CMake documentation.

## Precondition — check this first, every time

Do not begin work unless **all** of these hold:

- T24 and T25 are marked complete in `docs/TASK-LIST.md`.
- `ui_switch_leds()`, `harness_switch_trace()`, and `REQUIRE_SWITCH_LEDS` exist and are documented in `docs/TEST-HARNESS.md`.
- `docs/PROGRESS.md`'s T25 entry records that the switch-LED matcher was **proven to fail** against a deliberately wrong mask before T25 was closed.
- `ctest` is green on the existing suite, including the T10–T13 panel-LED tests.

If any is false, halt immediately with `LOOP-STATUS: HALTED — bootstrap incomplete` and do nothing else. T24 and T25 are human-led by design. A per-switch harness whose assertions cannot fail will report green for every task in this phase, which is the T03 failure mode repeating on a new surface.

## Standing constraints for this phase

These are not per-task notes. They apply to every commit.

**The §11.4 panel LED engine is finished work.** Do not modify `ui_lamp()`, its pattern engine, its tests, or its GP16/GP25 driver. The per-switch engine is a *second, independent* engine living beside it inside `ui`. **Criteria 41–56 must pass at every commit**, and confirming that is part of the verification bar, not an afterthought. The temptation to tidy the neighbouring engine while working in the same file is the single most likely way this phase breaks finished work — resist it and note anything you find in `PROGRESS.md` instead.

**The `UiEvent` contract does not change.** No new `UiEventKind`, no new field, no change to what any existing event carries. T08–T13 assert ordered `UiEvent` lists; inserting or altering an event turns them red for reasons unrelated to any defect. If you believe you need a button id that `UiEvent` does not carry, you need T24's reverse lookup, not a contract change. Proposing one is a BLOCKER.

**`harness_lamp_trace()` does not change.** Its signature, its element type, and its one-sample-per-timestamp meaning are relied on by T10–T13. Per-switch capture is a parallel API.

**One indication at a time.** PRD §11.5.3 permits exactly one active indication owning at most two LEDs, with every other bit clear. Do not implement the engine as ten independent per-LED state machines — that shape passes its own unit tests and fails criterion 78.

**Assert criterion 78 everywhere.** The two-LED ceiling is a cross-cutting invariant, not a single behaviour. Every test in T26 and T27 asserts `harness_switch_led_count() <= 2` alongside its own assertions. A test suite that checks it once will not catch the state that violates it.

## Each iteration

1. Read `docs/PROGRESS.md` to find the last completed task.
2. Read `docs/QUESTIONS.md`. Any entry whose `ANSWER:` field is now filled in becomes the governing decision — apply it, mark that entry `[RESOLVED]`, and unblock its task. **Q021–Q025 are the Phase 9 assumptions; read them before raising anything new.** New questions start at Q026.
3. Select the lowest-numbered task whose dependencies are met and which is not `[BLOCKED]`.
4. Implement it. Follow the patterns established by tasks already completed. Write tests using the API documented in `docs/TEST-HARNESS.md`, imitating the three exemplar tests from T03 and the waveform shape shown in the per-switch section — do not invent a new test style.
5. Verify per the bar in `AGENTS.md`, **including confirming 41–56 still pass.**
6. Commit, push, update `PROGRESS.md`.
7. Continue to the next task, or halt per the conditions below.

## Questions

There is no one to answer you in real time. Do not stop and wait. Instead, classify and route:

**BLOCKER** — write to `docs/QUESTIONS.md`, mark the task `[BLOCKED]`, and move to the next unblocked task. Use this only for:

- A new dependency or architectural pattern not already in the project.
- Anything touching PRD §5.2, OTP fuses, or the bootloader.
- A genuine self-contradiction in the PRD.
- Changing a `config.h` value the PRD specifies explicitly.
- Something you cannot write a host test for that is marked `Verify: host`.
- **Any change to `UiEvent`, `ui_lamp()`, or `harness_lamp_trace()`.**

**ASSUMPTION** — pick the most reasonable default, write it to `docs/QUESTIONS.md` as an `[ASSUMED]` entry with your reasoning, and keep going. Use this for anything local, reversible, and confined to one task: naming, file layout within a module, test structure, error message wording.

Before writing any question, check PRD §15 and `docs/QUESTIONS.md` Q021–Q025. Most of them are already answered there with reasoning.

When in doubt between the two, prefer ASSUMPTION and flag it. A documented wrong guess is cheap to reverse; a stalled loop wastes the whole window.

## Scope

Implement only the task currently in scope. Do not modify, implement, or refactor tasks belonging to other IDs, even where you see related items. If you find a defect in earlier work, note it in `PROGRESS.md` and continue — do not fix it outside its own task.

This applies with particular force to v1 modules. Phase 9 adds to `ui` and adds one pure function to the cue table; it does not restructure anything that already works.

## Branching

Dependencies are linear, so branches stack. Branch each task from its **predecessor's branch**.

**T26 is the exception.** Its predecessor T25 was human-led. Before branching, check whether T25's commit is reachable from `master`:

- If it is, branch T26 from `master`.
- If it is not, branch T26 from T25's branch.

Do not guess — run `git merge-base --is-ancestor` and branch accordingly. Getting this wrong leaves the whole phase without the harness it depends on, and the failure will look like missing symbols rather than a branching mistake.

From T27 onward, stack normally:

```
git checkout task/T26-switch-led-engine          # the predecessor
git checkout -b task/T27-switch-led-setup-chord
# ...work...
git add -A
git commit -m "T27: add setup channel, chord progress, and boot indication to the switch LEDs"
git push -u origin task/T27-switch-led-setup-chord
```

Use the exact branch name and commit message given in the task list. One task, one branch, one commit — squash work-in-progress before pushing. Never merge to `master`; that is a human decision.

## Testing

- Write host unit tests alongside each feature as it is implemented.
- The full `ctest` suite must be green before moving to the next task — not just the new tests. **A green new test and a red T11 is a failed task, not a partial success.**
- The target build must compile clean.
- `clang-format` and `clang-tidy` clean on changed files.
- Assert both edges of every transition. A test that only checks the lit state will pass against an engine that never turns the LED off.
- Write these first, in this order, because they are the ones that catch the subtle defects: **78** (two-LED ceiling), **69** (hold envelope never truncates while held), **72** (pending resumes after chord abort), **70** (accepted press cancels a confirmation, lockout-discarded press does not).

## Completion

Mark a task complete in `docs/TASK-LIST.md` only after its tests pass. Never speculatively.

Append to `docs/PROGRESS.md` after every task: task ID, what was done, what was tried and abandoned, anything that contradicted the PRD, and any question raised.

## Halt

Stop the loop and write `LOOP-STATUS: HALTED — <reason>` at the top of `PROGRESS.md` when any of these is true:

- **T26, T27, and T28 are all complete**, `ctest` is green covering criteria 61–82, and 41–56 still pass. This is success. T29 is a human hardware task.
- **Every remaining task is `[BLOCKED]`** on an open question.
- **The same test has failed on three consecutive iterations.** Do not keep retrying — write what you tried to `PROGRESS.md` and raise a BLOCKER.
- **Any test in T10–T13 goes red.** Stop immediately rather than working around it. That is finished work breaking, and continuing past it builds on a broken foundation.
- You are about to do anything in the Hard Stops section of `AGENTS.md`.

Do not attempt tasks marked `Verify: hardware`. Criterion **83** is hardware-only and outside this phase's exit condition by design, as are the v1 hardware criteria 1, 2, 3, 36, 37, 57, 58, and 59.

Begin with the next incomplete task.

---

## Appendix — v1 loop prompt (historical)

Retained for reference. This ran T04–T21 and completed. Do not paste it.

Its preconditions were T01–T03 complete with `docs/TEST-HARNESS.md` in place; its exit condition was T01–T21 complete with `ctest` green over criteria 4–56 minus the hardware-only ones; and T04 branched from `master` because `master` carried `.github/workflows/ci.yml` and T03's branch did not.

The durable parts — BLOCKER versus ASSUMPTION routing, defaulting to ASSUMPTION, checking PRD §15 before raising anything, append-only `PROGRESS.md`, one task per branch per commit, never merging to `master`, and the three-consecutive-failures halt — are all carried forward above unchanged.
