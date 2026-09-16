# Image alignment port

Status: contract defined and unit-tested; no backend implements it and no orchestration
consumes it yet. This is the boundary required by ADR 0005 for secondary (cascade) face
crops; it does not itself make FD→FR run.

## Purpose

A secondary model (for example a face embedding network) consumes one aligned crop per
detected face, not the full frame. `image_alignment_port` turns a borrowed source frame plus
typed source-pixel landmarks into the exact aligned destination tensor, and returns the
transform so results can be mapped back to the source frame.

Contract: `include/vqec/vision/ai/contracts/vqec_vision_image_alignment.hpp`.
Port: `include/vqec/vision/ai/ports/vqec_vision_image_alignment.hpp`.

## Types

- `alignment_template`: landmark schema id/version, destination width/height and the
  ordered reference points. `reference_points_.size()` is the required landmark count.
- `alignment_request`: source `preview_frame_key`, typed `observation_landmarks` and an
  optional monotonic `deadline_ns_` (0 = none). It carries no frame owner.
- `alignment_transform`: row-major 2x3 source→destination transform with source and
  destination geometry, returned as provenance.
- `alignment_result`: the owned destination `tensor_blob` and its transform.
- `alignment_capabilities`: `supports_similarity_`, `max_points_`,
  `max_destination_dimension_`.

## Ownership and completion

- The caller keeps the source frame `owner_` alive until `poll_completion` reports
  `complete`. Timeout, stop request, source disconnect and FD close are **not** completion.
- `align` borrows the source frame for the duration of the call and writes an owned
  destination tensor into `_result`; failure preserves `_result`.
- `align` returns a completion ticket; `poll_completion(ticket)` reports device completion.
  An unknown ticket is `invalid_state`. The destination must not be consumed before
  completion.
- The port never owns the source, the crop pool or the cascade task queue.

## Capability and errors

- `probe_capabilities` reports support; `validate_template` fails closed with `unsupported`
  when a template exceeds the probe. There is no silent CPU fallback.
- Structural validation (`vqec_vision_ai_core_imaln_validate_template`/`_validate_request`)
  rejects bad schema identity, destination geometry outside limits, empty/oversized or
  non-finite reference points, mismatched landmark schema/count, and invalid frame identity
  or deadline.

## FastCV capability evidence (QCS6490, Qualcomm Linux 1.8)

Checked against the board `.48` and the approved eSDK sysroot (qcom-fastcv-binaries 1.8.5):

- The QTI `qtivtransform` plugin exposes `crop`/`destination` rectangles, resize, flip and
  90° rotate only; it has no arbitrary-angle landmark affine.
- FastCV headers (`/usr/include/fastcv/fastcv.h`) document affine/perspective warp, and the
  sysroot library `usr/lib/libfastcvopt.so.1.8.0` **exports** the API (4692 `T` symbols),
  including `fcvTransformAffineu8_v2` (warps a patch centered at `position` with a 2×2
  affine), `fcv3ChannelTransformAffineClippedBCu8` (3-channel affine with border),
  `fcvGeomAffineFitf32` and `fcvGetPerspectiveTransformf32`.

Conclusion: the M4 alignment adapter should be an owned FastCV adapter that computes the
similarity transform in neutral code and calls the FastCV affine warp (with FastCV color
conversion), not the QTI plugin. This is capability evidence only — it does not prove
runtime execution, DSP offload, crop/tensor pool ownership or golden crop parity, all of
which remain M4.

## FastCV affine convention (established on `.48`)

A synthetic-image smoke (`tools/vqec_vision_fastcv_affine_smoke.cpp`, run on `.48`) established
the convention of `fcvTransformAffineu8_v2(source, W, H, stride, position, affine, patch,
pw, ph, stride)`:

- `position[2]` is the patch center in **source** coordinates (float).
- `affine[2][2]` (row-major a11,a12,a21,a22) is the **inverse** linear map from patch
  coordinates relative to the center to source coordinates relative to `position`:
  `source = position + affine * (patch - patch_center)`, `patch_center = (pw/2, ph/2)`.
