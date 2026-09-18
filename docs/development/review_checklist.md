# PR and release checklist

**Status:** normative — current per-PR and per-boundary checklist.

## Every PR

- [ ] No magic number/string/hardcode: semantic owner, units/provenance, validated
      configuration for deployment policy, documented defaults and boundary tests.

- [ ] Source/header/tools filename starts vqec_vision_; logical function owner unchanged.
- [ ] Run source-layout checker; include/CMake/docs references migrated together.
- [ ] Run docs-layout checker; filenames, title, Status line and links follow documentation_style.md.
- [ ] External wire/ring/D-Bus/executable names preserved; behavior changes reviewed
      against docs/contracts/fw_release_compatibility.md, not inferred from filename rules.

- [ ] Scope/owner clear; read AGENTS + convention + relevant contract.
- [ ] Function names follow the registered dir/file; overrides keep the declaration prefix.
- [ ] Parameters _snake_case; globals/static g_; members snake_case_.
- [ ] No reserved identifiers; exceptions are recorded, no blanket suppression.
- [ ] No vendor/OpenCV leakage; target dependencies do not cycle.
- [ ] API documents ownership/thread/blocking/completion/errors.
- [ ] Validate bounds/overflow/type/version; correct partial-init cleanup.
- [ ] Tests cover happy/error/stop path; golden/replay if semantics change.
- [ ] Docs/registry/manifest updated; report tests not run and why.
- [ ] Small reviewable change; do not mix broad refactor with behavior change.
- [ ] Build/test (when requested) uses the eSDK toolchain at `/home/a/Workspace/eSDK`;
      do not use the host compiler and do not treat a host build as target evidence.
- [ ] Do not write username/password/token or credential URL into the repo, logs, commit
      or remote; push uses the environment's credential helper/SSH agent/secret store.
- [ ] After each complete source step: check `git status`, stage only changes owned by
      this step and create a focused commit. Per current direction, the user pushes;
      push only when asked again, do not treat a local commit as synchronized.
- [ ] Do not stage/commit unrelated changes, secrets, model binaries, biometric data or
      private SDK libraries.

## Buffer/backend PR — lead + platform owner review

- [ ] Acquire sync, CPU cache scope and device completion are distinguished.
- [ ] Timeout/disconnect does not release active readers.
- [ ] FD reuse/cache invalidation/epoch/context lifetime test.
- [ ] Bounded pools/queues, overload policy, no lock across SDK/RPC.
- [ ] Stride/modifier/color/transform/quantization golden.
- [ ] SDK ABI/version checks; no guessed API or blind provider selection.
- [ ] Metrics actual CPU/copies/memory; hardware claims have board evidence.

## Feature/entitlement/output PR

- [ ] Dependencies share only compatible ones; disable does not break other consumers.
- [ ] Unknown/quality/freshness; attribute scopes are checked separately.
- [ ] Revoke during job + queued retry output is correctly blocked.
- [ ] Event dedup/checkpoint; reset/source-gap does not create false alarms/counts.
- [ ] Privacy: no raw image/embedding/token logs; retention policy.
- [ ] Dataset quality and workload report, not just a demo clip.

## Planned P0 CI rollout

PR target: formatter + AST naming + include/dependency rules + eSDK unit/contracts.
Merge: clean cross-build + package contents/dependencies validation.
Nightly target: golden/replay + eSDK ASan/UBSan; TSan when target/toolchain supports it.
Board: SDK smoke + buffer lifetime/perf; prerelease fault/soak/update matrix.
Current source: **unit/contract test source and CTest exist for validators, session,
cadence, ownership and output helpers; eSDK cross-build passes; eSDK/QEMU currently
123/123 and the native binary set on QCS6490 `.98` passes 117/117 via
`tools/board/vqec_vision_board_native_tests.sh` (see `docs/testing/qsc6490_board.md`);
the structural/eSDK workflow exists; runner run is not verified and there is no AST
naming checker**.
.editorconfig/.clang-format is only configuration, it does not enforce every rule by
itself.

## See also

- [Code convention](code_convention.md), [implementation status](implementation_status.md)
