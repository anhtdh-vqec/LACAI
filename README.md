# VQEC Vision AI Applications (LACAI)

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![Target QCS6490](https://img.shields.io/badge/target-QCS6490%20%2F%20Qualcomm%20Linux%201.8-blue)
![Tests](https://img.shields.io/badge/logic%20tests-71%20passing%20(QEMU)-green)
![Status](https://img.shields.io/badge/status-base--ready-yellow)

Workspace C++17 của team AI APP: nhận 1..16 luồng FW RAW NV12/FD trên AI Camera và
AI Box, chạy inference/feature đa model và sản xuất overlay + H264 vào FW ring. FW giữ
sensor/ISP/RTSP/UI/recording; AI APP chỉ sở hữu perception và preview overlay/encode.

Tài liệu trạng thái nguồn sự thật: [implementation status](docs/development/implementation_status.md).
Quy tắc bắt buộc cho mọi thay đổi: [AGENTS.md](AGENTS.md).

## Trạng thái hiện tại

| Mảng | Trạng thái | Bằng chứng / còn lại |
|---|---|---|
| Contracts / core neutral | Source-delivered | Validator, plan, ledger, output policy, feature/model catalog |
| Camera adapter (FW RAW) | Source-delivered | Wire decoder, SOCK_SEQPACKET/SCM_RIGHTS, lease Start/Stop, source lifecycle; chưa live FW |
| Multi-source / multi-model pump | Source-delivered | 1..16 session, cadence, shared-owner fan-out; device-free tested |
| Qualcomm plugin backend | Source-delivered | Graph lifecycle, typed tensor extraction, submission; board-tested ở mức lifecycle |
| QNN engine LACAI-owned | Source-delivered | dlopen + device + compose + execute + `inference_graph_port`; chưa qualify trên board |
| Perception / feature pipeline | Source-delivered (ports) | Decoder/tracker/feature là contract + registry; thuật toán thật chưa có |
| Output / preview / encoded | Helpers delivered | Chưa ghép thành pipeline chạy thật; thiếu renderer + hardware encoder |
| Service `vqec_ai_vision_applications` | Harness chạy được | Chạy device-free dưới QEMU; production mode fail-closed |

**Bằng chứng logic:** cấu hình default (mọi option OFF) **53/53** test và cấu hình mở rộng
(Camera, GIO D-Bus, GStreamer bridge, Qualcomm, JSON, digest, QNN engine) **71/71** test
chạy 100% dưới eSDK QEMU. Cấu hình mở rộng trước đó cũng pass natively trên QCS6490.
Đây là logic/wiring evidence, **không** phải board/BSP, model-accuracy, performance hay
zero-copy acceptance. Chi tiết: [esdk_emulation](docs/testing/esdk_emulation.md),
[qsc6490_board](docs/testing/qsc6490_board.md), [qnn board runbook](docs/testing/qnn_board_validation.md).

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
                                                        │
                                                        ▼
                                 output_gate ─► overlay + encoded_sink (FW ring)
```

Application chỉ phụ thuộc neutral ports; vendor/QNN/GStreamer chỉ nằm trong `src/adapters/`.
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
| `src/features/` | 13 gói feature (metadata + processor) |
| `src/outputs/` | Overlay, encoded dispatch, feature-event dispatch |
| `src/runtime/` | Admission, lifecycle, model registry, feature manager, scheduler |
| `src/adapters/` | Qualcomm, camera, FW control/output, reference, platform để trống |
| `docs/` | Architecture, ADR, contracts, research, testing, development |
| `config/`, `manifests/` | Schema + example cho deployment/model/feature catalog |
| `tools/`, `.github/workflows/` | Checker cấu trúc, smoke script, CI |

## Build và test

Bắt buộc dùng toolchain eSDK; host compiler không được coi là bằng chứng.

```bash
source /home/a/Workspace/eSDK/environment-setup-armv8-2a-qcom-linux

# Cấu hình default (mọi adapter OFF)
cmake -S . -B build-esdk-neutral -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
  "-DCMAKE_CROSSCOMPILING_EMULATOR=/home/a/Workspace/eSDK/tmp/sysroots/x86_64/usr/bin/qemu-aarch64;-L;$SDKTARGETSYSROOT"
cmake --build build-esdk-neutral -j4
ctest --test-dir build-esdk-neutral --output-on-failure

# Cấu hình mở rộng (Camera, D-Bus, GStreamer, Qualcomm, JSON, digest, QNN engine)
cmake -S . -B build-esdk-full -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
  -DVQEC_VISION_AI_ENABLE_CAMERA=ON -DVQEC_VISION_AI_ENABLE_CAMERA_DBUS=ON \
  -DVQEC_VISION_AI_ENABLE_GST_FRAME_BRIDGE=ON -DVQEC_VISION_AI_ENABLE_QUALCOMM=ON \
  -DVQEC_VISION_AI_ENABLE_QNN_ENGINE=ON -DVQEC_VISION_AI_ENABLE_MODEL_MANIFEST=ON \
  -DVQEC_VISION_AI_ENABLE_MODEL_CATALOG=ON -DVQEC_VISION_AI_ENABLE_DEPLOYMENT_CONFIG=ON \
  -DVQEC_VISION_AI_ENABLE_FEATURE_CATALOG=ON -DVQEC_VISION_AI_BUILD_MANIFEST_CHECK=ON \
  -DVQEC_VISION_AI_ENABLE_ARTIFACT_DIGEST=ON \
  "-DCMAKE_CROSSCOMPILING_EMULATOR=/home/a/Workspace/eSDK/tmp/sysroots/x86_64/usr/bin/qemu-aarch64;-L;$SDKTARGETSYSROOT"
cmake --build build-esdk-full -j4
ctest --test-dir build-esdk-full --output-on-failure
```

Kiểm tra cấu trúc filename/include (read-only):

```bash
powershell -NoProfile -File tools/vqec_vision_check_source_layout.ps1
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
10. [Implementation status](docs/development/implementation_status.md), [review checklist](docs/development/review_checklist.md)
11. [Delivery plan](docs/planning/delivery_plan.md)

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
| `third_party/nlohmann` | JSON parser vendored | MIT |

Xem [third_party/README.md](third_party/README.md). Không commit model binary, SDK private,
dữ liệu sinh trắc học hay secret.

## Giới hạn đã biết

- Chưa có live FW/model integration, board qualification cho QNN engine, hardware-completion,
  performance, thermal hay recovery evidence.
- Chưa có decoder/tracker/feature/renderer/hardware encoder thật; đây là các port + registry.
- Owned QNN engine hiện execute sync/copy; async, shared/registered memory và LoRA mới có
  contract, chưa dùng trong execute.
- CI workflow có job eSDK đang gate sau `vars.ESDK_ROOT`; chưa có bằng chứng runner được cấu hình.
- Structural checker kiểm filename/include/CMake, không phải AST naming hay ownership validator.
