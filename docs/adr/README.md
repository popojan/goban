# Architecture Decision Records

Why the code is the way it is. One file per decision, in the format described by
Michael Nygard in *Documenting Architecture Decisions* (2011).

## Why bother

Twice in one session, work was proposed that had already been decided against —
Sabaki-style genmove (Analysis mode *is* that decision) and removing the
`gameHasNewMoves` guard (it is deliberate). Both times the rationale existed only
in one person's head, and both times it cost a round trip. A decision that is not
written down gets re-litigated.

## Decisions vs. invariants

These are different things and live in different places:

| | Decision | Invariant |
|---|---|---|
| What it is | A choice made once, with alternatives rejected | A property that must always hold |
| Lifecycle | Immutable — **superseded**, never edited | Living; evolves with the code |
| Where | `docs/adr/NNNN-*.md` | CLAUDE.md "Design Invariants", enforced by a test |
| Answers | "Why is it like this?" | "What must never break?" |

An invariant may cite the ADR that produced it. An ADR may list the invariants it
established.

## Rules

- **Append-only.** Never rewrite a decision to match new thinking. Write a new
  ADR that supersedes it, and mark the old one `Superseded by ADR-NNNN`.
- **Record the alternatives you rejected**, and why. That is the part nobody can
  reconstruct later, and the part that stops the same idea coming back.
- **Record the consequences you accepted**, including the bad ones. A decision
  with no downside is usually a decision that was not understood.
- Keep them short. A page is plenty.

## Format

```markdown
# ADR-NNNN: Title

**Status:** Accepted | Superseded by ADR-NNNN | Deprecated
**Date:** YYYY-MM-DD

## Context
What forces are at play? What makes this hard?

## Decision
What we do, stated plainly.

## Consequences
What follows — the good, and the costs accepted.

## Alternatives rejected
Each with the reason it lost.
```

A compact alternative for small decisions is the *Y-statement*:

> In the context of `<use case>`, facing `<concern>`, we decided for `<option>`
> and neglected `<alternatives>`, to achieve `<quality>`, accepting `<downside>`.

## Index

- [ADR-0001: Engine-exclusive UI actions](0001-engine-exclusive-ui-actions.md)
