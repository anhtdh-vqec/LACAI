# Qualcomm preprocessing adapter

This document defines the descriptor-driven cDSP image-transform path behind the neutral
`image_processor_port` and its measured QCS6490 behavior.

**Status:** accepted — the declared current workload passed the AI APP lead's five-minute
30 FPS / average-CPU gate on QCS6490 `.98` on 2026-09-18. **Layer:** adapters.
**Source:** `src/adapters/qualcomm/dsp/host/vqec_vision_dsp_preprocessor.{hpp,cpp}`,
`src/adapters/qualcomm/dsp/v1/vqec_vision_dsp_v1_image.{h,c}`.

## Responsibility

- Convert a retained linear NV12 source into the exact admitted model tensor without exposing
  FastRPC, FastCV or DMA-BUF types through the neutral port.
- Marshal immutable image semantics as v1 descriptor data; never dispatch by model name.
- Keep source, mapping and output owners alive through synchronous DSP completion, and
  quarantine them after an uncertain transport result.
- Reject unsupported colour, geometry, normalization, dtype or capacity before DSP access.

## Production path

The Qualcomm platform constructs one reusable DSP preprocessor for every primary model slot:

```text
retained FW NV12 DMA-BUF
  -> bounded mapping lease / registered FastRPC input
  -> v1 image_transform descriptor
  -> cDSP FastCV scale + NV12-to-RGB
  -> UINT16 tensor quantization in reusable DSP scratch/output
  -> reusable neutral tensor_blob
  -> QNN HTP graph
```

The 176-byte descriptor carries the v1 request envelope plus source plane offsets/strides,
crop, tensor and destination geometry, colour matrix/range, interpolation, placement, channel
order, normalization, quantization and exact byte capacities. The current accepted envelope is
linear NV12, BT.709 limited range, bilinear letterbox, top-left or centred placement, RGB,
offset/scale normalization and UINT16 output. A descriptor outside this envelope fails closed.

The cDSP skeleton resolves the required FastCV functions from the board-provided
`libfastcvadsp.so`. It advertises `image_transform` only after all required symbols resolve.
Together with generic dense decode and overlay compose, the measured capability mask is `19`.
Person and fire/smoke differ only in validated descriptor values. SCRFD output is decoded by the
portable neutral anchor-distance decoder; EdgeFace retains its bounded ROI alignment path.

## Ownership and allocation

Registered DMA-BUF input is retained through the synchronous call. A compatibility memfd cannot
be registered and may use the explicit QAIC-copy fixture policy; that result is not zero-copy
evidence. Output uses one reusable rpcmem allocation per preprocessor and one reusable neutral
tensor allocation. DSP scale/colour scratch grows only to an admitted maximum and is reused.

Successful transport plus a valid operation response is completion for this synchronous v1
contract. Any execution/transport ambiguity faults the session and retains the affected owners
in quarantine; close, timeout or source disconnect is not treated as cancellation.

## Board evidence

The exact release candidate was built with the approved eSDK and Hexagon SDK 5.5.7.0. On
QCS6490 `.98`, the declared full workload included person, SCRFD, fire/smoke and the configured
EdgeFace cascade, with a 1920x1080@30 DMA-heap source and H.264 preview.

| Gate | Result |
|---|---:|
| DSP smoke | `image_transform`, `dense_decode` and `overlay_compose` pass; mask `19` |
| eSDK/QEMU | 135/135 CTest pass for the exact final source |
| Preview | 241 packets/8 s = 30.125 FPS, H.264 1920x1080 |
| Five-minute ring rate | 9006 frames/300.119829 s = **30.008 FPS** |
| Five-minute process CPU | 6.53% usr + 6.96% sys = **13.50% average** of one core |
| Short lifetime | FD 122 -> 122; threads 48 -> 48; RSS/HWM +508 KiB |

The four-frame contact sheet was reviewed: person/face boxes and labels were aligned, colour and
orientation were correct, and no stale overlay was observed. The one-second CPU maximum was
16.00%; the accepted requirement is the five-minute sustained average below 15%, not a per-second
ceiling. The small RSS increase is not proof of leak freedom.

## Limits and next work

- The result uses a DMA-heap compatibility camera, not the released FW camera/cache/fence path.
- BSP signing, reset during in-flight work and released-FW completion evidence remain external
  release gates.
- The current image envelope intentionally rejects BGR, full range, arbitrary crop, UINT8/FLOAT
  output and non-direct quantization until each receives contract and numeric evidence.
- AI Model still owns semantic/quality approval of model goldens; live regression output is not
  an independent accuracy oracle.
- Longer thermal and memory qualification may be required by product release policy, but was
  explicitly excluded from this plan's five-minute AI APP acceptance gate.

## See also

- [Qualcomm FastRPC adapter](qualcomm_fastrpc_adapter.md)
- [Qualcomm adapter blueprint](qualcomm_adapter.md)
- [DSP optimization plan](../planning/architecture_improvement/dsp_multiplatform_optimization_plan.md)
- [QCS6490 target](../testing/qsc6490_board.md)
