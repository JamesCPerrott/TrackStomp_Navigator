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

_None yet._

---

## Resolved

_None yet._
