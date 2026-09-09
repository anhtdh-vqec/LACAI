# Qualcomm plugin backend — current source boundary

User-supplied target: QSC6490 / Qualcomm Linux 1.8. No board qualification yet.
Read docs/research/qualcomm_plugins_reference.md and ADR 0002.

## Source delivered, not built/tested

- vqec_vision_plugin_graph.cpp: runtime factory probing, factory/property validation, explicit FastCV/QNN
  configuration, READY source binding, PLAYING, bounded submission and result polling,
  independent input/result completion, EOS/drain and unload guards.
- vqec_vision_inference_graph.cpp: vendor-neutral inference_graph_port adapter; owns the
  retention reference and is the only app-facing conversion from native handle to Linux FD.
- One outstanding job per graph; internal ticket PTS is separate from source timestamp.
  Armed destruction retains resources in a four-slot supervisor domain. Restoration
  supports late completion, not cancellation or BSP recovery.
- vqec_vision_frame_submission.cpp: reserve/wrap/commit/push primitive used by the graph.
- vqec_vision_dmabuf_bridge.cpp: read-only FD memory, original plane layout and root-memory
  owner retention. Final memory release requires the downstream lifetime contract;
  it does not independently prove hardware synchronization. Its exact geometry/allocation
  ceiling can be composed from a validated deployment source.
- vqec_vision_tensor_output.cpp: bounded ordered FLOAT32 extraction and shape checks.
- Test source covers synthetic lifecycle/ownership/state cases, without board execution.

## Missing and constrained

No runnable service, concrete decoder/tracker, preview renderer/encoder or integrated
FW ring runtime. A guarded FW ring sink exists in a separate optional adapter target.
No native multi-dtype/multi-graph QNN, batch/temporal/ROI scheduler or signed-grant runtime.
Input is single linear NV12 image; model input NHWC RGB/BGR UINT8/FLOAT32 within plan
limits. Golden preprocessing, SDK interoperability and performance remain unverified.
Successful graph assembly is not inference qualification.

AI APP owns preview overlay/encode in a separate output adapter; FW owns capture,
RTSP/UI and recording. See docs/contracts/fw_release_compatibility.md. Do not add
camera capture or preview encoding into this inference graph.

Build options VQEC_VISION_AI_ENABLE_QUALCOMM and VQEC_VISION_AI_ENABLE_GST_FRAME_BRIDGE
are available. The latter enables standard GStreamer fixtures without Qualcomm models.
See docs/architecture/qualcomm_submission_lifecycle.md and dmabuf_memory_bridge.md
for drain/retention limitations. No claim of bounded vendor teardown or zero-copy.