- Interpolation is bilinear (a half-scale warp of a gradient produced two-pixel steps).
- Border behavior near the image edge is not yet exercised.

Mapping the neutral source→destination similarity (`dst = M * src + t`) to FastCV:
`affine = M⁻¹` and `position = M⁻¹ * (patch_center - t)`. Geometry and RGB color paths have
board smoke evidence; golden crop/input parity, edge policy and DSP offload remain M4.

## Owned FastCV adapter (delivered)

`fastcv_aligner` (`src/adapters/qualcomm/vqec_vision_fastcv_aligner.cpp`) implements the port:
it validates the request/template, computes the neutral similarity, maps it to FastCV, maps
the borrowed source NV12 FD read-only (page-aligned `mmap`), converts the sampled even-aligned
ROI using the explicit model color contract, warps each channel with FastCV, and returns an
owned NHWC RGB/BGR `uint8` tensor plus the transform and a synchronous completion ticket.
The ROI is expanded to at least the destination dimensions because the QCS6490 FastCV
binary rejects a smaller affine input even when its sampled footprint is valid. If the
vendor warp still rejects a valid ROI, the adapter uses a bounded bilinear sampler on that
small ROI; it does not convert or warp the full source frame. This is a correctness fallback
and must be counted separately in future performance telemetry.

Board evidence (`.48`, synthetic NV12 memfd, `vqec_vision_fastcv_affine_smoke`): the identity
warp centered at source (32, 32) produced the expected 8×8 neighborhood with the marker `255`
exactly at the patch center (`align_rc=0`, `bytes=64`, `complete=1`). This verifies the
geometry mapping, not color, DSP offload or golden parity.

Still M4: crop/tensor pooling, golden crop/input parity, approved edge/border behavior and
any DSP offload claim. The adapter is built only under `VQEC_VISION_AI_ENABLE_FASTCV` and
is bound through `image_alignment_port` to the production cascade coordinator.

### RGB destination (delivered, board-verified color)

The FastCV semi-planar conversion `fcvColorYCrCb420PseudoPlanarToRGB8888u8` expects a Y plane
followed by an interleaved **CrCb** (NV21) plane and outputs **RGBA8888** with BT.601
coefficients, while FastCV also provides `fcvColorYCbCr420PseudoPlanarToRGB888u8` for linear
**NV12** (CbCr) converting directly to packed 24-bit RGB with ARM Neon SIMD vectorization.
The adapter prioritizes `fcvColorYCbCr420PseudoPlanarToRGB888u8` when the configuration requests
BT.601 limited-range NV12 (with stride aligned to 8-byte boundaries as required by FastCV),
falling back to the reviewed neutral `vqec_vision_ai_core_color_convert_nv12_to_rgb` for
BT.709 or full-range matrices.

Cost control & memory reuse:
1. **ROI Bounding**: Before conversion the adapter computes the source region the aligned patch
   actually samples (plus an interpolation margin) and converts/warps only that even-aligned
   NV12 ROI, so per-face cost scales with the face, not the frame.
2. **Persistent Scratch Workspace**: To eliminate per-face dynamic heap reallocations (which
   previously allocated 7 separate `std::vector` buffers per detected face per frame), the
   adapter maintains thread-confined scratch vectors (`rgb_scratch_`, `planes_scratch_`,
   `patches_scratch_`, `luma_scratch_`). Buffers only grow when needed (`reserve()` / `resize()`),
   eliminating heap fragmentation on the hot path.
3. **Neon Vectorization**: Utilizing `fcvColorYCbCr420PseudoPlanarToRGB888u8` provides direct
   Qualcomm Neon hardware acceleration for color conversion without per-pixel scalar math.

Board `.48` / `.98` smoke: synthetic BT.601-limited red NV12 aligned to `rgb center=238,14,14`
via FastCV Neon conversion (matching `qtivtransform` color output), and the luma path produced
the expected geometry. This verifies color, stride alignment, and geometry.

## Not claimed

