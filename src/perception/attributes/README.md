# attributes

Typed tracked-attribute schema, confidence/unknown handling, quality/freshness and
temporal-fusion boundary.

- **Status:** source-delivered reader — concrete attribute producers pending
- **Naming registry:** `attr` (`atrdr`)
- **Depends on:** detection/tracking condition contracts
- **Used by:** feature processors and output authorization

## Responsibility

- Provide exact schema/version lookup on a tracked batch.
- Enforce source-clock freshness and bounded values with borrowed-result lifetime.
- Select no model or feature and perform no temporal fusion.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_attribute_reader.cpp` | Exact schema/version lookup with source-clock freshness and bounded values |

## Limits and next work

- Concrete typed attribute producers, temporal fusion and calibration remain pending.

## See also

- [Attribute reader](../../../docs/architecture/attribute_reader.md)
