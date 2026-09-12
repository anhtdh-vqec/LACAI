# tools

Repository checks and optional diagnostics. All tools are read-only against source unless a
run explicitly writes an output directory.

- **Status:** source-delivered
- **Naming registry:** `tools` (`chlay`, `mnchk`)

## Contents

| Tool | Purpose |
|---|---|
| `vqec_vision_check_source_layout.ps1` | Filename, quoted-include existence and CMake source checks (Windows) |
| `vqec_vision_check_source_layout.sh` | Portable Linux/CI counterpart with the same read-only checks |
| `vqec_vision_manifest_check.cpp` | Optional model metadata diagnostic executable |
| `vqec_vision_qnn_board_smoke.sh` | Board-side `qnn-net-run` smoke for one model library |

## Limits and next work

- The structural checker is not an AST checker, dependency validator, compiler, ownership
  test or board test. No AST naming enforcement yet.
- `vqec_vision_qnn_board_smoke.sh` writes only its output directory; it does not modify the repository.

## See also

- [Code convention](../docs/development/code_convention.md), [review checklist](../docs/development/review_checklist.md)
