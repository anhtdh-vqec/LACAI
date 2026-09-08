# Model observation contract

`observation_batch` is the neutral handoff from model decoders to tracking and feature
rules. It is deliberately independent of Qualcomm/QNN, Rockchip, MediaTek or Novatek
types. Every batch binds one source epoch, frame ID, source PTS and image geometry.
Track IDs are local to that source/epoch; they are not face identity or a cross-camera
person identity.

An observation contains a model class, pixel-space box, confidence, quality and optional
attributes. Attribute schema ID/version identify meaning; value is opaque text at this
boundary so age, gender, clothing, PPE, face-recognition status, plate text and future
traffic properties can be added without changing the envelope. Sensitive values such as
embeddings or face identity require a separate reviewed storage/output contract and are
not automatically drawable or publishable.

Observed and expiry timestamps are in the producer's declared source clock. Expiry must
not precede observation. `unknown` quality is allowed and must not be treated as a
positive classification by feature rules. Confidence is bounded but is not calibration
or business acceptance.

The validator checks identity, geometry through the existing preview contract, bounded
observation/attribute counts, identifier syntax, confidence and time ordering. It does
not verify model artifact authenticity, decoder semantics, calibration, tracking
continuity, entitlement or dataset quality. Model integration must provide those checks
and golden/replay evidence before usecase rollout.
