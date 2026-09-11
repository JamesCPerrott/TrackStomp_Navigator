# Loop prompt

Paste the block below after `/loop` in Cursor. No interval — let the agent decide when to wake.

---

Work through `@docs/TASK-LIST.md` one task at a time, in ID order.

Reference `@docs/PRD.md` for all behavioural, timing, and architecture decisions. Reference `@AGENTS.md` for project rules and commands. Reference `@docs/TEST-HARNESS.md` for the test API. Use `/context7-mcp` for Pico SDK, TinyUSB, and CMake documentation.

## Precondition — check this first, every time

Do not begin work unless **T01, T02, and T03 are all marked complete** in `docs/TASK-LIST.md`, `docs/TEST-HARNESS.md` exists, and `ctest` is green on the existing suite.

If any of those is false, halt immediately with `LOOP-STATUS: HALTED — bootstrap incomplete` and do nothing else. Those three tasks are human-led by design (see the Bootstrap section of the task list). Building the test harness unattended means everything after it is written blind.

## Each iteration

1. Read `docs/PROGRESS.md` to find the last completed task.
2. Read `docs/QUESTIONS.md`. Any entry whose `ANSWER:` field is now filled in becomes the governing decision — apply it, mark that entry `[RESOLVED]`, and unblock its task.
3. Select the lowest-numbered task whose dependencies are met and which is not `[BLOCKED]`.
4. Implement it. Follow the patterns established by tasks already completed. Write tests using the API documented in `docs/TEST-HARNESS.md` and imitate the three exemplar tests from T03 — do not invent a new test style.
5. Verify per the bar in `AGENTS.md`.
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

**ASSUMPTION** — pick the most reasonable default, write it to `docs/QUESTIONS.md` as an `[ASSUMED]` entry with your reasoning, and keep going. Use this for anything local, reversible, and confined to one task: naming, file layout within a module, test structure, error message wording.

Before writing any question, check PRD §15. Most of them are already answered there with reasoning.

When in doubt between the two, prefer ASSUMPTION and flag it. A documented wrong guess is cheap to reverse; a stalled loop wastes the whole window.

## Scope

Implement only the task currently in scope. Do not modify, implement, or refactor tasks belonging to other IDs, even where you see related items. If you find a defect in earlier work, note it in `PROGRESS.md` and continue — do not fix it outside its own task.

## Branching

Dependencies are linear, so branches stack. Branch each task from its **predecessor's branch**, not from `master`.

**T04 is the exception: branch it from `master`.** T01–T03 are merged and `master` also carries `.github/workflows/ci.yml`, which T03's branch lacks. From T05 onward, stack normally:

```
git checkout task/T04-debounce          # the predecessor
git checkout -b task/T05-tap-hold-classification
# ...work...
git add -A
git commit -m "T05: implement tap and hold classification with per-button hold capability"
git push -u origin task/T05-tap-hold-classification
```

Use the exact branch name and commit message given in the task list. One task, one branch, one commit — squash work-in-progress before pushing. Never merge to `master`; that is a human decision.

For T01, branch from `master`.

## Testing

- Write host unit tests alongside each feature as it is implemented.
- The full `ctest` suite must be green before moving to the next task — not just the new tests.
- The target build must compile clean.
- `clang-format` and `clang-tidy` clean on changed files.
- Where the task list names specific acceptance criteria as the ones that catch subtle bugs, write those tests **first**.

## Completion

Mark a task complete in `docs/TASK-LIST.md` only after its tests pass. Never speculatively.

Append to `docs/PROGRESS.md` after every task: task ID, what was done, what was tried and abandoned, anything that contradicted the PRD, and any question raised.

## Halt

Stop the loop and write `LOOP-STATUS: HALTED — <reason>` at the top of `PROGRESS.md` when any of these is true:

- **T01–T21 are all complete** and `ctest` is green. This is success. T22–T23 are human hardware tasks.
- **Every remaining task is `[BLOCKED]`** on an open question.
- **The same test has failed on three consecutive iterations.** Do not keep retrying — write what you tried to `PROGRESS.md` and raise a BLOCKER.
- You are about to do anything in the Hard Stops section of `AGENTS.md`.

Do not attempt tasks marked `Verify: hardware`. Report them as blocked-on-hardware. Criteria 1, 2, 3, 36, 37, 57, 58, and 59 are outside the loop's exit condition by design.

Begin with the next incomplete task.
