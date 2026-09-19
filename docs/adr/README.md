# ADR

Architecture decisions with context, alternatives, consequences, owner and status.

- **Status:** index — decisions are recorded per file as they are made
- **Rule:** do not silently change public ABI or ownership policy

## Decisions

| ADR | Title |
|---|---|
| [0001](0001_baseline.md) | Architecture baseline |
| [0002](0002_qualcomm_plugin_backend.md) | Qualcomm plugin backend |
| [0003](0003_owned_qnn_engine.md) | LACAI-owned QNN engine |
| [0004](0004_fr_gallery_and_vector_index.md) | FR gallery and vector index |
| [0005](0005_scalable_model_integration.md) | Scalable model integration (packages, roles, alignment) |
| [0006](0006_unwired_execution_infrastructure.md) | Unwired execution infrastructure in the clean base |
| [0007](0007_versioned_fastrpc_operations.md) | Versioned FastRPC operation protocol (proposed) |
| [0008](0008_transactional_metadata_store.md) | Transactional metadata store baseline |
| [0009](0009_spatiotemporal_metadata_tiering.md) | Spatiotemporal metadata tiering (accepted) |
| [0010](0010_ai_app_manager_lifecycle.md) | AI-owned usecase application lifecycle (proposed) |
| [0011](0011_durable_evidence_transport.md) | Durable evidence transport (proposed) |

New decisions follow the same template; a decision that changes a public boundary must be
approved by the affected owner before implementation.
