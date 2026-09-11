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

## Resolved

_None yet._
