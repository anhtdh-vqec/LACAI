# Integration contract data

Machine-readable v1 registry and conformance cases for the BSP+FW, AI APP and AI Model boundary.

- **Status:** accepted — checked by the repository contract checker and eSDK CTest.
- **Naming registry:** `tcont` (`icchk` checker owner)
- **Depends on:** canonical version registry and scoped contract documents
- **Used by:** three-team handoff review and target/model conformance reports

## Responsibility

- Store stable C01–C10 ownership/producer authority and S01–S18 product identities as reviewable data.
- Provide valid/rejected acceptance cases without embedding secrets, models or private SDK data.
- Remain deployment-neutral; target values belong in signed/approved conformance receipts.

## Contents

| Path | Purpose |
|---|---|
| `integration_contract_registry.json` | Normative owners, limits, semantics and usecase dependencies |
| `integration_contract_cases.json` | One valid and one rejected baseline case per contract |

## Limits and next work

- The registry defines compatibility requirements; it does not attest another team's target or
  model implementation.
- Product receipts are external controlled artifacts and must not contain credentials, model
  binaries, biometric data or private SDK libraries in Git.
- Receipts use `config/schemas/integration_contract_receipt.schema.json`; producers leave the
  AI APP-owned consumer disposition at `pending` when submitting a handoff.

## See also

- [Registry contract](../../docs/contracts/integration_contract_registry.md)
- [Schemas](../schemas/README.md)