Defining this contract does not prove FastCV/QTI affine capability, crop/tensor pool
ownership, cache/fence behavior, device completion, alignment parity against a golden crop
or any FD→FR correlation. Those require the M4 adapter and board evidence.

## Hardware offload options (board `.48` probe)

`gst-inspect-1.0` on `.48` confirms the QTI plugin set relevant to alignment/preprocess:

- `qtivtransform`: `engine` = `gles` (OpenGLES GPU) or `fcv` (FastCV), with `crop` and
  `destination` rectangles, resize, flip and 90/180 rotation. It can offload an axis-aligned
  ROI crop + NV12->RGB + resize to GPU/FastCV, but has **no arbitrary-angle affine**.
- `qtimlvconverter`: machine-learning video converter with `image-batch-*` and
  `roi-batch-*` modes; `roi-batch-*` uses `GstVideoRegionOfInterestMeta` to crop each ROI and
  produce a tensor batch in hardware. This can offload per-face ROI crop + color + resize and
  tensor packing for multiple faces in one pass.
- `qtivcomposer`: GPU/FastCV video composer (`engine` gles/fcv).
- `qtiobjtracker`: ByteTrack object tracker plugin; a candidate to replace the reference IoU
  tracker.
- `v4l2h264enc`/`v4l2h264dec`: hardware codec (already used).

Recommended offload for the alignment adapter, instead of the current CPU ROI convert plus
FastCV CPU warp:

1. Take the face ROI (from primary detections) and drive `qtimlvconverter` `roi-batch-*` (or
   `qtivtransform` crop/destination with `engine=gles|fcv`) to do ROI crop, color conversion
   and resize in hardware.
2. Keep only the residual arbitrary-angle similarity rotation on the small buffer via FastCV,
   or verify that `engine-param` / a GLES transform matrix can express the full affine.
3. Use `qtiobjtracker` (ByteTrack) to move tracking off CPU.

The exact `engine-param` grammar and whether `qtivtransform`/`qtivcomposer` accept an
arbitrary transform matrix must be verified with a board pipeline before use; no offload
claim is made here.

Board experiment (`.48`): `gst-launch-1.0 videotestsrc ... ! qtivtransform crop="<100,100,
200,200>" destination="<0,0,112,112>" ! video/x-raw,width=112,height=112 ! fakesink` exited
0 for both `engine=fcv` and `engine=gles` (gles initialises an offscreen EGL display
headless). An arbitrary `engine-param` string was accepted without error, but the plugin
strings expose no `affine`/`matrix`/`perspective` support and it reports only
resize/colorspace/flip/90-180-rotate, so **arbitrary-angle affine is not proven**. The
verified offload is therefore axis-aligned ROI crop + NV12->RGB + resize on FastCV/GPU; the
residual rotation stays on FastCV until a board pipeline proves an affine-capable path.

### `qtiv_color_converter` (offload adapter, board-verified for fcv)

`src/adapters/qualcomm/vqec_vision_qtiv_color.cpp` runs a persistent `appsrc ->
qtivtransform -> appsink` pipeline (fixed NV12/RGB caps) and converts one tightly packed
NV12 image to RGB on the plugin backend. On `.48`, `engine=fcv` returned `bytes=12288` for a
64x64 frame (rc 0). Two findings constrain integration and need an owner decision:

- Color parity: a synthetic BT.601 red produced `(238,14,14)` from the plugin versus
  `(254,0,0)` from the reviewed neutral converter (~16/255 difference). The plugin colour
  conversion is therefore **not bit-identical** to the neutral path; which is authoritative
  for FR preprocessing must be decided with golden data.
- `engine=gles` did not deliver a sample in this appsrc path (basesrc not-negotiated) and is
  not claimed.

The converter is not yet wired into `fastcv_aligner`; the offload integration (ROI crop +
color + resize on the plugin, rotation on FastCV, and the colour-authority decision) is the
next step.

## See also

- [ADR 0005](../adr/0005_scalable_model_integration.md),
  [cascade inference](cascade_inference.md)
