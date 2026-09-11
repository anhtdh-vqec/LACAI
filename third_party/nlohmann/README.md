# Vendored nlohmann/json (single header)

Vendored because the approved eSDK AArch64 sysroot does not provide the
`nlohmann_json` 3.12.0 CMake package. The optional deployment/model/feature JSON
loaders require it; keeping a pinned copy in-tree makes those loaders buildable
without a host-only or download-at-build dependency.

- Upstream: https://github.com/nlohmann/json
- Version: 3.12.0 (single include)
- File: `include/nlohmann/json.hpp`
- Raw source: https://raw.githubusercontent.com/nlohmann/json/v3.12.0/single_include/nlohmann/json.hpp
- SHA-256: `aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63`
- License: MIT (`LICENSE.MIT`); SPDX header retained in the file
- Provenance review: downloaded once from the pinned upstream tag; not modified.

This is the only third-party source exception in this repository. It retains the
upstream filename per code_convention.md section 0. Do not edit the header; update
the version, hash and licence together under review.

CMake prefers an installed `nlohmann_json 3.12.0` package and falls back to the
interface target `nlohmann_json::nlohmann_json` over this header only when the
package is absent.
