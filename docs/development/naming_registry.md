# Naming registry v2

Physical source/tool filenames use `vqec_vision_`; logical owners and registered
function prefixes do not change. Reserved filenames below follow the same rule.
See code_convention.md section 0 for the finite non-source exceptions.

Registry này là nguồn chuẩn, không tự viết tắt theo cảm tính.
Các file ở bảng Reserved là **reserved/planned**; bảng Implementation additions
ghi các file đã bắt đầu có source, chưa build.
Mỗi cặp dir_id + file_id chỉ có một logical owner; header/source cặp cùng API
được coi là một owner. Không tái sử dụng prefix đã export cho ý nghĩa khác.

## Directory IDs

| Directory | ID |
|---|---|
| include/vqec/vision/ai/contracts | cntr |
| include/vqec/vision/ai/ports | ports |
| include/vqec/vision/ai/plugin | plug |
| src/core | core |
| src/app | appl |
| src/runtime/lifecycle | life |
| src/runtime/feature_manager | ftmgr |
| src/runtime/graph | graph |
| src/runtime/scheduler | sched |
| src/runtime/admission | admis |
| src/runtime/model_registry | mreg |
| src/perception/detection | detec |
| src/perception/tracking | track |
| src/perception/attributes | attr |
| src/perception/pose | pose |
| src/perception/embedding | embed |
| src/perception/ocr | ocr |
| src/adapters/camera | camer |
| src/adapters/qualcomm | qcom |
| src/adapters/reference | refer |
| src/adapters/fw_control | fwctl |
| src/adapters/fw_output | fwout |
| src/adapters/rockchip | rchip |
| src/adapters/mediatek | mtek |
| src/adapters/novatek | ntek |
| src/outputs | outpt |
| src/features/abnormal_behavior | abnor |
| src/features/crowd_gathering | crowd |
| src/features/smoking | smoke |
| src/features/intrusion | intr |
| src/features/abandoned_object | aband |
| src/features/person_tracking | ptrak |
| src/features/retrieval | retr |
| src/features/ppe | ppe |
| src/features/suspicious_object | susp |
| src/features/luggage_tracking | ltrak |
| src/features/blacklist | blist |
| src/features/heatmap | heat |
| src/features/counting | count |
| src/features/traffic | traff |
| tests/unit | unit |
| tests/contract | ctest |
| tests/golden | gold |
| tests/replay | replay |
| tests/integration | integ |
| tests/board | board |
| tools | tools |

Thêm thư mục con chứa source cần ID riêng trước khi tạo hàm ở đó.
Chỉ thư mục có hàm cần ID; thư mục tài liệu/manifest không cần.

## Reserved file IDs

| Logical owner path | file_id | Prefix |
|---|---|---|
| src/adapters/qualcomm/vqec_vision_qnn_engine.cpp | qneng | vqec_vision_ai_qcom_qneng_ |
| src/adapters/qualcomm/vqec_vision_fastcv_processor.cpp | fcprc | vqec_vision_ai_qcom_fcprc_ |
| src/adapters/qualcomm/vqec_vision_buffer_manager.cpp | bufmg | vqec_vision_ai_qcom_bufmg_ |
| src/adapters/qualcomm/vqec_vision_backend_factory.cpp | bfact | vqec_vision_ai_qcom_bfact_ |
| src/adapters/qualcomm/vqec_vision_sdk_loader.cpp | sdkld | vqec_vision_ai_qcom_sdkld_ |
| src/adapters/qualcomm/vqec_vision_c2d_processor.cpp | c2dpr | vqec_vision_ai_qcom_c2dpr_ |
| src/adapters/camera/vqec_vision_frame_source.cpp | frsrc | vqec_vision_ai_camer_frsrc_ |
| src/core/vqec_vision_frame_lease.cpp | frlse | vqec_vision_ai_core_frlse_ |
| src/runtime/scheduler/vqec_vision_job_scheduler.cpp | jobsc | vqec_vision_ai_sched_jobsc_ |
| src/runtime/feature_manager/vqec_vision_feature_manager.cpp | ftmgr | vqec_vision_ai_ftmgr_ftmgr_ |
| include/vqec/vision/ai/contracts/vqec_vision_frame_source.hpp | frsrc | vqec_vision_ai_cntr_frsrc_ |
| include/vqec/vision/ai/contracts/vqec_vision_image_processor.hpp | imgpr | vqec_vision_ai_cntr_imgpr_ |
| include/vqec/vision/ai/contracts/vqec_vision_inference_engine.hpp | infer | vqec_vision_ai_cntr_infer_ |
| include/vqec/vision/ai/contracts/vqec_vision_buffer_manager.hpp | bufmg | vqec_vision_ai_cntr_bufmg_ |
| include/vqec/vision/ai/plugin/vqec_vision_backend_api.h | bkapi | vqec_vision_ai_plug_bkapi_ |

