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
| src/adapters/zvec | zvec |
| src/adapters/camera | camer |
| src/adapters/qualcomm | qcom |
| src/adapters/reference | refer |
| src/adapters/fw_control | fwctl |
| src/adapters/fw_output | fwout |
| src/adapters/rockchip | rchip |
| src/adapters/mediatek | mtek |
| src/adapters/novatek | ntek |
| src/outputs | outpt |
| src/outputs/vqec_vision_overlay_preparation.cpp | ovrpr | vqec_vision_ai_outpt_ovrpr_ |
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
| src/adapters/qualcomm/vqec_vision_fastcv_aligner.cpp | fcaln | vqec_vision_ai_qcom_fcaln_ (port overrides retain vqec_vision_ai_ports_imaln_) |
| src/adapters/qualcomm/vqec_vision_qtiv_color.cpp | qtcol | vqec_vision_ai_qcom_qtcol_ |
| tools/vqec_vision_qtiv_color_smoke.cpp | qtcsm | vqec_vision_ai_tools_qtcsm_ |
| src/adapters/qualcomm/vqec_vision_buffer_manager.cpp | bufmg | vqec_vision_ai_qcom_bufmg_ |
| src/adapters/qualcomm/vqec_vision_backend_factory.cpp | bfact | vqec_vision_ai_qcom_bfact_ |
| src/adapters/qualcomm/vqec_vision_sdk_loader.cpp | sdkld | vqec_vision_ai_qcom_sdkld_ |
| src/adapters/qualcomm/vqec_vision_qnn_inference_graph.cpp | qnig | vqec_vision_ai_qcom_qnig_ |
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

Embedding contract: `include/vqec/vision/ai/contracts/vqec_vision_embedding.hpp` and
`src/core/vqec_vision_embedding.cpp`, file_id `embct`, prefix
`vqec_vision_ai_core_embct_`. Test `tests/unit/vqec_vision_embedding_test.cpp`, file_id
`embtst`; main retains the language exception.

Embedding index port: `include/vqec/vision/ai/ports/vqec_vision_embedding_index.hpp`
declares interface prefix `vqec_vision_ai_ports_emidx_`. Exact backend
`src/perception/embedding/vqec_vision_exact_embedding_index.cpp` uses file_id `exidx`,
prefix `vqec_vision_ai_embed_exidx_`. Test
`tests/unit/vqec_vision_exact_embedding_index_test.cpp` uses file_id `exitst`, prefix
`vqec_vision_ai_unit_exitst_`; `main` retains the language exception.
Optional Zvec adapter `src/adapters/zvec/vqec_vision_zvec_embedding_index.cpp` uses
file_id `zvidx`, prefix `vqec_vision_ai_zvec_zvidx_`.

Recognition policy contract: `include/vqec/vision/ai/contracts/vqec_vision_recognition.hpp`.
Implementation `src/perception/embedding/vqec_vision_recognition_policy.cpp` uses file_id
`rcpol`, prefix `vqec_vision_ai_embed_rcpol_`. Test
`tests/unit/vqec_vision_recognition_policy_test.cpp` uses file_id `rcptst`, prefix
`vqec_vision_ai_unit_rcptst_`; `main` retains the language exception.

Recognition session owner `src/perception/embedding/vqec_vision_recognition_session.cpp`
uses file_id `rcses`, prefix `vqec_vision_ai_embed_rcses_`. It owns bounded gallery
template metadata, revision-CAS mutations and recognition label correlation while
remaining independent of Zvec. Test `tests/unit/vqec_vision_recognition_session_test.cpp`
uses file_id `rcstst`; `main` retains the language exception.

Enrollment port `include/vqec/vision/ai/ports/vqec_vision_face_enrollment.hpp` uses
file_id `fenrl` and interface prefix `vqec_vision_ai_ports_fenrl_`. Its in-process
controller `src/perception/embedding/vqec_vision_face_enrollment_controller.cpp` uses
file_id `fenrc`, prefix `vqec_vision_ai_embed_fenrc_`; DBus adapters must remain behind
this port.

