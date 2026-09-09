# Attribute reader

`attribute_reader` is the portable lookup boundary used by feature algorithms after
tracking. A caller supplies an exact track ID, attribute schema ID/version and current
time in the attribute producer's source-clock domain. No schema, model or product
feature is selected inside the reader.

The reader first validates the complete tracked observation batch. It then returns a
borrowed pointer only for one exact, non-expired attribute on the requested track. A
missing track, missing attribute or expired value returns `pending`; malformed identity,
ambiguous duplicate data or a clock earlier than `observed_at_ns` is rejected. Failure
preserves the caller's previous pointer value. The pointer is valid only while the input
batch remains alive and unchanged.

Observation validation limits opaque attribute values to 512 bytes and rejects duplicate
nonzero track IDs and duplicate schema-ID/version pairs on one observation. These are AI
memory and ambiguity safety ceilings, not fixed model schemas or commercial defaults.
Embeddings and other sensitive binary payloads require a separate reviewed contract.

This helper does not infer unknown quality, fuse temporal values, authorize output or
choose a feature rule. Concrete attribute producers and feature algorithms remain
responsible for schema meaning, calibration, cadence and replay evidence.
