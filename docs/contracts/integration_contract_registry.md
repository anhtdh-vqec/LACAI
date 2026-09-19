# Three-team integration contract registry

This document defines the normative v1 ownership and handoff system between BSP+FW,
AI APP and AI Model. It is the contract authority for integration; implementation-specific
documents refine it but cannot silently change its ownership or semantics.

**Status:** accepted — AI APP lead baseline approved on 2026-09-18; producer conformance is a
release gate, not a prerequisite for adopting this registry. **Layer:** contracts.
**Source:** `config/contracts/integration_contract_registry.json`,
`tools/contracts/vqec_vision_check_integration_contract.py`.

## Responsibility

- Give every cross-team schema exactly one owner and every consuming team an explicit duty.
- Pin all LACAI-owned contract versions to the canonical baseline version registry.
- Define bounded transport, clock/units, ownership/completion, retry/idempotency, security,
  error and conformance-case requirements for C01–C10.
- Keep the 18 commercial security usecases stable without pretending that catalog presence
  means source support, model quality or product acceptance.

## Authority order

When statements conflict, use this order:

1. approved product/security requirements and `vqec_vision_version_registry.h`;
2. `integration_contract_registry.json` for owner, ID, version and mandatory handoff fields;
3. the scoped contract named by each registry entry;
4. a target/model conformance receipt tied to exact artifact and test-report digests;
5. implementation notes and examples.

An external FW ABI keeps the FW-owned numeric identity. A vendor ABI keeps its vendor identity.
Neither consumes a LACAI schema version. Every LACAI-owned schema starts at version 1; changing
meaning, required fields, ownership, completion, authorization or stable IDs requires an approved
migration ADR. An additive evidence receipt increments its revision, not the contract version.

## Ownership rule

| Contracts | Schema owner | Mandatory producer | Consumer decision |
|---|---|---|---|
| C01 RAW source, C02 accelerator, C10 deployment | BSP+FW | BSP+FW publishes target facts and evidence | AI APP validates, admits or rejects |
| C03 model integration kit | AI Model | AI Model publishes package, golden and quality report | AI APP validates, integrates or rejects |
| C04 scene/time, C05 control, C06 query, C07 event/evidence, C08 annotation/video, C09 cloud metadata | AI APP | AI APP owns canonical semantics; BSP+FW supplies authoritative external facts/transport | BSP+FW and AI Model conform; they do not fork schemas |

The owner controls schema meaning, not acceptance of another team's claims. BSP+FW alone can
attest allocator/cache/fence/reset and released-FW behavior. AI Model alone can attest model
quality and golden semantics. AI APP alone accepts integration, admission, feature semantics,
authorization and end-to-end runtime behavior.

## Conformance receipt

Every producer handoff is accompanied by an immutable receipt containing:

- `contract_id`, contract version, registry revision and producer team;
- target/model/package identity and SHA-256 digests of artifacts and manifests;
- exact valid/error case report, command, environment and timestamp;
- declared deviations, expiration/compatibility window and owner signature reference;
- consumer result: `accepted`, `rejected` or `accepted_with_deviation` plus stable reason.

A receipt is evidence linkage, not a signature verifier and not runtime authorization. Missing,
expired or digest-mismatched receipts make the capability `incompatible`; they never trigger a
CPU fallback or an implicit grant.

`integration_contract_receipt.schema.json` is the normative envelope. A producer can run the
same checker with one or more `--receipt <path>` arguments before handoff. The checker rejects a
producer not listed in the contract's `producer_teams`, stale registry/version, incomplete case
coverage, malformed digests, invalid expiry, or an inconsistent consumer disposition. Only AI APP
sets `consumer_disposition`; a producer submits it as `pending`.

## Common runtime rules

- Bounds are admission inputs, never suggestions. Oversized messages/rates fail before resource
  acquisition. Backpressure is explicit and bounded.
- Source monotonic time is the ordering clock. UTC is optional mapped metadata with mapping
  revision and uncertainty. Unknown clock/calibration is not zero.
- Producer retains mutable backing storage until the declared completion signal. Timeout,
  disconnect, FD close, local destructor or transport acceptance is never hardware completion.
- At-least-once boundaries carry idempotency and deduplication keys. Broker ACK, event ACK,
  evidence-media receipt and data-lake commit are distinct states.
- Authentication derives from transport/device identity. Caller-provided tenant, customer,
  consumer or subject IDs are not authentication. Authorization is deny-by-default per source,
  usecase, field and output sink.
- `unsupported`, `disabled`, `denied`, `incompatible`, `resource_exhausted`, `coverage_gap`,
  `expired`, `not_observable` and an empty result are distinct outcomes.

## Security usecase catalog

The registry assigns S01–S18 immutable `usecase_id` values and version 1. Each entry declares
model-role, data, query and output dependencies plus truthful capability status. Dependencies are
roles and schema IDs, not library paths or vendor backends. Adding traffic usecases uses new stable
IDs and the same entity/track/attribute/relation/event foundation; it does not reinterpret S01–S18.

`partial` means at least one reusable dependency exists but the complete production feature and
quality gate do not. `unsupported` fails activation with a reason. Catalog presence never means
installed, entitled, compatible, admitted or running.

## Machine conformance

The repository checker validates exact C01–C10 and S01–S18 coverage, unique ownership, baseline
version, bounds, safe completion, retry/security/error fields and one valid plus one rejected case
per contract. It also runs mutation-based negative checks:

```bash
python3 tools/contracts/vqec_vision_check_integration_contract.py \
  --registry config/contracts/integration_contract_registry.json \
  --cases config/contracts/integration_contract_cases.json \
  --schema config/schemas/integration_contract_registry.schema.json \
  --cases-schema config/schemas/integration_contract_cases.schema.json \
  --receipt-schema config/schemas/integration_contract_receipt.schema.json \
  --version-registry include/vqec/vision/ai/contracts/base/vqec_vision_version_registry.h \
  --self-test
```

This closes definition and machine-consistency of the boundary. A team passes a product release
only after its target/model-specific receipt and the scoped hardware/quality tests pass.

## Limits and next work

- BSP+FW target receipts and AI Model quality/golden receipts are not fabricated by AI APP; they
  remain downstream compatibility gates with explicit owners.
- C06/C09 implementation belongs to the metadata/query plan; C07 delivery implementation belongs
  to the event/evidence plan. This registry prevents semantic drift but does not claim those
  modules already exist.
- The current board fixture proves the checker runs on QCS6490, not released-FW conformance.

## See also

- [FW–AI APP integration contract](fw_ai_app_contract.md)
- [AI Model integration package](model_integration.md)
- [FW usecase activation](fw_usecase_control.md)
- [Contract and team scope plan](../planning/architecture_improvement/contract_and_team_scope.md)
