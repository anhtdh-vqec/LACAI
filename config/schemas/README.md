# schemas

Configuration and control validation schemas with bounded fields and version/revision semantics.

- **Status:** source-delivered schemas; runtime acceptance is authoritative

## Contents

| Schema | Purpose |
|---|---|
| `deployment.schema.json` | Reviewable companion for the bounded C++ deployment loader |
| `model_catalog.schema.json` | Model-team handoff; C++ loader adds rate/identity/normalization/memory checks |
| `feature_catalog.schema.json` | Usecase integration metadata independent of commercial activation |

## Limits and next work

- Runtime acceptance is authoritative: it also checks cross-field identity uniqueness,
  packed-NV12 allocation size, aggregate memory arithmetic and deployment/model concurrency.
- Authentication, catalog resolution, FW RAW-source resolution and board admission are
  deliberately outside JSON parsing.

## See also

- [Multi-source configuration](../../docs/architecture/multi_source_configuration.md)
- [Model catalog](../../docs/architecture/model_catalog.md), [feature catalog](../../docs/architecture/feature_catalog.md)
