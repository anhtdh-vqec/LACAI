# Board workspace and workflow

Scope: the canonical QCS6490 board workspace, its directory layout, and the exact build,
stage, test and run workflow. Every board session follows this so evidence is reproducible
and no personal directory names enter the repository.

**Status:** accepted — the minimal canonical layout ran the declared full workload at
30.008 FPS and 13.50% average CPU for five minutes on `.98` on 2026-09-18. **Layer:** docs.
**Source:** `tools/board/vqec_vision_run_full.sh`.

## Standard root and layout

The board workspace is **`/opt/lacai`**. Do not use a personal or date-stamped directory
name. The layout is:

| Path | Contents |
|---|---|
| `bin/` | Canonical service binary `vqec_ai_vision_applications` only |
| `config/` | Full deployment, model catalog/registry, usecase snapshot and measured hardware profile |
| `models/` | Licensed model artifacts and their package dirs |
| `dsp/v1/` | Exact negotiated v1 skeleton and its build/signing receipt |
| `lib/` | Zvec shared libraries for `LD_LIBRARY_PATH` |
| `tools/board/` | The repository-owned full-workload runner |
| `tools/fixtures/` | DMA-heap compatibility camera and RTSP ring reader |
| `protected_gallery/` | AI-owned encrypted gallery/key/lock files (mode 0600) |
| `data/metadata/` | AI-owned P2 catalog and bounded detail shards; never opened by FW/UI |
| `out/` | Current logs and the retained `acceptance_5m/` raw evidence |
| `run_full.sh` | Canonical link/copy of `tools/board/vqec_vision_run_full.sh` |

`/opt` is backed by the writable rootfs overlay; the directory is owned by the service UID.
Private Zvec storage lives on tmpfs under `/run`, not here.

## Host build and stage workflow

1. Build with the approved eSDK toolchain on the host (never a host compiler):

   ```bash
   source /home/a/Workspace/eSDK/environment-setup-armv8-2a-qcom-linux
   lacai_build_dir="$(mktemp -d /tmp/lacai-esdk.XXXXXX)"
   cmake -S . -B "$lacai_build_dir" -DBUILD_TESTING=ON \
     -DVQEC_VISION_AI_ENABLE_CAMERA=ON -DVQEC_VISION_AI_ENABLE_CAMERA_DBUS=ON \
     -DVQEC_VISION_AI_ENABLE_GST_FRAME_BRIDGE=ON \
     -DVQEC_VISION_AI_ENABLE_QUALCOMM=ON -DVQEC_VISION_AI_ENABLE_FASTCV=ON \
     -DVQEC_VISION_AI_ENABLE_QNN_ENGINE=ON \
     -DVQEC_VISION_AI_ENABLE_MODEL_MANIFEST=ON \
     -DVQEC_VISION_AI_ENABLE_MODEL_CATALOG=ON \
     -DVQEC_VISION_AI_ENABLE_DEPLOYMENT_CONFIG=ON \
     -DVQEC_VISION_AI_ENABLE_FEATURE_CATALOG=ON \
     -DVQEC_VISION_AI_ENABLE_ARTIFACT_DIGEST=ON
   cmake --build "$lacai_build_dir" -j"$(nproc)"
   ctest --test-dir "$lacai_build_dir" --output-on-failure
   ```

2. Stage the artifacts to the board (the user handles pushes; this is `scp`/`ssh` only):

   - service binary `$lacai_build_dir/src/app/vqec_ai_vision_applications` -> `/opt/lacai/bin/vqec_ai_vision_applications`;
   - validated metadata profile -> `/opt/lacai/config/metadata_runtime_profile.json`;
   - Hexagon-built `libvqec_vision_dsp_v1_skel.so` plus receipt -> `/opt/lacai/dsp/v1/`;
   - native test binaries and manifests may be staged into a temporary candidate directory for
     validation, then removed after evidence is retained; they are not part of runtime layout;
   - `tools/board/vqec_vision_run_full.sh` -> `/opt/lacai/tools/board/` and
     `/opt/lacai/run_full.sh`;
   - required `tools/fixtures/` scripts -> `/opt/lacai/tools/fixtures/`;
   - Zvec shared libraries -> `/opt/lacai/lib/`.

3. `sync` before executing a freshly written binary on the `/opt` overlay.

## Native test workflow

Run the repository board runner, which supplies the fixtures two device-free tests need:

