# Third-party dependencies

This directory holds everything LACAI needs locally. Only the first two entries are
tracked by this repository; the QAIRT SDK is an external, private artifact and is never
committed (AGENTS rule 10).

## nlohmann/json (committed, MIT)

Pinned single header used by the optional JSON loaders. See
[nlohmann/README.md](nlohmann/README.md) for version, hash and provenance.

## qai_appbuilder (git submodule, BSD-3-Clause)

Reference implementation of the Qualcomm QAI AppBuilder, pinned as a git submodule.

- Upstream: https://github.com/qualcomm/qai-appbuilder
- Recorded revision: see the submodule commit in `git submodule status` (main branch).
- License: BSD-3-Clause (`LICENSE` inside the submodule).
- Fetch/update: `git submodule update --init --depth 1 third_party/qai_appbuilder`.

LACAI does not link or copy QAI by default. [ADR 0003](../docs/adr/0003_owned_qnn_engine.md)
selects a LACAI-owned QNN engine (`src/adapters/qualcomm/`) behind the neutral
inference-graph port; QAI is inspected as the mechanism reference for float<->native
conversion, HTP core affinity, async execution, shared/registered memory and LoRA.
Any code reuse needs a per-file license/provenance review first.

## qairt (symlink, private SDK, NOT committed)

`third_party/qairt` is a symlink to the installed Qualcomm QAIRT SDK
(`/opt/qcom/aistack/qairt/2.43.0.260128`). It provides QNN headers, aarch64
`aarch64-oe-linux-gcc11.2` libraries, hexagon-v68 skels and tools used by the Qualcomm
adapter. It is listed in `.gitignore`; do not commit SDK binaries, headers or model
artifacts. A build host must place the approved SDK at that path before enabling the
Qualcomm adapter.