Ví dụ override image_processor vẫn mang tên
vqec_vision_ai_cntr_imgpr_submit_image, không đổi thành qcom_fcprc.
Ví dụ helper resize riêng FastCV dùng vqec_vision_ai_qcom_fcprc_resize_image.
Khi thêm source mới: thêm row exact path, owner reviewer, kiểm tra collision.
Đổi owner public cần ADR và kế hoạch compatibility.

## Implementation additions (2026-09-06)

Multi-source deployment contract:
`include/vqec/vision/ai/contracts/vqec_vision_deployment_config.hpp`, file_id `dpcfg`;
validation implementation `src/core/vqec_vision_deployment_config.cpp`, file_id `dpval`,
prefix `vqec_vision_ai_core_dpval_`. JSON loader owner
`src/runtime/lifecycle/vqec_vision_deployment_config.cpp`, file_id `dpcfg`, prefix
`vqec_vision_ai_life_dpcfg_`. The pure validator test uses file_id `dptst`,
prefix `vqec_vision_ai_unit_dptst_`; the JSON loader test uses file_id `dltst`,
prefix `vqec_vision_ai_unit_dltst_`. Test entrypoints retain the language exception.

Model catalog contract/validation:
`include/vqec/vision/ai/contracts/vqec_vision_model_catalog.hpp` and
`src/core/vqec_vision_model_catalog.cpp`, file_id `mdcat`, prefix
`vqec_vision_ai_core_mdcat_`. Test `tests/unit/vqec_vision_model_catalog_test.cpp`
uses file_id `mctst`, prefix `vqec_vision_ai_unit_mctst_`; main retains the language
exception. The JSON loader in runtime/model_registry uses the same physical stem,
dir owner `mreg`, and prefix `vqec_vision_ai_mreg_mdcat_`. Loader test
`tests/unit/vqec_vision_model_catalog_loader_test.cpp` uses file_id `mltst`, prefix
`vqec_vision_ai_unit_mltst_`; main retains the language exception.

Fixed-capacity activation mapping:
`src/runtime/admission/vqec_vision_activation_snapshot.cpp`, file_id `actsp`, prefix
`vqec_vision_ai_admis_actsp_`; paired private header has the same stem. Test
`tests/unit/vqec_vision_activation_snapshot_test.cpp` uses file_id `astst`, prefix
`vqec_vision_ai_unit_astst_`; main retains the language exception.

Observation contract: `include/vqec/vision/ai/contracts/vqec_vision_observation.hpp`,
file_id `obser`, prefix `vqec_vision_ai_cntr_obser_`; validation implementation
`src/core/vqec_vision_observation.cpp`, file_id `obval`, prefix `vqec_vision_ai_core_obval_`.
Test `tests/unit/vqec_vision_observation_test.cpp`, file_id `ovtst`, prefix
`vqec_vision_ai_unit_ovtst_`; main retains language exception.

Encoder input validation: `src/core/vqec_vision_encoder_contract.cpp`, file_id `encct`,
prefix `vqec_vision_ai_core_encct_`; declarations in encoder_backend contract header.
Test `tests/unit/vqec_vision_encoder_contract_test.cpp`, file_id `ectst`, prefix
`vqec_vision_ai_unit_ectst_`; main uses the language exception.

