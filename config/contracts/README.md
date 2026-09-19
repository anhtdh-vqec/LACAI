# Contract data

Machine-readable version 1 integration and metadata/query catalogs with conformance cases.

- **Status:** source-delivered — integration registry is accepted; metadata revision 2 passes
  structural checks while the P2 storage/query architecture is reopened
- **Naming registry:** `tcont` (`icchk`, `mdqck` checker owners)
- **Depends on:** canonical version registry and scoped contract documents
- **Used by:** three-team handoff review and target/model conformance reports

## Responsibility

- Store stable C01–C10 ownership/producer authority and S01–S18 product identities as reviewable data.
- Map D01–D18 and Q01–Q30 across all S01–S18 security usecases and future traffic profiles.
- Provide valid/rejected acceptance cases without embedding secrets, models or private SDK data.
- Remain deployment-neutral; target values belong in signed/approved conformance receipts.

## Contents

| Path | Purpose |
|---|---|
| `integration_contract_registry.json` | Normative owners, limits, semantics and usecase dependencies |
| `integration_contract_cases.json` | One valid and one rejected baseline case per contract |
| `metadata_query_catalog.json` | Record, query, usecase and traffic-extension authority; its legacy `sensitivity` field names access domains, not at-rest sensitivity |
| `metadata_query_cases.json` | Required outcome-class matrix for every Q01–Q30 identity |

## Limits and next work

- The registry defines compatibility requirements; it does not attest another team's target or
  model implementation.
- Product receipts are external controlled artifacts and must not contain credentials, model
  binaries, biometric data or private SDK libraries in Git.
- Metadata outcome rows are contract coverage, not product/model-quality golden data.
- Metadata revision 2 classifies identity, embedding-derived association and plate records as
  ordinary durable metadata. Access domains and export entitlement remain mandatory.
- Receipts use `config/schemas/integration_contract_receipt.schema.json`; producers leave the
  AI APP-owned consumer disposition at `pending` when submitting a handoff.

## See also

- [Registry contract](../../docs/contracts/integration_contract_registry.md)
- [Schemas](../schemas/README.md)
