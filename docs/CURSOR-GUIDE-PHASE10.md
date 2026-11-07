# Running Phase 10 (BLE MIDI) in Cursor

This is the run order for Phase 10, T30–T39. It follows the same method as v1 and Phase 9: the human leads the bootstrap tasks, the loop runs the host-testable middle, and hardware stays with the human.

| Step | Who | Tasks | Where |
|---|---|---|---|
| 0 | You | Checks before writing any code | Bench, Mac, iPad |
| 1 | You | Merge the Phase 10 docs | Git |
| 2 | You + Cursor agent | T30, T31, T32 (bootstrap) | Normal Agent chat, one per task |
| 3 | Cursor `/loop` | T33–T37 | Loop |
| 4 | You + Cursor agent | T38 (the BTstack transport) | Normal Agent chat |
| 5 | You | T39 (bench validation) | Bench |
| 6 | You | Review and merge | GitHub |

---

## Step 0 — Before writing any code (about an hour)

These are the checks that can kill or reshape the phase. Doing them first is cheaper than finding out at T39.

1. **Playback over BLE.** Use any BLE MIDI controller you can borrow.
   - **Mac:** open Audio MIDI Setup, then Window → Show MIDI Studio, then the Bluetooth button. Pair the device, then confirm Playback triggers a cue from it.
   - **iPad:** connect through your middleware app, then confirm Playback triggers a cue. Write down which app you used and whether it reconnects by itself after a power cycle. Criterion 106 asks for exactly this.
2. **Range in the sealed box.**
   - Build any BTstack peripheral example from `pico-examples/pico_w/bt/` for `pico2_w`, flash it to the Pico 2 W, and close it inside the Hammond box.
   - Stand on the box and read RSSI with nRF Connect or LightBlue on a phone at 3 m, 5 m and 10 m.
   - This is an early look at criterion 110 and needs none of the project's code. If the signal is unusable, settle the shielding question before you order the cape (PRD §11.6).
3. **SDK submodules.** The radio needs two SDK submodules that a USB-only install may never have fetched:
   ```bash
   ls $PICO_SDK_PATH/lib/btstack/src/bluetooth.h
   ls $PICO_SDK_PATH/lib/cyw43-driver/src/cyw43.h
   ```
   If either file is missing, initialise the submodule yourself. AGENTS.md forbids agents from changing anything under `$PICO_SDK_PATH`, and that rule should stay.
   ```bash
   git -C $PICO_SDK_PATH submodule update --init lib/btstack lib/cyw43-driver
   ```
4. **CI needs the same submodules.** If `ci.yml` checks out the SDK with a shallow or partial submodule set, the `pico2_w` job added in T30 will fail. Fixing that belongs to T30, but it helps to know in advance.

## Step 1 — Put the docs in the repo

Copy the updated files over the repo's copies:

| File from this folder | Repo path |
|---|---|
| `AGENTS.md` | `AGENTS.md` (root) |
| `docs/PRD.md` | `docs/PRD.md` |
| `docs/TASK-LIST.md` | `docs/TASK-LIST.md` |
| `docs/QUESTIONS.md` | `docs/QUESTIONS.md` |
| `docs/TEST-HARNESS.md` | `docs/TEST-HARNESS.md` |
| `docs/LOOP-PROMPT.md` | `docs/LOOP-PROMPT.md` |
| `docs/PROGRESS.md` | `docs/PROGRESS.md` |
| `docs/VALIDATION.md` | `docs/VALIDATION.md` |
| `docs/HANDOFF.md` | `docs/HANDOFF.md` |
| `docs/CURSOR-GUIDE-PHASE10.md` | `docs/CURSOR-GUIDE-PHASE10.md` |

These files were edited from the copies in the project folder, which match `master` at `796ff88`. **If the repo's copies have changed since then, diff them before overwriting.** `PROGRESS.md` matters most, because it is append-only.

```bash
git checkout master && git pull
git checkout -b docs/phase10-ble-midi
# copy the files in
git diff --stat            # expect the ten files above only
git add -A
git commit -m "docs: scope Phase 10 BLE MIDI (T30-T39)"
git push -u origin docs/phase10-ble-midi
```

Open a PR and **merge it before starting T30**. T30 branches from `master`, and the loop reads the docs from the branch it is on.

## Step 2 — Bootstrap: T30, T31, T32 (interactive)

Use Cursor's **Agent** mode in a normal chat, **not `/loop`**. Start a fresh chat for each task, pick the strongest model you have, and make sure the context7 MCP is enabled. Cursor reads `AGENTS.md` by itself; the prompts below also attach it to be safe.

The agent writes the code. Your job is to answer its questions and personally check the items listed under each task. Do not let it mark a task complete until you have checked them.

### T30 — pico2w-board-target

Paste:

