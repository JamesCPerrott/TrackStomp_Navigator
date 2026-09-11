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

## Resolved

_None yet._
