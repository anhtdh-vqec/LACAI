# contract

Cross-module contract-test source for the camera receiver/session/pump, the GStreamer
bridge/graph/submission/tensor path, encoders, perception, feature and output boundaries.

- **Status:** board-smoke — the complete eSDK configuration passes 135/135 CTest entries and
  130/130 cross-built executables pass on QCS6490 `.98` on 2026-09-18
- **Depends on:** neutral ports and the reference backend

## Responsibility

- Exercise boundaries spanning two or more implementation owners.
- Mirror the source owner hierarchy so a failing contract has one obvious review route.
- Keep real BSP/FW acceptance separate from synthetic adapter fixtures.

## Contents

| Path | Purpose |
|---|---|
| `core/` | Neutral processor, decoder and tracker port conformance |
| `application/` | Session, pipeline, factory and runtime-executor composition contracts |
| `runtime/` | Artifact and feature activation/stage boundary contracts |
| `perception/` | Decoder/reader/tracker registry and stage integration contracts |
| `outputs/` | Event/media authorization and delivery boundary contracts |
| `adapters/` | Camera, FW output, Qualcomm and reference adapter contracts |

## Limits and next work

- These fixtures do not prove real FW reader/writer ABI, hardware completion or RTSP/UI compatibility.
- Add FW01–FW09 acceptance coverage from
  [FW release compatibility](../../docs/contracts/fw_release_compatibility.md) during replacement slices.
- Fake sinks and graphs prove wiring and lifecycle only.

## See also

- [Review checklist](../../docs/development/review_checklist.md)
- [Repository source layout](../../docs/development/source_layout.md)
