# VQEC Vision AI Applications (LACAI)

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![Target QCS6490](https://img.shields.io/badge/target-QCS6490%20%2F%20Qualcomm%20Linux%201.8-blue)
![Tests](https://img.shields.io/badge/logic%20tests-see%20evidence-blue)
![Status](https://img.shields.io/badge/status-integration%20in%20progress-yellow)

Workspace C++17 của team AI APP: nhận 1..16 luồng FW RAW NV12/FD trên AI Camera và
AI Box, chạy inference/feature đa model và sản xuất overlay + H264 vào FW ring. FW giữ
sensor/ISP/RTSP/UI/recording; AI APP sở hữu runtime, perception/usecase, toàn bộ protected
FR gallery/key/matching/index và preview overlay/encode.

Tài liệu trạng thái nguồn sự thật: [implementation status](docs/development/implementation_status.md).
Quy tắc bắt buộc cho mọi thay đổi: [AGENTS.md](AGENTS.md).

## Trạng thái hiện tại

| Mảng | Trạng thái | Bằng chứng / còn lại |
|---|---|---|
| Contracts / core neutral | Source-delivered | Validator, plan, ledger, output policy, feature/model catalog |
| Camera adapter (FW RAW) | Source-delivered | Wire decoder, SOCK_SEQPACKET/SCM_RIGHTS, lease Start/Stop, source lifecycle; chưa có live FW service |
| Multi-source / multi-model pump | Source-delivered | 1..16 session, cadence, shared-owner fan-out; device-free tested |
| Qualcomm plugin backend | Source-delivered | Graph lifecycle, typed tensor extraction, submission; lifecycle + installed-plugin check pass native trên QCS6490 |
| QNN engine LACAI-owned | **Board-verified (sync)** | compose + finalize + execute SCRFD/EdgeFace/YOLOv8n trên HTP V68; person output byte-identical với `qnn-net-run`; async/shared/update chưa |
| Perception / feature pipeline | Source-delivered + person smoke | YOLOv8 decoder, IoU tracker, feature pipeline; person detections đã chạy từ camera thật qua QNN HTP |
| Face cascade | Compatibility board smoke | SCRFD decode → exact-frame FastCV alignment → EdgeFace → typed embedding đã chạy trên `.98`; golden/post-fix multi-face/released-FW acceptance chưa |
| Output / preview / encoded | Qualcomm board smoke | FastCV preprocess + QNN HTP + QTI overlay/H.264 đạt 30 AI results/s và 30.1 RTSP FPS trên compatibility flow; released-FW/thermal/latency acceptance chưa |
| Service `vqec_ai_vision_applications` | Chạy được | Reference/fake dưới QEMU; Qualcomm production flow đã chạy trên `192.168.138.98` qua compatibility FW services |

**Evidence snapshot 2026-09-17:** expanded eSDK QEMU suite **123/123**; board `.98` native
suite **117/117** with fixtures via `tools/board/vqec_vision_board_native_tests.sh`. Production
smoke publishes H.264 1920x1080 30/1 with D-Bus FR transitions and `first_error=0`. The
routed-result metric is `route_latency_*` (steady reservation-to-routing). Clean-base
remediation (ring ABI, QNN reload, config, validation, CI) is complete; see
[implementation status](docs/development/implementation_status.md). Golden parity, post-fix
multi-face, released-FW and thermal/performance acceptance remain open. Older suite counts
are historical runs and are not retroactively changed.
See [implementation status](docs/development/implementation_status.md),
[architecture alignment review](docs/development/architecture_alignment_review.md) and
[open architecture issues](docs/development/architecture_alignment_review.md).

## Kiến trúc tổng quan

```text
        FW (sensor/ISP/RTSP/decode)                    FW (RTSP/UI/recording)
                 │ NV12/FD lease                              ▲
                 ▼                                             │ H264 + metadata
   raw_source_port ─► multi_source_supervisor ─► multi_model_session
                                                        │
                                     image_processor_port│ (NV12 → tensor)
                                                        ▼
                              inference_graph_port ◄── QNN engine (owned) / plugin graph
                                                        │
                                                        ▼
      perception_result_stage ─► tracking / attributes ─► feature pipeline / fan-out
                 │                                      │
                 └─ cascade root ─► retained frame      │
                                      │                 │
                         image_alignment_port            │
                                      │                 │
                         secondary inference_graph       │
                                      │ embedding       │
                                      └─────────────────┘
                                                        ▼
                                 output_gate ─► overlay + encoded_sink (FW ring)
```

Orchestration phụ thuộc neutral ports. Composition root ghép concrete adapters; vendor
implementation và SDK types giữ trong adapters, không đưa vào neutral contracts/runtime.
Xem [system architecture](docs/architecture/system_architecture.md) và
[runtime composition factory](docs/architecture/runtime_composition_factory.md).

## Cấu trúc repository

| Path | Nội dung |
|---|---|
| `include/vqec/vision/ai/contracts/` | Descriptor/interface neutral công khai (không vendor type) |
| `include/vqec/vision/ai/ports/` | Port runtime neutral (`raw_source`, `inference_graph`, `image_processor`, `tracker`, ...) |
| `src/core/` | Validation/plan/ledger/policy thuần, không I/O |
| `src/app/` | Composition: pump, session, supervisor, pipeline, executor, service main |
| `src/perception/` | Detection/tracking/attributes/embedding/ocr/pose |
| `src/features/` | 14 gói feature (hiện chỉ README; processor/entitlement chưa có) |
| `src/outputs/` | Overlay, encoded dispatch, feature-event dispatch |
| `src/runtime/` | Admission, lifecycle, model registry, feature manager, scheduler |
| `src/adapters/` | Qualcomm, camera, FW control/output, storage, Zvec và reference |
| `docs/` | Architecture, ADR, contracts, research, testing, development |
| `config/`, `manifests/` | Schema + example cho deployment/model/feature catalog |
| `tools/`, `.github/workflows/` | Checker cấu trúc, smoke script, CI |

## Build và test

Bắt buộc dùng toolchain eSDK; host compiler không được coi là bằng chứng.

```bash
source /home/a/Workspace/eSDK/environment-setup-armv8-2a-qcom-linux

# Logic-only profile (explicitly disables Zvec; not the default product build)
lacai_neutral_build="$(mktemp -d /tmp/lacai-esdk-neutral.XXXXXX)"
cmake -S . -B "$lacai_neutral_build" -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
  -DVQEC_VISION_AI_ENABLE_ZVEC=OFF \
  "-DCMAKE_CROSSCOMPILING_EMULATOR=/home/a/Workspace/eSDK/tmp/sysroots/x86_64/usr/bin/qemu-aarch64;-L;$SDKTARGETSYSROOT"
cmake --build "$lacai_neutral_build" -j4
ctest --test-dir "$lacai_neutral_build" --output-on-failure

# Cấu hình mở rộng; Zvec enabled by default, acquire pinned public SDK once
bash tools/build/vqec_vision_prepare_zvec.sh
# Skip bootstrap when third_party/zvec/sdk already exists.
# Camera, D-Bus, GStreamer, Qualcomm, JSON, digest, QNN engine
lacai_expanded_build="$(mktemp -d /tmp/lacai-esdk-expanded.XXXXXX)"
cmake -S . -B "$lacai_expanded_build" -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
  -DVQEC_VISION_AI_ENABLE_CAMERA=ON -DVQEC_VISION_AI_ENABLE_CAMERA_DBUS=ON \
  -DVQEC_VISION_AI_ENABLE_GST_FRAME_BRIDGE=ON -DVQEC_VISION_AI_ENABLE_QUALCOMM=ON \
  -DVQEC_VISION_AI_ENABLE_FASTCV=ON \
  -DVQEC_VISION_AI_ENABLE_QNN_ENGINE=ON -DVQEC_VISION_AI_ENABLE_MODEL_MANIFEST=ON \
  -DVQEC_VISION_AI_ENABLE_MODEL_CATALOG=ON -DVQEC_VISION_AI_ENABLE_DEPLOYMENT_CONFIG=ON \
  -DVQEC_VISION_AI_ENABLE_FEATURE_CATALOG=ON -DVQEC_VISION_AI_BUILD_MANIFEST_CHECK=ON \
  -DVQEC_VISION_AI_ENABLE_ARTIFACT_DIGEST=ON \
  "-DCMAKE_CROSSCOMPILING_EMULATOR=/home/a/Workspace/eSDK/tmp/sysroots/x86_64/usr/bin/qemu-aarch64;-L;$SDKTARGETSYSROOT"
cmake --build "$lacai_expanded_build" -j4
ctest --test-dir "$lacai_expanded_build" --output-on-failure
```

Kiểm tra cấu trúc filename/include (read-only):

```bash
bash tools/checks/vqec_vision_check_source_layout.sh
# PowerShell alternative: tools/checks/vqec_vision_check_source_layout.ps1
```

Board smoke: [QCS6490 target](docs/testing/qsc6490_board.md) và
[QNN validation runbook](docs/testing/qnn_board_validation.md).

## Đọc theo thứ tự

1. [Quy tắc bắt buộc](AGENTS.md)
2. [Code convention](docs/development/code_convention.md), [naming registry](docs/development/naming_registry.md) và [documentation style](docs/development/documentation_style.md)
3. [Kiến trúc hệ thống](docs/architecture/system_architecture.md)
4. [Qualcomm source review](docs/research/qualcomm_source_review.md) và [Qualcomm adapter](docs/architecture/qualcomm_adapter.md)
5. [Owned QNN engine ADR](docs/adr/0003_owned_qnn_engine.md) và [execution policy](docs/architecture/qualcomm_execution_policy.md)
6. Contracts: [camera service](docs/contracts/camera_service.md), [model integration](docs/contracts/model_integration.md), [FW control](docs/contracts/fw_control.md)
7. [FW–AI APP integration contract](docs/contracts/fw_ai_app_contract.md) và [FW release compatibility](docs/contracts/fw_release_compatibility.md)
8. [Feature catalog](docs/architecture/feature_catalog.md) và [model catalog](docs/architecture/model_catalog.md)
9. [Multi-source configuration](docs/architecture/multi_source_configuration.md)
10. [Implementation status](docs/development/implementation_status.md), [capability matrix](docs/development/capability_matrix.md), [review checklist](docs/development/review_checklist.md)
11. [Architecture improvement plans](docs/planning/architecture_improvement/README.md), [Model integration M0–M4](docs/planning/model_integration_plan.md)

## Phạm vi

- Nhận 1..16 logical FW RAW source cùng NV12/FD lease contract trên AI Camera và AI Box;
  profile explicit, không mặc định 4K/1080p. Sensor/ISP/RTSP/demux/decode thuộc FW.
- C++17, không OpenCV trong production; vendor SDK chỉ nằm trong `src/adapters/`.
- Qualcomm (QCS6490 / Qualcomm Linux 1.8) là target đầu tiên; contract trung lập cho
  Rockchip/MediaTek/Novatek.
- Runtime chia sẻ perception giữa các feature tương thích; bật/tắt theo entitlement.
- AI sở hữu preview overlay/encode/ring production; FW sở hữu RTSP/UI và recording.
- Epoch/clock/ownership/quantization của tensor phải tường minh; không release buffer khi
  hardware còn có thể truy cập.

## Bên thứ ba

| Thành phần | Vai trò | License |
|---|---|---|
| `third_party/qai_appbuilder` (submodule) | Reference cơ chế plugin QNN, không link | BSD-3-Clause |
| `third_party/qairt` (symlink, gitignored) | QAIRT/QNN SDK dùng để build QNN engine | Qualcomm proprietary |
| `third_party/zvec` (SDK gitignored, fetch qua script) | Vector index FR (derived) | pinned public SDK |
| `third_party/nlohmann` | JSON parser vendored | MIT |

Xem [third_party/README.md](third_party/README.md). Không commit model binary, SDK private,
dữ liệu sinh trắc học hay secret.

## Giới hạn đã biết

- Camera/FW thật: chỉ có compatibility simulator; chưa có released-FW conformance,
  hardware-completion/DMA-BUF device evidence, accuracy theo nhãn hay multi-camera/thermal/
  performance acceptance.
- Tracker/feature production và attribute producer chưa có; model/usecase thật dùng reference
  tracker/zone. 14 feature package hiện chỉ là README.
- Owned QNN engine execute sync (đã board-verified, có reload path); async, shared/registered
  memory và LoRA mới có contract, chưa dùng trong execute.
- CI workflow có 6 job: structural/host-sanitizers/clang-tidy/fuzz chạy mặc định; eSDK
  neutral/expanded gate sau `vars.ESDK_ROOT` (self-hosted), chưa có bằng chứng runner được cấu hình.
- Structural checker kiểm filename/include/CMake, không phải AST naming hay ownership validator.

Current runtime D-Bus/enrollment evidence and remaining production gates:
[FR validation](docs/testing/face_recognition_production_validation.md).
