# core

Pure, header-visible validation and bookkeeping shared by every adapter and application
module. No I/O, no allocation of runtime pools and no vendor, GStreamer or OpenCV types.

- **Status:** board-smoke — eSDK tests and the 2026-09-18 `.98` native candidate run pass
- **Layer:** core
- **Naming registry:** `core` (`dpval`, `infpl`, `subwn`, `srcbd`, `tnctr`, `inexe`, `otgat`, `ftevt`, `ftcat`, `otgen`, `encot`, `pvpol`, `encwn`, `pvsrf`, `pvctr`)
- **Depends on:** `include/vqec/vision/ai/contracts/`
- **Used by:** `src/app/`, `src/outputs/`, adapters through neutral contracts

## Responsibility

- Validate deployment/source/model/feature descriptors and packed NV12 byte counts.
- Own frame-correlated submission bookkeeping, relative PTS mapping and drain/fault state.
- Authorize outputs from revision-aware source/feature/attribute scopes.
- Validate preview overlay, encoded-AU and feature-event envelopes with bounded limits.

## Contents

| Path | Purpose |
|---|---|
| `configuration/` | Deployment and model-catalog validation; no runtime loading or I/O |
| `features/` | Feature catalog, neutral event and effective-usecase validation |
| `inference/` | Model/package/IO, preprocess, binding, execution and submission contracts |
| `media/` | Color, image alignment, preview and encoder value/ownership contracts |
| `memory/` | Bounded reusable memory-pool bookkeeping |
| `output/` | Output authorization and monotonic generation identities |
| `perception/` | Observation, embedding and face-gallery value validation |
| `CMakeLists.txt` | Declares the core target from the ownership subtrees above |

## Limits and next work

- Pool, encoder and submission code owns no image memory and cannot detect hardware completion.
- `output_generation` and `output_gate` are unit-tested; wiring the generation allocator into a running ring supervisor remains pending.
- Trusted grant verification and serialized output dispatch integration stay outside the pure evaluator.
- No model-kit parser, signature verification or decoder is implied.
- Public headers remain under `include/vqec/vision/ai/contracts/`; this source split does not
  change the installed include ABI.

## See also

- [Multi-source configuration](../../docs/architecture/multi_source_configuration.md)
- [Submission window](../../docs/architecture/submission_window.md), [output generation](../../docs/architecture/output_generation.md)
- [Preview pool](../../docs/architecture/preview_pool.md), [encoder window](../../docs/architecture/encoder_window.md)
- [Repository source layout](../../docs/development/source_layout.md)
