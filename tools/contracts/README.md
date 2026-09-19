# Contract tools

Read-only validation tools for machine-readable cross-team integration contracts.

- **Status:** logic-tested — included in the eSDK CTest suite.
- **Naming registry:** `tcont` (`icchk`, `mdqck`)
- **Depends on:** Python 3 standard library
- **Used by:** CI, board smoke and producer handoff review

## Responsibility

- Validate registry structure and semantic invariants without downloading dependencies.
- Validate producer receipts against the authoritative producer, revision, digest, case coverage,
  expiry and AI APP disposition rules.
- Read the canonical baseline version from the C header rather than duplicating a version.
- Exercise built-in negative mutations when `--self-test` is selected.
- Validate D01–D18/Q01–Q30, S01–S18 mappings, catalog revision and required query outcomes.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_check_integration_contract.py` | C01–C10/S01–S18 registry, case and optional receipt validator |
| `vqec_vision_check_metadata_query.py` | Metadata/query catalog, integration identity and negative-mutation validator |

## Limits and next work

- This checker is a handoff-definition gate, not a released-FW, hardware-completion or model-
  quality test.
- Passing the metadata checker does not accept the P2 storage architecture or its capacity.

## See also

- [Integration contract registry](../../docs/contracts/integration_contract_registry.md)
- [Review checklist](../../docs/development/review_checklist.md)
