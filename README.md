# VQEC Vision AI Applications

Source inventory update (2026-09-10): source includes the existing FW third
NV12/FD receiver, strict legacy decoder and session-owned ACK; configured targets now
cross-build with the AArch64 eSDK, and all 57 unit/contract binaries pass natively on the
QCS6490 target. See [board smoke evidence](docs/testing/qsc6490_board.md); live FW/model,
hardware-completion, performance and recovery qualification remain open.
See [camera adapter implementation boundary](docs/architecture/camera_legacy_adapter.md).
The released camera adapter follows FW effective profiles; the new deployment contract
requires explicit per-source dimensions/FPS and supports 1..16 unified FW RAW inputs on
both AI Camera and AI Box without a fixed 4K/1080p default. RTSP demux/decode on AI Box is
entirely FW-owned and invisible to AI APP. This is a parser ceiling, not a board
capacity claim. Start/Stop state machine and optional GIO D-Bus client
are now source-delivered; see [control client contract](docs/architecture/camera_control_client.md).
Combined control/media lifecycle is now source-delivered with explicit drain/release;
see [source lifecycle](docs/architecture/camera_source_lifecycle.md).
FD-to-GstMemory wrapping is now source-delivered; see
[memory bridge contract](docs/architecture/dmabuf_memory_bridge.md).
Submission/result/drain source is present and cross-builds; hardware completion validation
and a runnable end-to-end vqec_ai_vision_applications are still pending. Current status is
maintained in [implementation status](docs/development/implementation_status.md).

Workspace greenfield C++17 của team AI APP. Baseline tài liệu: 2026-09-06.
Build và test phải dùng toolchain eSDK tại `/home/a/Workspace/eSDK`; host compiler không
được dùng làm bằng chứng xác nhận target.
Trạng thái: **có source camera, multi-source/multi-model, Qualcomm inference, feature
integration catalog/pipeline và output helpers; eSDK cross-build và 57/57 target logic
tests đạt, chưa có live model/FW pipeline hoặc app thay release**.

Current priorities: [FW release compatibility](docs/contracts/fw_release_compatibility.md)
and [replacement execution gates](docs/planning/fw_compatibility_execution.md).
Neutral admitted owner construction is documented in
[runtime composition factory](docs/architecture/runtime_composition_factory.md).
AI APP must produce overlay/H264 into the released FW ring; FW retains RTSP/UI/recording.
Source filenames use `vqec_vision_`; run the structural check with
`powershell -NoProfile -File tools/vqec_vision_check_source_layout.ps1`.

## Đọc theo thứ tự

1. [Quy tắc bắt buộc](AGENTS.md).
2. [Code convention](docs/development/code_convention.md).
3. [Registry tên](docs/development/naming_registry.md).
4. [Kiến trúc](docs/architecture/system_architecture.md).
5. [Phân tích source Qualcomm](docs/research/qualcomm_source_review.md).
6. [Thiết kế adapter Qualcomm](docs/architecture/qualcomm_adapter.md).
7. [Camera contract](docs/contracts/camera_service.md), [Model contract](docs/contracts/model_integration.md),
   [FW control và entitlement](docs/contracts/fw_control.md).
   Contract gửi đội FW: [FW–AI APP integration contract](docs/contracts/fw_ai_app_contract.md).
8. [13 bài AI và traffic](docs/architecture/feature_catalog.md).
9. [Kế hoạch 12 tuần](docs/planning/delivery_plan.md).
10. [Review và nghiệm thu](docs/development/review_checklist.md).
11. [Tra cứu inventory plugin Qualcomm](docs/research/qualcomm_plugins_reference.md).
12. [Ranh giới adapter và cách dùng plugin Qualcomm](docs/architecture/qualcomm_plugin_adapter_reference.md).
13. [Quyết định dùng plugin backend](docs/adr/0002_qualcomm_plugin_backend.md).
14. [Source đã có và phần chưa triển khai](docs/development/implementation_status.md).
15. [Cấu hình 1..16 source và memory/zero-copy ledger](docs/architecture/multi_source_configuration.md).
16. [Model-team catalog và cross-admission](docs/architecture/model_catalog.md).

## Phạm vi

- Nhận 1..16 logical FW RAW source với cùng NV12/FD lease contract trên AI Camera và
  AI Box; profile explicit. Sensor/ISP/RTSP/demux/decode/credential đều thuộc FW.
- C++17, không OpenCV trong production; vendor SDK chỉ ở adapters.
- Qualcomm là target đầu tiên; contract trung lập cho Rockchip/MediaTek/Novatek.
- Runtime chia sẻ perception giữa các feature tương thích; bật/tắt theo entitlement.
- Package runtime/backend/feature/model độc lập; không đồng nghĩa mỗi feature một process.
- Tham khảo source Qualcomm bên ngoài, không copy hoặc sửa repository đó.
- Không coi source mẫu là bằng chứng hiệu năng, SDK compatibility hoặc zero-copy trên board.

## Những gì chưa được xác nhận

Người dùng đã xác nhận target QSC6490, Qualcomm Linux 1.8 và plugin đã chạy/tối ưu.
Phiên bản binary/model cụ thể, camera transport và KPI workload cần pin khi tích hợp.
Không yêu cầu sysroot để bắt đầu viết source. Các contract ở đây là đề xuất để bốn team ký,
không phải API đã được FW cung cấp.

Đã có validation/config loaders, activation snapshot, Camera lease bridge/session,
Qualcomm graph submit/result/drain, cadence, shared-frame multi-model session và
multi-source supervisor qua neutral ports. Feature activation manager và tracker factory
registry hiện đã tạo effective-state/owner boundary theo contract. Nhánh output đã có
CPU preview pool, encoder ledger/preparation/submit/drain helpers, authorized event
dispatch và optional FW SDK ring sink; chưa ghép thành pipeline chạy thực tế.
Qualcomm plugin factory probing is available at the adapter boundary; it reports
runtime availability without selecting a fallback backend.
CMake targets và unit/contract-test source đã có; cross-build eSDK đạt, 57/57 binary
unit/contract pass trực tiếp trên QCS6490. eSDK cũng có QEMU cho logic smoke; xem
[hướng dẫn emulation](docs/testing/esdk_emulation.md).
Chưa có decoder/tracker/feature thực tế, overlay renderer, concrete hardware encoder,
service vqec_ai_vision_applications/IPK, CI chạy tự động hoặc AST naming checker.
Structural filename checker đã có. Xem bảng kiểm kê source và phần thiếu tại
[implementation status](docs/development/implementation_status.md).
Không có model, dữ liệu khuôn mặt, key/license thật hay vendor binary trong Git.
