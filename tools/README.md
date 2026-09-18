# Tools

Repository and CI utilities grouped by operational responsibility. Tool filenames remain
stable while their parent directory identifies the owner and execution environment.

- **Status:** source-delivered
- **Naming registry:** `tools` (`chlay`, `mnchk`)

## Contents

### `checks/` — read-only repository validation

| Tool | Purpose |
|---|---|
| `vqec_vision_check_source_layout.ps1` | Filename, quoted-include existence and CMake source checks (Windows) |
| `vqec_vision_check_source_layout.sh` | Portable Linux/CI counterpart with the same read-only checks |
| `vqec_vision_check_docs_layout.sh` | Documentation filename, template and relative-link checks |

### `build/` — reproducible dependency and DSP builds

| Tool | Purpose |
|---|---|
| `vqec_vision_prepare_zvec.sh` | Acquire the checksum-verified pinned Zvec public SDK |
| `vqec_vision_build_dsp_v1.sh` | Generate QAIC v1 skeleton, build the v68 DSP shared object and emit a digest/provenance receipt into an explicit empty directory; uses `--host-compat-lib-dir` or the persistent `$HOME/.local/lib/hexagon-sdk-compat` fallback |

### `board/` — target-only execution and hardware probes

| Tool | Purpose |
|---|---|
| `vqec_vision_board_native_tests.sh` | Reproducible native board run with manifest and Zvec fixtures |
| `vqec_vision_preview_acceptance.sh` | Host capture of `.98` RTSP output, effective packet FPS gate and overlay contact sheet |
| `vqec_vision_dsp_v1_smoke.cpp` | Explicit board probe for v1 capability negotiation and one descriptor-driven dense operation; requires an absolute candidate skeleton directory |
| `vqec_vision_qnn_engine_smoke.cpp` | Board smoke: compose/execute (and `--reload-cycles`) for one model library |
| `vqec_vision_qnn_board_smoke.sh` | Board-side `qnn-net-run` smoke for one model library |
| `vqec_vision_fastcv_affine_smoke.cpp`, `vqec_vision_qtiv_color_smoke.cpp` | FastCV affine / QTI color board probes |

### `diagnostics/` — optional built diagnostics

| Tool | Purpose |
|---|---|
| `vqec_vision_manifest_check.cpp` | Optional model metadata diagnostic executable |
| `vqec_vision_model_runner.cpp` | Explicit preprocess -> QNN -> decoder model runner diagnostic |

### `fixtures/` — FW and control compatibility fixtures

| Tool | Purpose |
|---|---|
| `vqec_vision_fw_camera_sim.py` | Compatibility FW RAW camera simulator over the wire socket |
| `vqec_vision_fw_camera_sim_test.py` | Device-free simulator pool/ACK ownership regression |
| `vqec_vision_ring_rtsp.py` | Mock FW RTSP service reading the v5 encoded ring |
| `vqec_vision_ring_rtsp_test.py` | Synthetic file-backed ring-generation regression |
| `vqec_vision_fr_runtime_dbus_test.py`, `vqec_vision_usecase_runtime_dbus_test.py` | Session-bus integration peers for FR and usecase control |

## Limits and next work

- The structural checker is not an AST checker, dependency validator, compiler, ownership
  test or board test. No AST naming enforcement yet.
- Board programs are cross-built with the approved eSDK; their location does not make a
  host build valid evidence.
- `vqec_vision_qnn_board_smoke.sh` writes only its output directory; it does not modify the repository.
- The Hexagon host-compat fallback contains only the required host runtime compatibility
  library. It is not a vendor SDK copy, target dependency, system package or BSP signing proof.

## See also

- [Code convention](../docs/development/code_convention.md), [review checklist](../docs/development/review_checklist.md)
- [Repository source layout](../docs/development/source_layout.md)
