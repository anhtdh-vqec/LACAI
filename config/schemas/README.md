# schemas

Configuration and control validation schemas with bounded fields, version and
revision semantics.

`deployment.schema.json` is the reviewable JSON Schema companion for the bounded C++
loader. Runtime acceptance remains authoritative because it also checks cross-field
identity uniqueness, packed-NV12 allocation size and aggregate memory arithmetic.
Authentication, model-catalog resolution, FW RAW-source resolution and board admission
are deliberately outside JSON parsing.

`model_catalog.schema.json` describes the separate Model-team handoff. Its bounded C++
loader additionally performs rational-rate, identifier, normalization and memory checks;
deployment cross-validation accounts source compatibility, concurrency and tensor/model
resident budgets. The synthetic example lives under `manifests/models`.

`feature_catalog.schema.json` describes usecase integration metadata independently from
commercial activation. It maps stable feature IDs to processor/configuration contracts,
model roles, attribute freshness requirements and bounded temporal/event resources. The
synthetic example under `manifests/features` enables nothing by itself.