```bash
cd /opt/lacai
LD_LIBRARY_PATH=/opt/lacai/lib sh tools/board/vqec_vision_board_native_tests.sh \
  tests manifests/models <zvec_tmpfs_root> <zvec_scratch_base>
```

- `<zvec_tmpfs_root>` must be an existing current-UID-owned mode-0700 tmpfs directory
  (e.g. `/run/lacai_zvec_check`); `/opt/lacai/manifests/models` supplies the decoder package
  fixtures.
- `<zvec_scratch_base>` must be a writable directory on a **non-tmpfs** filesystem (e.g.
  `/opt/lacai/zvec_scratch`); the Zvec test asserts that the default private policy rejects
  its collection path on a non-tmpfs parent.
- Relative arguments are resolved to absolute by the runner, but absolute paths are
  recommended.

The historical 2026-09-17 runner result was `PASS=117 FAIL=0`; do not use 117 as an
acceptance target for the current binary set.

The latest clean-layout run on 2026-09-18 staged the current native binary set and fixtures:
`PASS=128 FAIL=0`. See the
[exact-candidate review](../development/production_composition_foundation_review.md).
Candidate smoke may temporarily stage `bin/vqec_ai_vision_applications.plan0` without
replacing the canonical service; hash it, record its profile and clean it up after testing.

## Production smoke workflow

The canonical runner validates required files and the DMA heap, creates bounded runtime state,
starts the compatibility camera, full production service and RTSP bridge, and prints the VLC URL
only after the bridge reports its first decodable frame. A live process or an allocated ring alone
is not readiness. `LACAI_PREVIEW_READY_TIMEOUT_SECONDS` bounds cold QNN/DSP startup separately
from the socket/ring creation timeout.
It owns only processes whose PID and command match its state files; it does not use `killall`.

```bash
/opt/lacai/run_full.sh start
/opt/lacai/run_full.sh status
/opt/lacai/run_full.sh logs
/opt/lacai/run_full.sh stop
```

Open `rtsp://192.168.0.102:8554/live/ai/detect0` in VLC after `start` reports success.
`restart` performs the bounded stop and full start. Deployment choices can be overridden through
documented `LACAI_*` environment variables in the script, while the canonical defaults use only
`/opt/lacai` and the current `.102` address. The default full workload uses parallel model
execution, eight output surfaces, 30 FPS preview and the registered `qcom,system` DMA heap.

Verify from the host:

```bash
tools/board/vqec_vision_preview_acceptance.sh \
  --uri rtsp://192.168.0.102:8554/live/ai/detect0 \
  --output-dir /tmp/lacai-preview-acceptance \
  --duration-seconds 8 --expected-width 1920 --expected-height 1080 \
  --expected-fps 30 --fps-tolerance 1.0
```

The tool must pass codec, dimensions and effective packet FPS. Open
`overlay_contact_sheet.png` and explicitly verify correct boxes/labels, coordinates,
orientation, color and absence of stale overlay. Service metrics must also print
`first_error=0`, `cascade_failed=0` and a steady `route_latency_*`.

## Troubleshooting and rules

- **"expected exactly one configured AI service process"**: more than one
  `vqec_ai_vision_applications` is running, or the fixture `service_executable` path does
  not match the staged binary. Kill stale processes and keep the fixture pointed at
  `/opt/lacai/bin/vqec_ai_vision_applications`.
- **"private Zvec parent cannot be opened" / "must be service-owned mode-0700 tmpfs"**: the
  `/run/lacai_fr_index` tmpfs parent is missing or has the wrong owner/mode. `/run` is
  cleared on reboot; recreate it before each session.
- **Service refuses to start with "invalid production platform configuration"**: one of the
  required CLI values is missing. The service never substitutes a built-in default.
- **Newly written binary on `/opt` does not execute**: run `sync`, or copy it to `/tmp`
  first.
- **Rules:** no personal directory names in paths or repository content; no credentials,
  model binaries, biometric data or private SDKs in Git; `/opt/lacai` is the only board
  workspace. A requested live-view session may be left running and must be reported explicitly;
  otherwise stop it with the canonical runner before finishing.

## See also

- [QSC6490 target](qsc6490_board.md), [FW camera service](lacai_camera_service.md)
- [FW release compatibility](../contracts/fw_release_compatibility.md)
- [eSDK emulation](esdk_emulation.md), [eSDK configuration matrix](esdk_configuration_matrix.md)
- [Board native test runner](../../tools/board/vqec_vision_board_native_tests.sh)
- [Preview acceptance capture](../../tools/board/vqec_vision_preview_acceptance.sh)