Embedding decoder port: `include/vqec/vision/ai/ports/vqec_vision_embedding_decoder.hpp`
declares interface prefix `vqec_vision_ai_ports_embdc_`. Generic implementation
`src/perception/embedding/vqec_vision_embedding_decoder.cpp` uses file_id `embdd`, prefix
`vqec_vision_ai_embed_embdd_`. Test `tests/unit/vqec_vision_embedding_decoder_test.cpp` uses
file_id `edtst`, prefix `vqec_vision_ai_unit_edtst_`.

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

Header-only `include/vqec/vision/ai/contracts/vqec_vision_identifier.hpp`:
file_id `ident`, owner `cntr`, prefix `vqec_vision_ai_cntr_ident_`. Shared bounded
ASCII identifier check; callers keep their own per-contract byte ceiling. This is the
single source of truth for the accepted character set so validators cannot diverge.

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
| src/core/vqec_vision_inference_execution.cpp | inexe | vqec_vision_ai_core_inexe_ |
| src/core/vqec_vision_tensor_pool.cpp | tnpl | vqec_vision_ai_core_tnpl_ |
| src/core/vqec_vision_secondary_inference.cpp | secin | vqec_vision_ai_core_secin_ |
| include/vqec/vision/ai/contracts/vqec_vision_image_alignment.hpp | imaln | (contract data only; no functions) |
| src/core/vqec_vision_image_alignment.cpp | imaln | vqec_vision_ai_core_imaln_ |
| include/vqec/vision/ai/contracts/vqec_vision_color.hpp | color | (contract declaration; implemented in core) |
| src/core/vqec_vision_color.cpp | color | vqec_vision_ai_core_color_ |
| tests/unit/vqec_vision_color_test.cpp | coltst | vqec_vision_ai_unit_coltst_ |
| include/vqec/vision/ai/ports/vqec_vision_image_alignment.hpp | imaln | vqec_vision_ai_ports_imaln_ |
| tests/unit/vqec_vision_image_alignment_test.cpp | imtst | vqec_vision_ai_unit_imtst_ |
| src/core/vqec_vision_preprocess_spec.cpp | ppspc | vqec_vision_ai_core_ppspc_ |
| src/core/vqec_vision_model_io_manifest.cpp | ioman | vqec_vision_ai_core_ioman_ |
| src/core/vqec_vision_model_package.cpp | mpkg | vqec_vision_ai_core_mpkg_ |
| src/core/vqec_vision_model_package_registry.cpp | mprgy | vqec_vision_ai_core_mprgy_ |
| src/runtime/model_registry/vqec_vision_model_package_registry.cpp | mprld | vqec_vision_ai_mreg_mprld_ |
| include/vqec/vision/ai/contracts/vqec_vision_decoder_package.hpp | dcpkg | (contract data only; no functions) |
| src/runtime/model_registry/vqec_vision_decoder_package.cpp | dcpkg | vqec_vision_ai_mreg_dcpkg_ |
| tests/unit/vqec_vision_decoder_package_test.cpp | dctst | vqec_vision_ai_unit_dctst_ |
| tests/unit/vqec_vision_preprocess_spec_test.cpp | ppst | vqec_vision_ai_unit_ppst_ |
| tests/unit/vqec_vision_model_io_manifest_test.cpp | iomtst | vqec_vision_ai_unit_iomtst_ |
| tests/unit/vqec_vision_model_package_test.cpp | mpktst | vqec_vision_ai_unit_mpktst_ |
| tests/unit/vqec_vision_model_package_registry_test.cpp | mprtst | vqec_vision_ai_unit_mprtst_ |
| src/runtime/scheduler/vqec_vision_secondary_inference_scheduler.cpp | secsd | vqec_vision_ai_sched_secsd_ |
| tests/unit/vqec_vision_secondary_inference_test.cpp | sitst | vqec_vision_ai_unit_sitst_ |
| tests/unit/vqec_vision_lifecycle_invariants_test.cpp | litst | vqec_vision_ai_unit_litst_ |
| tests/unit/vqec_vision_inference_execution_test.cpp | inxtst | vqec_vision_ai_unit_inxtst_ |
| tests/unit/vqec_vision_tensor_pool_test.cpp | tptst | vqec_vision_ai_unit_tptst_ |
| src/core/vqec_vision_output_gate.cpp | otgat | vqec_vision_ai_core_otgat_ |
| src/core/vqec_vision_feature_event.cpp | ftevt | vqec_vision_ai_core_ftevt_ |
| src/core/vqec_vision_feature_catalog.cpp | ftcat | vqec_vision_ai_core_ftcat_ |
| tests/unit/vqec_vision_feature_catalog_test.cpp | fctst | vqec_vision_ai_unit_fctst_ |
| src/outputs/vqec_vision_feature_event_dispatch.cpp | ftdsp | vqec_vision_ai_outpt_ftdsp_ |
| tests/unit/vqec_vision_output_gate_test.cpp | ogtst | vqec_vision_ai_unit_ogtst_ |
| src/runtime/model_registry/vqec_vision_output_manifest.cpp | otman | vqec_vision_ai_mreg_otman_ |
| src/runtime/model_registry/vqec_vision_artifact_digest.cpp | ardgt | vqec_vision_ai_mreg_ardgt_ |
| src/runtime/model_registry/vqec_vision_artifact_resolver.cpp | artsr | vqec_vision_ai_mreg_artsr_ |
| tests/contract/vqec_vision_artifact_resolver_test.cpp | arsct | vqec_vision_ai_ctest_arsct_ |
| tests/unit/vqec_vision_artifact_digest_test.cpp | adtst | vqec_vision_ai_unit_adtst_ |
| tests/unit/vqec_vision_output_manifest_test.cpp | omtst | vqec_vision_ai_unit_omtst_ |
| tools/vqec_vision_manifest_check.cpp | mnchk | vqec_vision_ai_tools_mnchk_ |
| tools/vqec_vision_qnn_engine_smoke.cpp | qnsmk | vqec_vision_ai_tools_qnsmk_ |
| tools/vqec_vision_fastcv_affine_smoke.cpp | fasmy | vqec_vision_ai_tools_fasmy_ |
| tools/vqec_vision_model_runner.cpp | mdlrun | vqec_vision_ai_tools_mdlrun_ |
| tools/vqec_vision_board_deploy.sh | bdep | vqec_vision_ai_tools_bdep_ |
| tools/vqec_vision_fw_camera_sim.py | fwsim | vqec_vision_ai_tools_fwsim_ |
| tools/vqec_vision_ring_rtsp.py | rrtsp | vqec_vision_ai_tools_rrtsp_ |
| tests/unit/vqec_vision_tensor_contract_test.cpp | tctst | vqec_vision_ai_unit_tctst_ |
| src/app/vqec_vision_camera_graph_pump.cpp | cgpmp | vqec_vision_ai_appl_cgpmp_ |
| src/app/vqec_vision_multi_model_pump.cpp | mmump | vqec_vision_ai_appl_mmump_ |
| src/app/vqec_vision_cascade_coordinator.cpp | cscrd | vqec_vision_ai_appl_cscrd_ |
| tests/unit/vqec_vision_cascade_coordinator_test.cpp | cctst | vqec_vision_ai_unit_cctst_ |
| src/app/vqec_vision_cascade_graph_session.cpp | cgses | vqec_vision_ai_appl_cgses_ |
| tests/unit/vqec_vision_cascade_graph_session_test.cpp | cgsts | vqec_vision_ai_unit_cgsts_ |
| src/app/vqec_vision_multi_model_session.cpp | mmses | vqec_vision_ai_appl_mmses_ |
| src/app/vqec_vision_camera_session.cpp | camsn | vqec_vision_ai_appl_camsn_ |
| src/app/vqec_vision_source_session.hpp | srcsn | vqec_vision_ai_appl_srcsn_ |
| src/app/vqec_vision_multi_source_supervisor.cpp | mssup | vqec_vision_ai_appl_mssup_ |
| tests/unit/vqec_vision_multi_source_supervisor_test.cpp | mstst | vqec_vision_ai_unit_mstst_ |
| tests/unit/vqec_vision_multi_source_supervisor_async_test.cpp | msast | vqec_vision_ai_unit_msast_ |
| src/runtime/scheduler/vqec_vision_model_cadence.cpp | mdcad | vqec_vision_ai_sched_mdcad_ |
| src/runtime/scheduler/vqec_vision_inference_worker.cpp | inwrk | vqec_vision_ai_sched_inwrk_ |
| tests/unit/vqec_vision_inference_worker_test.cpp | iwtst | vqec_vision_ai_unit_iwtst_ |
| src/runtime/lifecycle/vqec_vision_recovery_controller.cpp | rcvr | vqec_vision_ai_life_rcvr_ |
| tests/unit/vqec_vision_recovery_controller_test.cpp | rctst | vqec_vision_ai_unit_rctst_ |
| tests/unit/vqec_vision_model_cadence_test.cpp | mctst | vqec_vision_ai_unit_mctst_ |
| tests/unit/vqec_vision_multi_model_pump_test.cpp | mmpst | vqec_vision_ai_unit_mmpst_ |
| tests/unit/vqec_vision_multi_model_pump_tensor_test.cpp | mptst | vqec_vision_ai_unit_mptst_ |
| tests/unit/vqec_vision_multi_model_pump_qos_test.cpp | mqst | vqec_vision_ai_unit_mqst_ |
| tests/unit/vqec_vision_multi_model_session_test.cpp | mmsts | vqec_vision_ai_unit_mmsts_ |
| tests/contract/vqec_vision_camera_session_test.cpp | cstst | vqec_vision_ai_ctest_cstst_ |
| tests/contract/vqec_vision_camera_graph_pump_test.cpp | cgtst | vqec_vision_ai_ctest_cgtst_ |
| tests/unit/vqec_vision_source_binding_test.cpp | sbtst | vqec_vision_ai_unit_sbtst_ |
| tests/unit/vqec_vision_submission_window_test.cpp | swtst | vqec_vision_ai_unit_swtst_ |
| src/adapters/qualcomm/vqec_vision_plugin_graph.cpp | plgr | vqec_vision_ai_qcom_plgr_ |
| src/perception/detection/vqec_vision_model_decode_stage.cpp | mdstg | vqec_vision_ai_detec_mdstg_ |
| src/perception/detection/vqec_vision_model_decoder_registry.cpp | mdreg | vqec_vision_ai_detec_mdreg_ |
| src/perception/detection/vqec_vision_tensor_reader.cpp | tnrd | vqec_vision_ai_detec_tnrd_ |
| src/perception/detection/vqec_vision_dense_decoder.cpp | dnsdc | vqec_vision_ai_detec_dnsdc_ |
| src/perception/detection/vqec_vision_yolov8_decoder.cpp | y8dec | vqec_vision_ai_detec_y8dec_ |
| tests/unit/vqec_vision_dense_decoder_test.cpp | ddtst | vqec_vision_ai_unit_ddtst_ |
| tests/unit/vqec_vision_yolov8_decoder_test.cpp | y8tst | vqec_vision_ai_unit_y8tst_ |
| tests/unit/vqec_vision_device_free_harness_test.cpp | dfhst | vqec_vision_ai_unit_dfhst_ |
| tests/fuzz/vqec_vision_legacy_wire_fuzz.cpp | lwfz | vqec_vision_ai_unit_lwfz_ (LLVMFuzzerTestOneInput is a fixed framework entrypoint) |
| tests/fuzz/vqec_vision_manifest_fuzz.cpp | mnfz | vqec_vision_ai_unit_mnfz_ (LLVMFuzzerTestOneInput is a fixed framework entrypoint) |
| tests/fuzz/vqec_vision_dense_decoder_fuzz.cpp | ddfz | vqec_vision_ai_unit_ddfz_ (LLVMFuzzerTestOneInput is a fixed framework entrypoint) |
| tests/fuzz/vqec_vision_model_catalog_fuzz.cpp | mcfz | vqec_vision_ai_unit_mcfz_ (LLVMFuzzerTestOneInput is a fixed framework entrypoint) |
| src/perception/tracking/vqec_vision_tracking_stage.cpp | trkst | vqec_vision_ai_track_trkst_ |
| src/perception/tracking/vqec_vision_tracker_registry.cpp | trreg | vqec_vision_ai_track_trreg_ |
| src/perception/attributes/vqec_vision_attribute_reader.cpp | atrdr | vqec_vision_ai_attr_atrdr_ |
| src/runtime/feature_manager/vqec_vision_feature_stage.cpp | ftstg | vqec_vision_ai_ftmgr_ftstg_ |
| src/runtime/feature_manager/vqec_vision_feature_activation_manager.cpp | famgr | vqec_vision_ai_ftmgr_famgr_ |
| tests/unit/vqec_vision_inference_plan_test.cpp | iptst | vqec_vision_ai_unit_iptst_ |
| include/vqec/vision/ai/contracts/vqec_vision_application_composition.hpp | acomp | vqec_vision_ai_cntr_acomp_ |
| include/vqec/vision/ai/contracts/vqec_vision_model_decoder.hpp | mddec | vqec_vision_ai_cntr_mddec_ |
| tests/contract/vqec_vision_model_decoder_test.cpp | mdtst | vqec_vision_ai_ctest_mdtst_ |
| tests/contract/vqec_vision_model_decode_stage_test.cpp | mdsct | vqec_vision_ai_ctest_mdsct_ |
| tests/contract/vqec_vision_model_decoder_registry_test.cpp | mdrct | vqec_vision_ai_ctest_mdrct_ |
| tests/contract/vqec_vision_tensor_reader_test.cpp | trct | vqec_vision_ai_ctest_trct_ |
| tests/contract/vqec_vision_tracker_port_test.cpp | trpct | vqec_vision_ai_ctest_trpct_ |
| tests/contract/vqec_vision_tracking_stage_test.cpp | tstgt | vqec_vision_ai_ctest_tstgt_ |
| tests/contract/vqec_vision_tracker_registry_test.cpp | trrct | vqec_vision_ai_ctest_trrct_ |
| tests/contract/vqec_vision_feature_activation_manager_test.cpp | famct | vqec_vision_ai_ctest_famct_ |
| tests/contract/vqec_vision_attribute_reader_test.cpp | atrct | vqec_vision_ai_ctest_atrct_ |
| tests/unit/vqec_vision_feature_event_test.cpp | fetst | vqec_vision_ai_unit_fetst_ |
| include/vqec/vision/ai/ports/vqec_vision_feature_processor.hpp | ftpro | vqec_vision_ai_ports_ftpro_ |
| include/vqec/vision/ai/ports/vqec_vision_feature_processor_factory.hpp | ftfac | vqec_vision_ai_ports_ftfac_ |
| src/runtime/feature_manager/vqec_vision_feature_processor_registry.cpp | ftreg | vqec_vision_ai_ftmgr_ftreg_ |
| src/runtime/feature_manager/vqec_vision_feature_catalog.cpp | ftcat | vqec_vision_ai_ftmgr_ftcat_ |
| include/vqec/vision/ai/ports/vqec_vision_feature_event_sink.hpp | fesnk | vqec_vision_ai_ports_fesnk_ |
| tests/contract/vqec_vision_feature_processor_test.cpp | fpct | vqec_vision_ai_ctest_fpct_ |
| tests/contract/vqec_vision_feature_processor_registry_test.cpp | fprct | vqec_vision_ai_ctest_fprct_ |
| tests/unit/vqec_vision_feature_catalog_loader_test.cpp | fclt | vqec_vision_ai_unit_fclt_ |
| tests/contract/vqec_vision_feature_event_dispatch_test.cpp | fedct | vqec_vision_ai_ctest_fedct_ |
| tests/contract/vqec_vision_feature_stage_test.cpp | fsct | vqec_vision_ai_ctest_fsct_ |
| tests/contract/vqec_vision_overlay_preparation_test.cpp | oprct | vqec_vision_ai_ctest_oprct_ |
| src/app/vqec_vision_application_composition.cpp | acomp | vqec_vision_ai_appl_acomp_ |
| src/app/vqec_vision_perception_result_stage.cpp | prstg | vqec_vision_ai_appl_prstg_ |
| src/app/vqec_vision_perception_stage_factory.cpp | prfac | vqec_vision_ai_appl_prfac_ |
| src/app/vqec_vision_feature_fanout.cpp | ftfan | vqec_vision_ai_appl_ftfan_ |
| src/app/vqec_vision_multi_model_result_router.cpp | mmrrt | vqec_vision_ai_appl_mmrrt_ |
| src/app/vqec_vision_multi_model_feature_pipeline.cpp | mmfpl | vqec_vision_ai_appl_mmfpl_ |
| tests/contract/vqec_vision_application_composition_test.cpp | actst | vqec_vision_ai_ctest_actst_ |
| tests/contract/vqec_vision_perception_result_stage_test.cpp | prct | vqec_vision_ai_ctest_prct_ |
| tests/contract/vqec_vision_perception_stage_factory_test.cpp | psfct | vqec_vision_ai_ctest_psfct_ |
| tests/contract/vqec_vision_feature_fanout_test.cpp | ffct | vqec_vision_ai_ctest_ffct_ |
| tests/contract/vqec_vision_multi_model_result_router_test.cpp | mrrct | vqec_vision_ai_ctest_mrrct_ |
| tests/contract/vqec_vision_multi_model_feature_pipeline_test.cpp | mfpct | vqec_vision_ai_ctest_mfpct_ |

