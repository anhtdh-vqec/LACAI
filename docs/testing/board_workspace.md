# Board workspace and workflow

Scope: the canonical QCS6490 board workspace, its directory layout, and the exact build,
stage, test and run workflow. Every board session follows this so evidence is reproducible
and no personal directory names enter the repository.

**Status:** board-smoke — board `.98` native 121/121 and live H.264 1920x1080 30/1
corrective smoke ran from this layout on 2026-09-17. **Layer:** docs. **Source:** n/a (references
`tools/vqec_vision_board_native_tests.sh` and the board workspace).

## Standard root and layout

The board workspace is **`/opt/lacai`**. Do not use a personal or date-stamped directory
name. The layout is:

| Path | Contents |
|---|---|
| `bin/` | Canonical service binary `vqec_ai_vision_applications` only |
| `config/` | Deployment, model catalog, model registry, usecase snapshot, reviewed hardware admission profile, FR fixture |
| `models/` | Licensed model artifacts and their package dirs |
| `manifests/` | Staged repository `manifests/models` tree (decoder/IO/preprocess fixtures) |
| `lib/` | Zvec shared libraries for `LD_LIBRARY_PATH` |
| `tests/` | Cross-built native test binaries staged for the board runner |
| `tools/` | Board-side Python simulators/peers and the board runner copy |
| `inputs/` | Raw tensors/inputs for smoke tools |
| `enrollment/` | Authorized enrollment images under the configured root |
| `protected_gallery/` | AI-owned encrypted gallery/key/lock files (mode 0600) |
| `out/` | Service/RTSP/camera/dbus logs (never evidence by itself) |
| `*_run_*.sh` | Board-local convenience run scripts (not repository source) |

`/opt` is backed by the writable rootfs overlay; the directory is owned by the service UID.
Private Zvec storage lives on tmpfs under `/run`, not here.

## Host build and stage workflow

1. Build with the approved eSDK toolchain on the host (never a host compiler):

   ```bash
   source /home/a/Workspace/eSDK/environment-setup-armv8-2a-qcom-linux
   cmake --build build-esdk-full -j4
   ctest --test-dir build-esdk-full --output-on-failure
   ```

2. Stage the artifacts to the board (the user handles pushes; this is `scp`/`ssh` only):

   - service binary `build-esdk-full/src/app/vqec_ai_vision_applications` -> `/opt/lacai/bin/vqec_ai_vision_applications`;
   - native test binaries `vqec_vision_ai_*test*` -> `/opt/lacai/tests/`;
   - repository `manifests/models` -> `/opt/lacai/manifests/models`;
   - `tools/vqec_vision_board_native_tests.sh` -> `/opt/lacai/tools/`;
   - Zvec shared libraries -> `/opt/lacai/lib/`.

3. `sync` before executing a freshly written binary on the `/opt` overlay.

## Native test workflow

Run the repository board runner, which supplies the fixtures two device-free tests need:

