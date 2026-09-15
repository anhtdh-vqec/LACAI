# Documentation style

Every `README.md` in this repository follows one structure so the tree stays readable on
GitHub and honest about delivery state. Prose may be Vietnamese or English; headings and
keys stay English and consistent.

## README template

```markdown
# <Title>

<One short paragraph: what this area owns and why it exists.>

- **Status:** <delivered / partial / planned> — <evidence, one line>
- **Naming registry:** `<dir_id>` (<file_ids in use>)
- **Depends on:** <layers>
- **Used by:** <layers>

## Responsibility
<Bullets: what this area does and must not do.>

## Contents
| Path | Purpose |
|---|---|
| `...` | ... |

## Limits and next work
<Bullets: what is missing, blocked or measured-only.>

## See also
- [<Doc>](<relative path>)
```

A placeholder area keeps the same keys but a shorter body; it must still state the owner
`dir_id`, the dependency and the document that defines it. Never describe placeholder or
unbuilt source as a completed module.

## Rules

- One `#` title per file; do not skip heading levels.
- Relative links only; link to the owning contract or architecture document.
- Fenced code blocks with a language for every command.
- State delivery explicitly; a green test does not imply board or model acceptance.
- No secrets, credentials, model binaries or private SDK paths in prose.
- Keep lines within 100 columns where practical.
- Update the README in the same change as the source or contract it describes.

## Authority and status (2026-09-15 normalization)

1. User-approved project rules govern workflow. AGENTS.md records persistent constraints.
2. system_architecture.md defines current layer/ownership direction. Contracts define exact
   boundaries; a proposal is not a released protocol. Changing boundaries requires review.
3. ADRs record decisions and explicit supersession; ADR 0003 extends 0002, ADR 0004 governs FR.
4. implementation_status.md and capability_matrix.md own current delivery/evidence claims.
5. Architecture/module docs describe their scoped mechanism. Plans describe unfinished work.
6. Dated testing/research/review reports retain historical evidence; newer source does not
   retroactively change the test count or qualification of an earlier run.

Use: planned, source-delivered, logic-tested (eSDK/QEMU), board-smoke (specific native test),
accepted (specified workload/contract with review). Never equate these levels. Compatibility
FW runs are not released-FW acceptance. Pure helper limitations apply to that helper, not
necessarily to the whole product. No blanket label can replace a checked source finding.

The complete docs inventory lives in ../README.md. All document changes check links and
state whether code/board tests were run. Source changes update owners/status once, link
other documents to them, and avoid appending a contradictory current-state paragraph.
