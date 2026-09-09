# Qualcomm plugin adapter reference

This document is the implementation reference for using the Qualcomm GStreamer
stack from LACAI. It records the source that was inspected and the boundary that
keeps that stack replaceable. It does not claim that a plugin is present on a
particular image or that a path is qualified on QSC6490.

## Source baseline

The source reviewed was `/home/a/Workspace/gst-plugins-qti-oss` at commit
`0cdf24a99c625fa616564ebf82fd8813c744ed82` (2026-09-09). The checkout was clean;
it is an external reference and must not be modified or copied into LACAI.
The target supplied by the product team is QSC6490 with Qualcomm Linux 1.8.
Runtime availability, ABI, enum values and caps are still established on the
target image with `gst-inspect-1.0` and a board smoke test.

The source evidence used for this reference is concentrated in:

| Plugin/source | Evidence used by LACAI |
|---|---|
| `gst-plugin-mlvconverter/mlvconverter.c` | `qtimlvconverter`, FCV conversion, image/ROI batch modes, disposition, channel order and normalization properties |
| `gst-plugin-mlqnn/mlqnn.c`, `ml-qnn-engine.cc` | `qtimlqnn` model/backend/system loading, tensor caps and synchronous graph execution |
| `gst-plugin-mlpostprocess/mlpostprocess.cc` and `modules/` | model-specific postprocess module loading and metadata output |
| `gst-plugin-mlmetaextractor`, `gst-plugin-mlmetaparser` | translation between ML metadata and text/region metadata |
| `gst-plugin-vtransform/videotransform.c` | hardware video conversion, crop/rotate/flip and destination properties |
| `gst-plugin-voverlay/overlay.c` | optional drawing of authorized metadata on an AI-owned output surface |
| `gst-plugin-qmmfsrc` | camera capture source; not part of the LACAI inference graph |
| `gst-plugin-smartvencbin/vencbin.c` and sample `v4l2h264enc` wiring | optional hardware encode path behind the output adapter |

## Required graph boundary

The production composition follows the LACAI ports and ownership model:

```text
FW raw_source_port
    -> Qualcomm frame/memory adapter
    -> appsrc
    -> qtimlvconverter (engine=fcv)
    -> tensor caps negotiated from model catalog
    -> qtimlqnn (model/backend/system selected by deployment)
    -> appsink
    -> model_decoder_port
    -> portable observations, tracking and features
```

Preview is a separate branch owned by AI APP:

```text
authorized observations + AI-owned surface
    -> optional qtivoverlay / qtivtransform adapter
    -> optional v4l2h264enc or qtismartvencbin adapter
    -> encoded_sink / released FW ring
```

`qtiqmmfsrc` is deliberately excluded from the inference graph because FW owns
camera capture, RTSP, demux/decode and recording in the release contract. The
capture plugin may be used only in a separately approved product where FW hands
that ownership to AI APP.

## Plugin use and adapter rules