Header contracts/vqec_vision_inference_plan.hpp declarations use implementation owner core_infpl.
types/vqec_vision_status.hpp is not introduced: status types live in contracts/vqec_vision_status.hpp and
contain no named functions. tests/unit/vqec_vision_inference_plan_test.cpp main is a C++ test
entrypoint exception, approved by the baseline language rule.

## Exception register

Tool owner: `tools/vqec_vision_check_source_layout.ps1`, file_id `chlay`, prefix
`vqec_vision_ai_tools_chlay_` reserved for future named helpers. Current script uses
only top-level control flow; PowerShell pipeline automatic variables retain shell syntax.
Portable Linux/CI counterpart `tools/vqec_vision_check_source_layout.sh` shares the same
logical owner and performs the same read-only checks. No AST naming enforcement is
implied by the structural filename checker.

Board-side helper `tools/vqec_vision_qnn_board_smoke.sh` runs `qnn-net-run` for one model
library against a pinned QAIRT runtime; it writes only the output directory and does not
modify the repository.

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
| src/adapters/reference/vqec_vision_reference_source.cpp | rfsrc | vqec_vision_ai_refer_rfsrc_ |
| src/adapters/reference/vqec_vision_reference_graph.cpp | rfgph | vqec_vision_ai_refer_rfgph_ |
| src/adapters/reference/vqec_vision_reference_sink.cpp | rfsnk | vqec_vision_ai_refer_rfsnk_ |
| src/adapters/reference/vqec_vision_reference_processor.cpp | rfprc | vqec_vision_ai_refer_rfprc_ |
| src/adapters/reference/vqec_vision_reference_tracker.cpp | rftrk | vqec_vision_ai_refer_rftrk_ |
| src/adapters/reference/vqec_vision_reference_feature.cpp | rfeat | vqec_vision_ai_refer_rfeat_ |
| tests/unit/vqec_vision_reference_processor_test.cpp | rptst | vqec_vision_ai_unit_rptst_ |
| tests/unit/vqec_vision_reference_tracker_test.cpp | rttst | vqec_vision_ai_unit_rttst_ |
| tests/unit/vqec_vision_reference_feature_test.cpp | rftst | vqec_vision_ai_unit_rftst_ |
| tests/unit/vqec_vision_preprocess_conformance_test.cpp | pctst | vqec_vision_ai_unit_pctst_ |
| src/adapters/reference/vqec_vision_reference_encoder.cpp | renc | vqec_vision_ai_refer_renc_ |
| src/adapters/reference/vqec_vision_reference_ring_sink.cpp | rring | vqec_vision_ai_refer_rring_ |
| tests/unit/vqec_vision_reference_output_test.cpp | routt | vqec_vision_ai_unit_routt_ |
| include/vqec/vision/ai/ports/vqec_vision_raw_source.hpp | rawsr | vqec_vision_ai_ports_rawsr_ |
| include/vqec/vision/ai/ports/vqec_vision_cascade_frame_lease.hpp | cflse | vqec_vision_ai_ports_cflse_ |
| include/vqec/vision/ai/ports/vqec_vision_inference_graph.hpp | infgr | vqec_vision_ai_ports_infgr_ |
| include/vqec/vision/ai/ports/vqec_vision_tracker.hpp | trker | vqec_vision_ai_ports_trker_ |
| src/adapters/qualcomm/vqec_vision_inference_graph.cpp | ifgr | vqec_vision_ai_qcom_ifgr_ |
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

