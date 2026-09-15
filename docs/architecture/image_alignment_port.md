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

## Not claimed

Defining this contract does not prove FastCV/QTI affine capability, crop/tensor pool
ownership, cache/fence behavior, device completion, alignment parity against a golden crop
or any FD→FR correlation. Those require the M4 adapter and board evidence.

## See also

- [ADR 0005](../adr/0005_scalable_model_integration.md),
  [cascade inference](cascade_inference.md)
