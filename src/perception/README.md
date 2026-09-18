# perception

Portable interpretation of tensors and observations, separated by algorithm family.

- **Status:** source-delivered — decoder/tracker/recognition paths are covered by eSDK tests
- **Layer:** perception
- **Naming registry:** `detec`, `track`, `attr`, `pose`, `embed`, `ocr`
- **Depends on:** neutral contracts and portable core values
- **Used by:** application perception stages and feature processors

## Responsibility

- Decode model outputs and maintain bounded temporal perception state.
- Publish neutral observations/attributes without vendor runtime types.
- Keep business-event policy in `src/features/`, not in decoders or trackers.

## Contents

| Path | Purpose |
|---|---|
| `detection/` | Detection tensor readers and decoder implementations |
| `tracking/` | Tracker implementations and registry |
| `attributes/`, `pose/`, `embedding/`, `ocr/` | Typed perception families |

## Limits and next work

- Model parity and accuracy require model-specific golden/board evidence.

## See also

- [Repository source layout](../../docs/development/source_layout.md)