| Factory | Use in LACAI | Adapter rule |
|---|---|---|
| `qtimlvconverter` | FastCV image-to-tensor preprocessing, including ROI batching when the model declares it | Configure only through a Qualcomm adapter. Select `engine=fcv` explicitly; do not inherit the plugin's automatic backend. `mode`, `image-disposition`, `subpixel-layout`, `mean` and `sigma` come from validated model/deployment metadata. Verify normalization with a golden tensor because the source scales coefficients internally. |
| `qtimlqnn` | QNN graph inference | Set `model`, `backend`, and `system` from the admitted deployment, never from plugin defaults. The inspected wrapper executes `graphExecute` synchronously and currently has wrapper-specific graph/output behavior; a worker deadline does not make the SDK cancellable. Keep all tensor identity, dtype, layout and quantization checks in LACAI contracts. |
| `qtimlpostprocess` | Optional vendor postprocess for a model whose module and output semantics are explicitly accepted | Use only behind a decoder/metadata adapter with a golden equivalence test. Module, labels, settings and stabilization are model data, not universal defaults. A factory existing does not make a YOLO, pose or classifier output compatible. |
| `qtimlmetaextractor` / `qtimlmetaparser` | Interop with legacy metadata pipelines | Translate at the adapter edge. `GstVideoRegionOfInterestMeta`, landmarks and classification metadata must never appear in neutral contracts or feature code. Preserve source id, PTS and geometry transforms. |
| `qtivtransform` | Hardware resize/format/crop/rotate for preview or a separately approved preprocessing path | Probe the actual `engine` enum and memory caps. Keep it out of the inference graph unless the model golden proves equivalent results. Do not assume a DMA-BUF is importable or that an engine implies zero-copy. |
| `qtivoverlay` | Optional overlay rendering | Render only authorized attributes onto a surface owned by AI APP. It is an output capability, not a substitute for observation authorization or FW UI/recording ownership. Preserve the source epoch and transform metadata. |
| `v4l2h264enc` / `qtismartvencbin` | Hardware H.264 output for the AI-owned preview branch | Wrap the installed encoder and negotiate caps at runtime. Sample code uses DMABUF modes, but that is not proof of zero-copy on the product BSP. Measure latency, copies and retention before claiming acceleration. |
| `qtiqmmfsrc` | Qualcomm camera capture | Not selected by LACAI under the FW release contract. Never reacquire one camera source per model. |

The plugin repository also contains specialized flow, tracker and codec elements.
They are optional adapters only after the model contract, ownership, licensing and
board evidence are agreed. Portable tracking and feature semantics remain in LACAI;
vendor elements must not become hidden schedulers or entitlement bypasses.

## Configuration is runtime evidence

The adapter must discover factories, properties, enum nicks, pad caps and mutability
on the target image. The source shows, among other properties, `engine`, `mode`,
`image-disposition`, `subpixel-layout`, `mean`, `sigma` on the converter and
`model`, `backend`, `system`, `backend-device-id`, `tensors` on QNN. These names are
not permission to hardcode paths or numeric enum values in LACAI. In particular,
the source default `/usr/lib/libQnnCpu.so` is not an HTP selection.

Set properties in NULL/READY according to the plugin's documented mutability, reject
missing or type-mismatched properties, and fail closed on unsupported enum/caps.
Deployment configuration and model catalog own artifact paths and tensor metadata;
the adapter only translates them to plugin values. Do not silently fall back from
FCV/HTP to CPU or OpenCV. A permitted degraded mode must be explicit and observable.

## Ownership, synchronization and shutdown

`appsrc` accepting a buffer means ownership was accepted by the pipeline, not that
the buffer is no longer read. The frame lease and root memory owner stay alive until
the adapter has evidence of downstream completion. A DMA-BUF FD, cache sync, GStreamer
buffer finalization or a timeout is not device completion. The existing LACAI
retention domain, submission window and drain protocol remain authoritative.

Use bounded appsrc/appsink admission around the plugin's internal pools. Keep the
input read-only; overlay and encode use a separate writable AI-owned branch. On stop,
stop admission, drain submitted work, reconcile results, then release FW leases. A
state transition to NULL or a worker timeout does not cancel an in-flight accelerator
operation. If completion cannot be proved, quarantine the resource and request the
FW/BSP recovery path.

## Evidence gates before enabling a path

1. Record `gst-inspect-1.0` output, plugin version and the negotiated caps on the
   target image.
2. Run model input/output golden tests, including stride, chroma layout, placement,
   channel order, dtype and quantization.
3. Trace buffer ownership, cache scope, fences and completion while repeatedly
   loading, streaming, draining and unloading.
4. Measure copies, pool occupancy, latency, FPS, CPU, memory and thermal behavior on
   the agreed workload. A plugin name or vendor sample is not performance evidence.
5. Keep per-file license/provenance notices. Do not copy vendor implementation or
   private SDK headers into neutral LACAI layers.

The current LACAI Qualcomm implementation is a private plugin-backed adapter with
source binding, bounded submission/result handling and drain bookkeeping. It is not
yet a board-qualified service or an end-to-end `vqec_ai_vision_applications` binary.
