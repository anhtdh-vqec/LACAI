# Documentation style — normative v3

This is the single source of truth for repository documentation. It defines file naming,
language, the required document template and the status vocabulary. A document that does not
follow this file is a defect to fix, not a local exception.

**Status:** normative — current documentation rules.

## 1. Authority and status

1. User-approved project rules govern workflow. AGENTS.md records persistent constraints.
2. system_architecture.md defines current layer/ownership direction. Contracts define exact
   boundaries; a proposal is not a released protocol.
3. ADRs record decisions and explicit supersession in numeric order.
4. implementation_status.md and capability_matrix.md own current delivery/evidence claims.
5. Architecture/module docs describe their scoped mechanism. Plans describe unfinished work.
6. Dated test/research/review records retain historical evidence; newer source does not
   retroactively change an earlier run's numbers.

Status vocabulary (use exactly one, never equate levels):

| Status | Meaning |
|---|---|
| `planned` | contract/design only; no built source |
| `source-delivered` | source exists in the tree |
| `logic-tested` | eSDK cross-build + QEMU/CTest pass for the relevant targets |
| `board-smoke` | a named native/target run passed (state board and date) |
| `accepted` | specified workload/contract with owner review and board evidence |

Compatibility-FW runs are not released-FW acceptance. A pure helper's limitation applies to
that helper, not necessarily to the product.

## 2. File naming

- Markdown filenames are lowercase `snake_case.md`. No uppercase, spaces or hyphens.
- `README.md` is reserved for a directory index and follows the README template below.
- ADRs are `NNNN_<snake_case>.md` under `docs/adr/` with a sequential number.
- One topic per file. Do not create `misc.md`, `notes.md`, `todo.md` or dated copies; use a
  section or supersede the file instead.
- Filename families are semantic, not decorative: `fw_*` for FW boundary contracts,
  `qualcomm_*` for the Qualcomm adapter, `feature_*` for the feature framework, `<name>.md`
  for one module. Do not rename an existing file only to restyle it.
- Renaming a document updates every relative link in the same change; run the link checker.
- A dated record states its date in the title or first line and is never edited to reflect
  newer runs.

## 3. Language

- Titles, section headings, status labels, table headers and keys are **English** for every
  document except `docs/planning/` (see below).
- Body prose is **English** for `docs/architecture/`, `docs/contracts/`, `docs/development/`,
  `docs/testing/`, `docs/research/`, `docs/adr/`, `docs/operations/`, the root `README.md`
  may stay Vietnamese, and all module `README.md` may be English.
- `docs/planning/` (roadmaps, cohort plans) may be written in Vietnamese with Vietnamese
  headings, but must still follow the section skeleton in section 4 and keep the literal
  `Status:` label (English) so the checker can verify it.
- One document is written in one language; do not alternate prose languages within a file.
  Vietnamese planning docs quote English contract/identifier names verbatim.

## 4. Document template

Every non-ADR document uses this skeleton. Omit a section only when it is truly empty, and
never reorder the fixed sections.

```markdown
# <Title>

<Scope: one short paragraph — what this document defines and why it exists.>

**Status:** <vocabulary> — <evidence>. **Layer:** <layer>. **Source:** `<paths or n/a>`.

## Responsibility
<What this area does and must not do. Bullets.>

## <Mechanism / Contract / Rules / Results>
<The scoped content. Use one or more domain sections with concrete English headings.>

## Limits and next work
<What is missing, blocked or measured-only. Bullets.>

## See also
- [<Doc>](<relative path>)
```

Rules:

- Exactly one `#` H1 title; do not skip heading levels.
- The three-line header block (scope, status/layer/source) appears before the first `##`.
- `Status:` uses only the section-1 vocabulary and names its evidence.
- `Layer:` is one of: `contracts`, `core`, `runtime`, `perception`, `features`, `adapters`,
  `outputs`, `app`, `reference`, `docs`.
- `Source:` lists the owning files/directories in backticks, or `n/a` for a contract/idea.
- Relative links only, and they must resolve (see section 6).
- Prefer tables for enumerable facts and fenced code blocks with a language for commands.
- Keep lines within 100 columns where practical.
- No secrets, credentials, personal directory paths, model binaries or private SDK paths.

### ADR exception

ADRs keep their conventional sections instead of the template above:
`# ADR NNNN — <title>`, then `Status:`, `Date:`, `Owner:`, `## Context`, `## Decision`,
`## Alternatives`, `## Consequences`, and optionally `## Approve after (gates)`.

### README template

Module/directory READMEs use:

```markdown
# <Title>

<One short paragraph: what this area owns and why.>

- **Status:** <vocabulary> — <evidence>
- **Layer:** <layer>
- **Naming registry:** `<dir_id>` (<file_ids>)
- **Depends on:** <layers>
- **Used by:** <layers>

## Responsibility
## Contents
| Path | Purpose |
## Limits and next work
## See also
```

A placeholder area keeps the same keys, states its `dir_id`, and never describes unbuilt
source as a completed module.

## 5. Status fidelity

- State delivery explicitly; a green test does not imply board or model acceptance.
- Do not use `accepted` without board evidence and owner review.
- Mark historical numbers with their date; do not silently update another run's count.
- Update the owning document in the same change as the source or contract it describes, and
  keep current delivery claims in `implementation_status.md` / `capability_matrix.md` only.

## 6. Enforcement

- `tools/vqec_vision_check_docs_layout.sh` verifies: lowercase snake_case filenames (with
  `README.md` and `NNNN_slug.md` exceptions), a single H1, a `Status:` line before the first
  `##`, and resolvable relative links. It does not judge prose quality or status truth.
- The checker runs in the `structure` CI job and per the review checklist before a commit.
- A failing checker is fixed, never ignored; a genuine exception is recorded in this file.

## 7. Adding or changing a document

1. Pick the category directory and a snake_case name.
2. Copy the template skeleton for the category.
3. State status with evidence; keep it honest.
4. Link to the owning contract/architecture document.
5. Run the docs checker and the source-layout checker; fix links.
6. Update `docs/README.md` (the documentation map) if a document is added or removed.
