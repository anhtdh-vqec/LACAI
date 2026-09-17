# Model observation contract

`observation_batch` is the neutral envelope used from model decoders through tracking and
feature rules, binding one source epoch, frame ID, source PTS and image geometry. This
document defines that envelope, its attribute and landmark carriers, and the validation
that the detection and tracked forms apply.

**Status:** source-delivered — the neutral contract and its detection/tracked validators
exist in the tree. **Layer:** contracts. **Source:**
`include/vqec/vision/ai/contracts/vqec_vision_observation.hpp`,
`src/core/vqec_vision_observation.cpp`,
`tests/unit/vqec_vision_observation_test.cpp`.

## Responsibility

- Carries one source epoch, frame ID, source PTS and image geometry per batch.
- Distinguishes a decoder detection batch (`track_id=0`, unassociated) from a tracker
  tracked batch (every observation nonzero track ID).
- Carries optional attributes and typed landmarks without widening the envelope.
- Must not equate track IDs with face identity or cross-camera person identity.
- Must not make sensitive values automatically drawable or publishable.

## Batch and observation fields

`observation_batch` is deliberately independent of Qualcomm/QNN, Rockchip, MediaTek
or Novatek types. Every batch binds one source epoch, frame ID, source PTS and image
geometry. A decoder produces a detection batch where `track_id=0` means unassociated.
A tracker produces a tracked batch where every observation has a nonzero track ID.
Track IDs are local to that source/epoch; they are not face identity or a cross-camera
person identity.

An observation contains a model class, pixel-space box, confidence, quality and optional
attributes. Attribute schema ID/version identify meaning; value is opaque text at this
boundary so age, gender, clothing, PPE, face-recognition status, plate text and future
traffic properties can be added without changing the envelope. Sensitive values such as
embeddings or face identity require a separate reviewed storage/output contract and are
not automatically drawable or publishable.

## Landmarks and embeddings

Decoder geometry such as face landmarks is carried separately as typed source-pixel
points with a schema ID/version defining point order. Empty landmarks require empty
schema identity; a populated set is bounded, finite and strictly inside source geometry.
Model-package decoder configuration enforces an exact point count such as five rather
than relying on a model name. Embeddings use the separate `embedding_result` contract,
which binds source frame, track and model provenance and validates dimension, finite
values and optional L2-normalization claims.

## Time and quality

Observed and expiry timestamps are in the producer's declared source clock. Expiry must
not precede observation and must be finite; `UINT64_MAX` is not a permanent-value
shortcut. `unknown` quality is allowed and must not be treated as a positive
classification by feature rules. Confidence is bounded but is not calibration or
business acceptance.

## Validation

The detection validator checks identity, geometry through the existing preview contract,
bounded observation/attribute counts, identifier syntax, confidence and time ordering;
it permits zero track IDs. Opaque attribute values have a 512-byte safety ceiling;
duplicate schema-ID/version pairs and duplicate nonzero track IDs are rejected. The
tracked validator applies the same checks and additionally requires every track ID to be
nonzero. Feature rules that depend on continuity must accept only the tracked form.

Neither validator verifies model artifact authenticity, decoder semantics, calibration,
continuity, entitlement or dataset quality. Model integration must provide those checks
and golden/replay evidence before usecase rollout.

## Limits and next work

- Validation is structural only; it does not verify decoder semantics, calibration,
  continuity, entitlement or dataset quality.
- Model integration must provide authenticity checks and golden/replay evidence before
  usecase rollout.
- Sensitive values such as embeddings require a separate reviewed storage/output contract.

## See also

- [Preview metadata boundary](preview_contract.md)
- [Attribute reader](attribute_reader.md)
- [Tracking stage](tracking_stage.md)
