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