```bash
cd /opt/lacai
LD_LIBRARY_PATH=/opt/lacai/lib sh tools/vqec_vision_board_native_tests.sh \
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

The subsequent Plan 0 corrective run on 2026-09-17 staged the current native binary set
and example-profile fixture: `PASS=121 FAIL=0`. See the
[exact-candidate review](../development/production_composition_foundation_review.md).
Candidate smoke may temporarily stage `bin/vqec_ai_vision_applications.plan0` without
replacing the canonical service; hash it, record its profile and clean it up after testing.

## Production smoke workflow

The production smoke publishes AI overlay + H.264 to the FW v5 ring, read back by the mock
RTSP service. It needs the compatibility camera simulator, the ring reader, and the service
with its full explicit configuration (the service refuses to start with any required value
missing).

Canonical service invocation (all flags are required):

Provision `config/hardware_admission_profile.json` from the measured, owner-reviewed
schema in [hardware admission profile](../architecture/hardware_admission_profile.md).
No repository fixture supplies product capacity. The example below cannot run until the
file and its evidence are available; do not fabricate limits to make it start.

```bash
dbus-run-session -- sh -c '
  python3 /opt/lacai/tools/vqec_vision_fr_runtime_dbus_test.py \
    --fixture /opt/lacai/config/fr_runtime_test.json >/opt/lacai/out/dbus.log 2>&1 &
  sleep 1
  exec /opt/lacai/bin/vqec_ai_vision_applications \
    --mode production --platform qualcomm \
    --usecase-dbus-session --usecase-service-name com.vqec.AiVision.Control \
    --usecase-object-path /com/vqec/AiVision/UsecaseControl \
    --usecase-peer-name com.vqec.FwEnrollmentTest \
    --usecase-rpc-timeout-ms 10000 --usecase-callbacks-per-poll 8 \
    --deployment /opt/lacai/config/deployment.json \
    --model-catalog /opt/lacai/config/model_catalog.json \
    --usecase-snapshot /opt/lacai/config/usecase_control_snapshot.json \
    --model-package-registry /opt/lacai/config/model_registry.json \
    --qnn-backend-library /usr/lib/libQnnHtp.so \
    --qnn-system-library /usr/lib/libQnnSystem.so \
    --model-root /opt/lacai/models/ \
    --hardware-profile /opt/lacai/config/hardware_admission_profile.json \
    --tracker-contract portable.iou.tracker.v1 \
    --event-schema-id reference.zone --event-schema-version 1 \
    --consumer-id-prefix lacai_ai \
    --camera-socket-dir /run/camera_ai --camera-producer-uid 0 --nv12-format 23 \
    --output-ring-id encoded_ai_detect0_cam0_ch0 \
    --output-bitrate 4000000 --output-keyframe-interval 30 \
    --output-box-color-rgba 0x00ff00ff --output-surface-count 2 \
    --output-colorimetry bt709 --output-interlace-mode progressive \
    --fr-gallery-path /run/lacai_fr_index/face_protected_1 \
    --fr-protected-directory /opt/lacai/protected_gallery \
    --fr-gallery-file gallery.bin --fr-key-file gallery.key --fr-lock-file gallery.lock \
    --fr-gallery-id face_protected_1 --fr-preprocess-revision 1 \
    --fr-store-max-bytes 16777216 --fr-min-similarity 0.35 --fr-subject-margin 0.05 \
    --fr-max-templates 5 --fr-top-k 5 \
    --fr-feature-id face_recognition --fr-identity-attribute subject_ref \
    --enrollment-dbus-session --enrollment-peer-name com.vqec.FwEnrollmentTest \
    --enrollment-image-root /opt/lacai/enrollment
'
```

Before starting, the camera simulator and reader must run, and the tmpfs private index
parent must exist:

```bash
mkdir -m 0777 /run/camera_ai
mkdir -p -m 0700 /run/lacai_fr_index        # service-UID-owned tmpfs parent
python3 /opt/lacai/tools/vqec_vision_fw_camera_sim.py \
  --socket-dir /run/camera_ai --camera 0 --channel 0 --consumer ai \
  --width 1920 --height 1080 --fps 30 --max-in-flight 3 &
python3 /opt/lacai/tools/vqec_vision_ring_rtsp.py read \
  --ring-id encoded_ai_detect0_cam0_ch0 --port 8554 --mount /live/ai/detect0 --fps 30 &
```

Verify from the host:

```bash
ffprobe -rtsp_transport tcp -v error \
  -show_entries stream=codec_name,width,height,r_frame_rate \
  -of default=noprint_wrappers=1 rtsp://192.168.138.98:8554/live/ai/detect0
```

Expected: H.264, 1920x1080, `30/1`; service metrics print `first_error=0`, cascade
`cascade_failed=0`, and a steady `route_latency_*`.

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
  workspace; stop every started process and remove ring files before finishing a session.

## See also

- [QSC6490 target](qsc6490_board.md), [FW camera service](lacai_camera_service.md)
- [FW release compatibility](../contracts/fw_release_compatibility.md)
- [eSDK emulation](esdk_emulation.md), [eSDK configuration matrix](esdk_configuration_matrix.md)
- [Board native test runner](../../tools/vqec_vision_board_native_tests.sh)
