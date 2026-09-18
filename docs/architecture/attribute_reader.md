# Attribute reader

`attribute_reader` is the portable lookup boundary used by feature algorithms after
tracking. This document defines its exact-input lookup contract, the borrowed-pointer
lifetime rule and the validation limits it enforces.

**Status:** source-delivered — the reader source exists in the tree. **Layer:** perception.
**Source:** `src/perception/attributes/vqec_vision_attribute_reader.cpp`,
`tests/contract/perception/vqec_vision_attribute_reader_test.cpp`.

## Responsibility

- Looks up one exact, non-expired attribute on a requested track.
- Validates the complete tracked observation batch first.
- Returns a borrowed pointer only while the input batch remains alive and unchanged.
- Must not select a schema, model or product feature inside the reader.
- Must not infer unknown quality, fuse temporal values, authorize output or choose a
  feature rule.

## Lookup contract

A caller supplies an exact track ID, attribute schema ID/version and current time in the
attribute producer's source-clock domain. The reader first validates the complete tracked
observation batch. It then returns a borrowed pointer only for one exact, non-expired
attribute on the requested track. A missing track, missing attribute or expired value
returns `pending`; malformed identity, ambiguous duplicate data or a clock earlier than
`observed_at_ns` is rejected. Failure preserves the caller's previous pointer value. The
pointer is valid only while the input batch remains alive and unchanged.

## Validation limits

Observation validation limits opaque attribute values to 512 bytes and rejects duplicate
nonzero track IDs and duplicate schema-ID/version pairs on one observation. These are AI
memory and ambiguity safety ceilings, not fixed model schemas or commercial defaults.
Embeddings and other sensitive binary payloads require a separate reviewed contract.

## Limits and next work

- The reader does not authorize output or choose a feature rule.
- Concrete attribute producers and feature algorithms remain responsible for schema
  meaning, calibration, cadence and replay evidence.
- Sensitive binary payloads require a separate reviewed contract.

## See also

- [Model observation contract](observation_contract.md)
- [Feature stage](feature_stage.md)
- [Tracking stage](tracking_stage.md)