Encoder backend port: `include/vqec/vision/ai/contracts/vqec_vision_encoder_backend.hpp`,
file_id `encbk`, prefix `vqec_vision_ai_cntr_encbk_`. All backend overrides retain
these declaring-interface names. Internal C++ interface, not an exported binary ABI.

Output generation allocator: `src/core/vqec_vision_output_generation.cpp`, file_id
`otgen`, prefix `vqec_vision_ai_core_otgen_`; paired public contracts header.
Test `tests/unit/vqec_vision_output_generation_test.cpp`, file_id `ogent`, prefix
`vqec_vision_ai_unit_ogent_`; main retains its language exception.

Camera protocol constants: `src/adapters/camera/vqec_vision_camera_protocol.hpp`,
file_id `cmpro`, owner `camer`; constants only, no functions.

Header-only `include/vqec/vision/ai/contracts/vqec_vision_preview_limits.hpp`:
file_id `pvlim`, owner `cntr`; constants only, no named functions.

FW ring wrapper: `src/adapters/fw_output/vqec_vision_ring_sink.cpp`, file_id `rgsnk`,
prefix `vqec_vision_ai_fwout_rgsnk_`; paired private header uses the same stem.
Sink overrides retain `vqec_vision_ai_cntr_encsk_`. Optional test owner
`tests/contract/vqec_vision_ring_sink_test.cpp`, file_id `rstst`, prefix
`vqec_vision_ai_ctest_rstst_`; main retains the language exception.

Output dispatch: `src/outputs/vqec_vision_encoded_dispatch.cpp`, file_id `encdp`, prefix
`vqec_vision_ai_outpt_encdp_`; paired private header uses the same stem.
Test `tests/contract/vqec_vision_encoded_dispatch_test.cpp`, file_id `edtst`, prefix
`vqec_vision_ai_ctest_edtst_`; fake sink overrides retain `cntr_encsk` names.

Encoded output ownership: `src/core/vqec_vision_encoded_output.cpp`, file_id `encot`,
prefix `vqec_vision_ai_core_encot_`; paired public contracts header uses the same stem.
Header-only `include/vqec/vision/ai/contracts/vqec_vision_encoded_sink.hpp` owns
file_id `encsk`, prefix `vqec_vision_ai_cntr_encsk_` (overrides retain these names).
Test `tests/unit/vqec_vision_encoded_output_test.cpp`, file_id `eotst`, prefix
`vqec_vision_ai_unit_eotst_`; main retains the language exception.

Input composition: `src/app/vqec_vision_encoder_preparation.cpp`, file_id `enprp`,
prefix `vqec_vision_ai_appl_enprp_`; paired private header uses the same stem.
Test `tests/contract/vqec_vision_encoder_preparation_test.cpp`, file_id `eptst`,
prefix `vqec_vision_ai_ctest_eptst_`; main retains the language exception.

Preview pool: `src/core/vqec_vision_preview_pool.cpp`, file_id `pvpol`, prefix
`vqec_vision_ai_core_pvpol_`; paired public contracts header uses the same stem.
Test owner `tests/unit/vqec_vision_preview_pool_test.cpp`, file_id `pptst`, prefix
`vqec_vision_ai_unit_pptst_`; main is the language exception.

Encoder admission: `src/core/vqec_vision_encoder_window.cpp`, file_id `encwn`, prefix
`vqec_vision_ai_core_encwn_`; paired public contracts header uses the same stem.
Test owner `tests/unit/vqec_vision_encoder_window_test.cpp`, file_id `ewtst`, prefix
`vqec_vision_ai_unit_ewtst_`; main retains the language exception.

Preview surface: `src/core/vqec_vision_preview_surface.cpp`, file_id `pvsrf`, prefix
`vqec_vision_ai_core_pvsrf_`; paired public contracts header has the same filename stem.
Test owner `tests/unit/vqec_vision_preview_surface_test.cpp`, file_id `pstst`, prefix
`vqec_vision_ai_unit_pstst_`; main is the language exception.