> Implement **T30** from `@docs/TASK-LIST.md`, interactively with me. Read the Phase 10 scope boundary and T30's Done-when in full first, then `@docs/PRD.md` §4, §11.1, §11.2, §12 and §12.2, and `@AGENTS.md`. Use context7 for the Pico SDK, but treat `$PICO_SDK_PATH/lib/cyw43-driver` headers as authoritative.
>
> Before writing code: confirm the BTstack and cyw43-driver submodules exist (T30's Done-when). Then tell me your plan for how `CMakeLists.txt` should handle `PICO_BOARD`, since T01 pinned `set(PICO_BOARD pico2)` inside it. Wait for my OK before changing that.
>
> Constraints: do not touch `ui_lamp()`, its engine, or its tests. On `pico2_w`, nothing may configure or write GP23, GP24, GP25 or GP29. Both `pico2` and `pico2_w` must build. Branch `task/T30-pico2w-board-target` from `master`, one commit with the exact message from the task list, push, and open a draft PR. Log the entry in `@docs/PROGRESS.md` including the grep evidence. Ask me whenever a decision isn't covered by the docs.

**Check yourself:**
- `cmake --preset pico2_w` reports board `pico2_w`, and `cmake --preset pico2` still reports `pico2`.
- Flash `build-w/*.uf2` to a Pico 2 W. It should enumerate as `TrackStomp Navigator` with the `Navigator Cues` port, and the onboard LED should follow Setup Mode entry, the channel blink and exit.
- The new CI job is green on the PR.

### T31 — flash-map

Paste:

> Implement **T31** from `@docs/TASK-LIST.md`, interactively with me. Read `@docs/PRD.md` §10.1, §10.3 and §13, and `@docs/QUESTIONS.md` Q015–Q018, first. Branch `task/T31-flash-map` from `task/T30-pico2w-board-target`.
>
> Before writing code: (1) show me exactly how `config_store` computes its flash address today, and how `pico_override_flash_size` is used; (2) find and quote the RP2350-E10 erratum from the RP2350 datasheet, and tell me whether a single-family UF2 drag-and-drop can write the last flash sector. If you cannot find the datasheet text, say so. Do not paraphrase from memory. Stop there until I confirm.
>
> Then implement the §10.1 map with the static assertions in T31's Done-when. The config sector moves to 0x3FC000 and the last sector stays empty (PRD §10.1, §15 row 32). Record in PROGRESS.md the highest loaded address of both ELFs and that Q016's flash-size-derived addressing is superseded.

**Check yourself:**
- Read the E10 text the agent quotes. It confirms why the last sector stays empty: the SDK's `--abs-block` makes an A2 bootrom erase it on every drag-and-drop.
- Run criterion 114 on both boards with the **T31** build:
  1. Set channel 8 in Setup Mode and exit with 6+9. The pedal must keep playing, with cues on channel 8. Before T31 it froze here.
  2. Power-cycle. It should boot on channel 8.
  3. Reflash the same or a later build. It should still be on channel 8.
  4. Change the channel again and let the 30 s timeout exit. It should not freeze.
- The full `ctest` suite is still green, including the T18 and T19 config tests, unchanged.

### T32 — link-harness

Paste:

> Implement **T32** from `@docs/TASK-LIST.md`, interactively with me. This is the harness the loop will rely on for T33–T37, so read the whole "Link and transport (Phase 10)" section of `@docs/TEST-HARNESS.md`, plus `@docs/PRD.md` §7.5, §8.5, §9.4 and §12.1, and `@docs/QUESTIONS.md` Q032. Branch `task/T32-link-harness` from `task/T31-flash-map`.
>
> Propose the final API first: any names or signatures you would change from the planning draft, and why. Wait for my OK. Then implement the stubbed `link_state` and `transport`, the fake transport in the harness, the matchers, and the `tests/test_link.cpp` exemplar, and update TEST-HARNESS.md to match exactly what you built.
>
> Before closing the task, run the negative control. Write one test asserting a deliberately wrong `LinkRequest` list and one asserting a deliberately wrong `TransportSend`. Show me both failing with file:line, then correct them. The PROGRESS.md entry must say in words that the matchers were proven to fail. `harness_lamp_trace()`, `harness_switch_trace()` and all existing matchers stay unchanged, and the full suite stays green.

**Check yourself:**
- You saw both negative-control failures with `file:line`.
- `docs/TEST-HARNESS.md` matches the code.
- `PROGRESS.md` has the T30, T31 and T32 entries, and the T32 entry contains the "proven to fail" statement. The loop checks for it.

**Merge T30–T32 to `master` before starting the loop** if you can. Otherwise the loop branches T33 from T32's branch, which also works because the prompt checks with `merge-base`.

## Step 3 — Run the loop: T33–T37

1. Open a new Cursor chat in Agent mode.
2. Type `/loop`, then paste **everything between the first `---` and the `## Appendix` heading** of `docs/LOOP-PROMPT.md`. Use no interval.
3. The first thing it does is check the precondition. If it writes `LOOP-STATUS: HALTED — Phase 10 bootstrap incomplete`, read which check failed in its reply and fix that before running it again.

**While it runs:**
- Check `docs/QUESTIONS.md` now and then. Answer any `[OPEN]` entry on its `ANSWER:` line and leave everything else alone. The loop picks the answer up on its next wake.
- Skim new `[ASSUMED]` entries. Silence counts as agreement.
- **Watch T35 and T37 most closely.**
  - T35 is where the 2+4 chord could leak into performance mode. Criterion 86 is the guard: Select (note 28) must still fire on button 2.
  - T37 is where the two-LED ceiling (criterion 78) and the boot-flash ordering can break.

**It should halt with:** `HALTED — T33–T37 complete …`. Each task arrives as a stacked draft PR against its predecessor. Review them in order.

## Step 4 — T38 ble-midi-transport (interactive)

This task implements the security posture, and it can only be proven with a radio, so do it with the agent in a normal chat.

Paste:

> Implement **T38** from `@docs/TASK-LIST.md`, interactively with me. Read `@docs/PRD.md` §5.4, §9.3, §10.1, §10.3 and §12.2 in full, and `@docs/QUESTIONS.md` Q029–Q030. BTstack headers under `$PICO_SDK_PATH/lib/btstack` are authoritative; check every API against them rather than recalled signatures. Branch `task/T38-ble-midi-transport` from `task/T37-switch-led-link-indications`.
>
> Before writing code, show me: the `.gatt` file you intend to use; the Security Manager calls you will make to confirm Just Works inside the window and decline outside it; how bonds are cleared; and which BTstack events map to each `LinkEventKind`. Wait for my OK.
>
> Hard rules: only GAP, GATT and BLE-MIDI in the GATT database, with a build-time check that fails if anything else appears. The MIDI characteristic requires encryption. One bond. No BTstack logging outside `DEBUG_UART`. `ble_midi.cpp` is the only file that includes BTstack. Also add the instrumented flash-write counter for criterion 112.

**Smoke test before T39:**
1. Flash the build. The pedal should not advertise, so nRF Connect sees nothing.
2. Enter Setup Mode (hold 6+9 for 5 s), then hold 2+4 for 2 s. LEDs 2 and 4 should slow-flash.
3. Pair from the Mac in Audio MIDI Setup. LEDs 2 and 4 should fast-flash and then give one long flash.
4. Exit Setup Mode with a 6+9 tap. Unplug USB, keep 9 V or a USB charger on it, and confirm cues arrive over BLE in a MIDI monitor or in Playback.
5. Power-cycle. The Mac should reconnect, and LEDs 2 and 4 should flash once after the boot channel indication.
6. Try to pair from a phone outside the window. It should be refused.

Record the results in the T38 entry in `PROGRESS.md`.

## Step 5 — T39 bench validation

Fill in the T39 block in `docs/VALIDATION.md`, criteria 102–114:
- **Rebuild both build directories and reflash first.** A stale `.uf2` has already cost a session once.
- Record the `git describe` and SHA-256 of the `pico2_w` `.uf2`.
- Do criterion 110 (range in the sealed box) **before** deciding anything about shielding.
- Record the connection interval each host actually granted beside criterion 108. Mac and iPad will likely differ.
- This can run in the same post-assembly round as T22, T23 and T29.

## Step 6 — Merge

Merge the stack in order, T30 through T39, when you are satisfied. Tagging is your call. Once the Pico 2 W is the only board, retiring the `pico2` build is a small follow-up task. PRD §16 lists it under Deferred.

---

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `pico2_w` configure fails on BTstack or cyw43 | The SDK submodules are missing (Step 0.3), or CI's SDK checkout is shallow |
| `pico2_w` build reports board `pico2` | `CMakeLists.txt` still forces `PICO_BOARD` (T30's first question) |
| Pico 2 W hangs or the radio never starts | Something still drives GP25, or `cyw43_arch_init()` failed. Check with a `DEBUG_UART` build |
| Pedal freezes on setup exit after a channel change | `pico_override_flash_size` is back, or `PICO_FLASH_SIZE_BYTES` is below 4 MB (PRD §10.1) |
| Channel lost after flashing a later build | The config sector moved. T31 regression; criterion 114 |
| Mac shows the old name or won't reconnect after re-pairing | Clear the stale entry in Audio MIDI Setup and use "Forget" in Bluetooth settings |
| iPad pairs, then can't reconnect | The pedal's bond was cleared by a later 2+4 chord. Forget the device on the iPad and re-pair inside a window |
| A phone can't pair at all | Working as designed outside the pairing window (PRD §5.4) |
| Every cue fires twice in Playback | Both transports are sending. Criterion 97 has regressed; check T34 |
| Loop halts immediately | The precondition failed. Usually the T32 "proven to fail" statement is missing from `PROGRESS.md` |
