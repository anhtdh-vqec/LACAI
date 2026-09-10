# PR and release checklist

## Mọi PR

- [ ] Source/header/tools filename starts vqec_vision_; logical function owner unchanged.
- [ ] Run source-layout checker; include/CMake/docs references migrated together.
- [ ] External wire/ring/D-Bus/executable names preserved; behavior changes reviewed
      against docs/contracts/fw_release_compatibility.md, not inferred from filename rules.

- [ ] Scope/owner rõ; đọc AGENTS + convention + relevant contract.
- [ ] Tên hàm theo registered dir/file; override giữ declaration prefix.
- [ ] Parameters _snake_case; globals/static g_; members snake_case_.
- [ ] Không reserved identifiers; ngoại lệ có record, không blanket suppression.
- [ ] No vendor/OpenCV leakage; target dependencies không cycle.
- [ ] API ghi ownership/thread/blocking/completion/errors.
- [ ] Validate bounds/overflow/type/version; partial-init cleanup đúng.
- [ ] Tests happy/error/stop path; golden/replay nếu semantics thay đổi.
- [ ] Docs/registry/manifest cập nhật; report tests not run và lý do.
- [ ] Small reviewable change; không mix refactor rộng với behavior change.
- [ ] Build/test (khi được yêu cầu) dùng toolchain eSDK tại `/home/a/Workspace/eSDK`;
      không dùng host compiler và không coi host build là bằng chứng target.
- [ ] Không ghi username/password/token hoặc credential URL vào repo, log, commit hay
      remote; push dùng credential helper/SSH agent/secret store của môi trường.
- [ ] Sau mỗi bước source hoàn chỉnh: kiểm tra `git status`, chỉ stage thay đổi thuộc
      bước này, tạo focused commit và push lên tracking upstream (`@{upstream}`).
      Nếu thiếu upstream hoặc push lỗi, ghi rõ blocker và chưa coi bước đã đồng bộ.
- [ ] Không stage/commit thay đổi không thuộc task, secrets, model binaries, biometric
      data hoặc private SDK libraries.

## Buffer/backend PR — lead + platform owner review

- [ ] Acquire sync, CPU cache scope và device completion được phân biệt.
- [ ] Timeout/disconnect không release active readers.
- [ ] FD reuse/cache invalidation/epoch/context lifetime test.
- [ ] Bounded pools/queues, overload policy, no lock across SDK/RPC.
- [ ] Stride/modifier/color/transform/quantization golden.
- [ ] SDK ABI/version checks; no guessed API or blind provider selection.
- [ ] Metrics actual CPU/copies/memory; hardware claim có board evidence.

## Feature/entitlement/output PR

- [ ] Dependencies share compatible only; disable không phá consumer khác.
- [ ] Unknown/quality/freshness; attribute scopes kiểm tra riêng.
- [ ] Revoke during job + queued retry output được chặn đúng.
- [ ] Event dedup/checkpoint; reset/source-gap không tạo alarm/count giả.
- [ ] Privacy: no raw image/embedding/token logs; retention policy.
- [ ] Dataset quality và workload report, không chỉ demo clip.

## CI dự kiến triển khai P0

PR: formatter + AST naming + include/dependency rules + host unit/contracts.
Merge: clean cross-build + package contents/dependencies validation.
Nightly: golden/replay + host ASan/UBSan; TSan job riêng nếu toolchain hỗ trợ.
Board: SDK smoke + buffer lifetime/perf; prerelease fault/soak/update matrix.
Current source: **đã có unit/contract test source và CTest cho validators, session,
cadence, ownership và output helpers; eSDK cross-build đạt và 57/57 binary hiện pass
trực tiếp trên QCS6490 theo `docs/testing/qsc6490_board.md`;
chưa có workflow CI hoặc AST naming checker**.
.editorconfig/.clang-format chỉ là cấu hình, không tự cưỡng chế mọi quy tắc.
