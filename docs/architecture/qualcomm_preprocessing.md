# Qualcomm preprocessing adapter

Status: source-delivered and measured on the QCS6490 `.48` integration target on
2026-09-15. This path is private to `src/adapters/qualcomm`; application composition
continues to depend only on `image_processor_port`.

## Runtime path

The production Qualcomm platform constructs one `fastcv_processor` for each admitted
model slot and binds it through the neutral image-processor port. It builds the reusable
pipeline below from the validated source profile, inference plan and graph input tensor:

```text
FW linear NV12 FD
  -> appsrc
  -> qtivtransform engine=fcv, destination=<validated letterbox rectangle>
  -> NV12 tensor-size caps
  -> qtimlvconverter engine=fcv, RGB/BGR, UINT8
  -> appsink
  -> AArch64 NEON UINT8-to-UFIXED16 packing when required by the graph
  -> owned tensor_blob
  -> QNN graph input
```

`qtivtransform` owns resize and letterbox placement so the pad color comes from the model
preprocess manifest. `qtimlvconverter` owns NV12-to-RGB/BGR conversion. The adapter selects
the `fcv` engine explicitly and fails closed when the installed properties, enum nicks or
requested semantics do not match. The current reviewed surface accepts linear NV12,
BT.601-limited or BT.709-limited input, bilinear stretch/letterbox, batch-one NHWC RGB/BGR
and the exact UINT8/UINT16 offset-scale quantization implemented by the pipeline.

The graph input used in this board run is UINT16. Asking `qtimlvconverter` to produce
UINT16 invokes its generic per-value normalization loop. LACAI therefore requests UINT8
from the plugin and packs each byte as the equivalent little-endian UINT16 value. On
AArch64 this packing uses NEON byte interleave; the scalar fallback keeps the adapter
source compilable for another architecture. This decision is derived from the tensor
contract and never from a model name.

## Ownership and remaining copies

The source FD remains owned through the shared `raw_frame` owner while GStreamer/FastCV
reads it. The adapter maps the plugin output, copies it into a bounded LACAI-owned tensor,
and only then returns. The synchronous QNN engine subsequently copies that tensor into its
client input buffer. These two copies remain measurable optimization targets. FD wrapping
and a synchronous plugin return do not prove released-FW DMA-BUF compatibility or an
end-to-end zero-copy path.

Changing vendor requires another `image_processor_port` implementation. No Gst, FastCV,
QNN or ARM SIMD type crosses the neutral contract, perception or application boundary.

## Board evidence and bottleneck progression

The controlled source was 1280x720 NV12 at 30 FPS, the tensor was 640x640x3 UINT16, and
the output-disabled runs used the same QNN HTP graph and 30/1 inference cadence. CPU is a
process-wide sample, while `perf` percentages are sampled CPU cycles; neither is a
per-stage wall-time percentage.

| Preprocess path | AI results in 10 s | Rate | Process CPU | Dominant sampled cost |
|---|---:|---:|---:|---|
| Portable reference CPU | 97 | 9.7 FPS | about 65% | 96.55% in reference preprocess |
| FastCV plus plugin UINT16 output | 176 | 17.6 FPS | about 64% | 81.74% in `gst_video_frame_normalize_ip` |
| FastCV UINT8 plus NEON UINT16 pack | 301 | 30.1 FPS | 38.8% | 24.31% FastCV color conversion; 5.00% adapter preprocess |
| Same optimized path plus overlay/encode/ring | 300 | 30.0 FPS | 45.4% | camera-paced full application |

The final RTSP client decoded 241 H.264 1280x720 frames in eight seconds (30.1 FPS).
Standalone owned-QNN measurements for this graph were previously 10.83/12.12/13.27 ms
minimum/average/maximum over 50 executions. In the final live profile, QNN-side client
buffer `memcpy` accounted for 8.34% of sampled CPU cycles and YOLO tensor element decode
for 2.78%. The FastCV DSP scale call was visible in the profile.

These measurements prove the current compatibility flow sustains the requested frame
rate. They do not yet provide percentile latency from released-FW capture to ring commit,
thermal/soak qualification, model accuracy, real camera DMA-BUF import, or multi-model
capacity. The compatibility camera copies QMMF pixels into memfd before the LACAI boundary.

A later steady-state sample after label and geometry fixes measured 44.5% process CPU and
exactly 30.0 encoded frames/s over five seconds. This remains outside the requested
15–25% CPU range. The standalone HTP probes measured SCRFD at 4.405 ms average over 20
executions and EdgeFace at 2.918 ms average over 50 executions, so adding model-specific
CPU postprocess or running all secondary crops without admission would work against the
CPU target. See [cascade inference](cascade_inference.md).

## Next optimization gates

1. Pin a released-FW timestamp clock and report capture-to-result and capture-to-ring
   p50/p95/p99 latency rather than inferring latency from throughput.
2. Register or import reusable QNN input memory to remove the owned-tensor-to-client-buffer
   copy, with completion-backed reuse and numeric parity evidence.
3. Replace compatibility memfd input with released-FW DMA-BUF and validate modifiers,
   cache synchronization, fences and completion before claiming zero-copy.
4. Run sustained thermal, concurrent model and concurrent source tests. Feed those
   measurements into admission instead of encoding board capacity in source constants.