Preview boundary additions: `src/core/vqec_vision_preview_contract.cpp` uses file_id
`pvctr`, prefix `vqec_vision_ai_core_pvctr_`; paired public header is
`include/vqec/vision/ai/contracts/vqec_vision_preview_contract.hpp`.
`tests/unit/vqec_vision_preview_contract_test.cpp` uses file_id `pvtst`, prefix
`vqec_vision_ai_unit_pvtst_` (main retains the language exception).

| Logical owner path | file_id | Prefix |
|---|---|---|
| src/core/vqec_vision_inference_plan.cpp | infpl | vqec_vision_ai_core_infpl_ |
| src/core/vqec_vision_submission_window.cpp | subwn | vqec_vision_ai_core_subwn_ |
| src/core/vqec_vision_source_binding.cpp | srcbd | vqec_vision_ai_core_srcbd_ |
| src/core/vqec_vision_tensor_contract.cpp | tnctr | vqec_vision_ai_core_tnctr_ |
| src/core/vqec_vision_output_gate.cpp | otgat | vqec_vision_ai_core_otgat_ |
| tests/unit/vqec_vision_output_gate_test.cpp | ogtst | vqec_vision_ai_unit_ogtst_ |
| src/runtime/model_registry/vqec_vision_output_manifest.cpp | otman | vqec_vision_ai_mreg_otman_ |
| src/runtime/model_registry/vqec_vision_artifact_digest.cpp | ardgt | vqec_vision_ai_mreg_ardgt_ |
| tests/unit/vqec_vision_artifact_digest_test.cpp | adtst | vqec_vision_ai_unit_adtst_ |
| tests/unit/vqec_vision_output_manifest_test.cpp | omtst | vqec_vision_ai_unit_omtst_ |
| tools/vqec_vision_manifest_check.cpp | mnchk | vqec_vision_ai_tools_mnchk_ |
| tests/unit/vqec_vision_tensor_contract_test.cpp | tctst | vqec_vision_ai_unit_tctst_ |
| src/app/vqec_vision_camera_graph_pump.cpp | cgpmp | vqec_vision_ai_appl_cgpmp_ |
| src/app/vqec_vision_camera_session.cpp | camsn | vqec_vision_ai_appl_camsn_ |
| src/app/vqec_vision_source_session.hpp | srcsn | vqec_vision_ai_appl_srcsn_ |
| src/app/vqec_vision_multi_source_supervisor.cpp | mssup | vqec_vision_ai_appl_mssup_ |
| tests/unit/vqec_vision_multi_source_supervisor_test.cpp | mstst | vqec_vision_ai_unit_mstst_ |
| src/runtime/scheduler/vqec_vision_model_cadence.cpp | mdcad | vqec_vision_ai_sched_mdcad_ |
| tests/unit/vqec_vision_model_cadence_test.cpp | mctst | vqec_vision_ai_unit_mctst_ |
| tests/contract/vqec_vision_camera_session_test.cpp | cstst | vqec_vision_ai_ctest_cstst_ |
| tests/contract/vqec_vision_camera_graph_pump_test.cpp | cgtst | vqec_vision_ai_ctest_cgtst_ |
| tests/unit/vqec_vision_source_binding_test.cpp | sbtst | vqec_vision_ai_unit_sbtst_ |
| tests/unit/vqec_vision_submission_window_test.cpp | swtst | vqec_vision_ai_unit_swtst_ |
| src/adapters/qualcomm/vqec_vision_plugin_graph.cpp | plgr | vqec_vision_ai_qcom_plgr_ |
| tests/unit/vqec_vision_inference_plan_test.cpp | iptst | vqec_vision_ai_unit_iptst_ |

Header contracts/vqec_vision_inference_plan.hpp declarations use implementation owner core_infpl.
types/vqec_vision_status.hpp is not introduced: status types live in contracts/vqec_vision_status.hpp and
contain no named functions. tests/unit/vqec_vision_inference_plan_test.cpp main is a C++ test
entrypoint exception, approved by the baseline language rule.

