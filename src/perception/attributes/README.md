# attributes

Typed attribute schema, confidence/unknown, quality/freshness and temporal fusion.

`vqec_vision_attribute_reader` provides exact schema/version lookup on a tracked batch
with source-clock freshness checks and borrowed-result lifetime. It selects no model or
feature and performs no temporal fusion. Concrete attribute producers remain pending.
