# third_party

External dependencies referenced or vendored by this repository. Only the JSON parser is
available to optional loaders; vendor SDKs stay behind adapters and are never linked into
neutral layers.

| Component | Role | License | Delivery |
|---|---|---|---|
| `qai_appbuilder/` | Qualcomm AI AppBuilder reference for plugin/QNN mechanisms (not linked) | BSD-3-Clause | git submodule, pinned |
| `qairt/` | QAIRT/QNN SDK used to build the optional owned QNN engine | Qualcomm proprietary | local symlink, gitignored |
| `nlohmann/` | Header-only JSON parser used by optional loaders | MIT | vendored single header |

## Rules

- Do not commit model binaries, private SDK libraries, credentials or biometric data.
- Do not copy vendor source without per-file license/provenance review.
- Keep `qairt` a local symlink; CMake defaults `VQEC_VISION_AI_QAIRT_ROOT` to it.
- Bump a submodule pin only with a reviewed, focused commit.

## See also

- [AGENTS.md](../AGENTS.md), [code convention](../docs/development/code_convention.md)