Source perception factory registrations (source delivered):

| Logical owner path | file_id | Prefix |
|---|---|---|
| src/app/vqec_vision_source_perception_factory.cpp | spfac | vqec_vision_ai_appl_spfac_ |
| tests/contract/vqec_vision_source_perception_factory_test.cpp | spfct | vqec_vision_ai_ctest_spfct_ |

Runtime composition factory registrations (source delivered):

| Logical owner path | file_id | Prefix |
|---|---|---|
| src/app/vqec_vision_runtime_composition_factory.cpp | rcfac | vqec_vision_ai_appl_rcfac_ |
| tests/contract/vqec_vision_runtime_composition_factory_test.cpp | rcfct | vqec_vision_ai_ctest_rcfct_ |
| src/app/vqec_vision_runtime_executor.cpp | rtexe | vqec_vision_ai_appl_rtexe_ |
| src/app/vqec_vision_service_main.cpp | svcmn | vqec_vision_ai_appl_svcmn_ |
| src/app/vqec_vision_fake_platform.cpp | fkplt | vqec_vision_ai_appl_fkplt_ |
| src/app/vqec_vision_reference_platform.cpp | rplat | vqec_vision_ai_appl_rplat_ |
| src/app/vqec_vision_fixture_detector.cpp | fxdet | vqec_vision_ai_appl_fxdet_ |
| src/app/vqec_vision_production_platform.cpp | pdplt | vqec_vision_ai_appl_pdplt_ |
| src/adapters/qualcomm/vqec_vision_qtiv_renderer.cpp | qtvr | vqec_vision_ai_qcom_qtvr_ |
| src/app/vqec_vision_source_session_worker.cpp | sswrk | vqec_vision_ai_appl_sswrk_ |
| tests/unit/vqec_vision_source_session_worker_test.cpp | sswtst | vqec_vision_ai_unit_sswtst_ |

Zvec integration test: `tests/unit/vqec_vision_zvec_embedding_index_test.cpp`,
file_id `zvitst`, prefix `vqec_vision_ai_unit_zvitst_`.

Zvec dependency bootstrap: `tools/vqec_vision_prepare_zvec.sh`; no named functions.
Release/version/URL/checksum constants belong to `third_party/zvec/dependency.json`.

Anchor-distance decoder: `src/perception/detection/vqec_vision_anchor_distance_decoder.cpp`,
file_id `addcd`, prefix `vqec_vision_ai_detec_addcd_`. Test
`tests/unit/vqec_vision_anchor_distance_decoder_test.cpp`, file_id `addtst`,
prefix `vqec_vision_ai_unit_addtst_`.

Cascade frame store: `src/runtime/scheduler/vqec_vision_cascade_frame_store.hpp`,
file_id `cfstr`, prefix `vqec_vision_ai_sched_cfstr_` (header-only).
Test `tests/unit/vqec_vision_cascade_frame_store_test.cpp`, file_id `cfst`.