## Exception register

Tool owner: `tools/vqec_vision_check_source_layout.ps1`, file_id `chlay`, prefix
`vqec_vision_ai_tools_chlay_` reserved for future named helpers. Current script uses
only top-level control flow; PowerShell pipeline automatic variables retain shell syntax.
No AST naming enforcement is implied by the structural filename checker.

### Camera transport additions (2026-09-06)

| Logical owner path | file_id | Prefix |
|---|---|---|
| src/adapters/camera/vqec_vision_legacy_wire.cpp | lwire | vqec_vision_ai_camer_lwire_ |
| src/adapters/camera/vqec_vision_frame_source.cpp | frsrc | vqec_vision_ai_camer_frsrc_ |
| tests/unit/vqec_vision_legacy_wire_test.cpp | lwtst | vqec_vision_ai_unit_lwtst_ |
| tests/contract/vqec_vision_camera_receiver_test.cpp | crtst | vqec_vision_ai_ctest_crtst_ |
| src/adapters/camera/vqec_vision_camera_control.cpp | cctrl | vqec_vision_ai_camer_cctrl_ |
| src/adapters/camera/vqec_vision_camera_rpc.hpp | cmrpc | vqec_vision_ai_camer_cmrpc_ |
| src/adapters/camera/vqec_vision_dbus_rpc.cpp | dbrpc | vqec_vision_ai_camer_dbrpc_ |
| src/adapters/camera/vqec_vision_source_lifecycle.cpp | srclc | vqec_vision_ai_camer_srclc_ |
| src/adapters/camera/vqec_vision_raw_source_resolver.cpp | rsrsv | vqec_vision_ai_camer_rsrsv_ |
| include/vqec/vision/ai/ports/vqec_vision_raw_source.hpp | rawsr | vqec_vision_ai_ports_rawsr_ |
| src/adapters/qualcomm/vqec_vision_dmabuf_bridge.cpp | dmbrg | vqec_vision_ai_qcom_dmbrg_ |
| src/adapters/qualcomm/vqec_vision_tensor_output.cpp | tnout | vqec_vision_ai_qcom_tnout_ |
| src/adapters/qualcomm/vqec_vision_frame_submission.cpp | frsub | vqec_vision_ai_qcom_frsub_ |
| tests/contract/vqec_vision_frame_submission_test.cpp | fstst | vqec_vision_ai_ctest_fstst_ |
| tests/contract/vqec_vision_tensor_output_test.cpp | totst | vqec_vision_ai_ctest_totst_ |
| tests/contract/vqec_vision_dmabuf_bridge_test.cpp | dbtst | vqec_vision_ai_ctest_dbtst_ |
| tests/contract/vqec_vision_plugin_graph_test.cpp | pgtst | vqec_vision_ai_ctest_pgtst_ |
| tests/contract/vqec_vision_graph_lifecycle_test.cpp | gltst | vqec_vision_ai_ctest_gltst_ |
| tests/unit/vqec_vision_camera_control_test.cpp | cctst | vqec_vision_ai_unit_cctst_ |
| tests/unit/vqec_vision_raw_source_resolver_test.cpp | rsrst | vqec_vision_ai_unit_rsrst_ |

vqec_vision_frame_source.cpp activates the reserved camer/frsrc owner. The received_frame
accessors share that implementation owner. Tests' main uses the language exception.


| Scope | Symbol | Reason | Approval |
|---|---|---|---|
| src/app/vqec_vision_main.cpp (planned) | main | C++ entrypoint | baseline |
| Per-class special members | constructor/destructor/operator | C++ language requirement | baseline |
| Interface overrides | exact declaring interface symbol | C++ override identity | baseline |

External callbacks chỉ được miễn prefix nếu framework lookup tên cố định;
callback function pointer tự đặt tên KHÔNG được miễn.
Không có ngoại lệ mutable global hoặc production OpenCV.
