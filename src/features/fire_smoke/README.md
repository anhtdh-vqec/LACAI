# Fire and smoke alarm feature

The fire/smoke package converts model candidates into bounded semantic incident episodes using a
typed application configuration.

- **Status:** board-smoke — processor, strict configuration, App Manager registration, S04
  metadata/hotspot projection and AI-owned durable evidence transport passed the compatibility
  board; model-quality and released-FW evidence acceptance remain open
- **Layer:** features
- **Naming registry:** `fires` (`fsalm`, `fsfac`)
- **Depends on:** neutral observation, feature-event and processor-factory contracts
- **Used by:** S04 `security.fire_smoke_detection`

## Responsibility

- Filter fire/smoke candidates with application thresholds and minimum region area.
- Associate bounded same-class regions and apply capture-time confirmation/update/clear rules.
- Emit stable version 1 episode identities and evidence correlation intents.
- Interrupt active episodes on configured source gaps and reset all temporal state on epoch change.
- Never call QNN, DSP, camera, metadata storage, App Manager or FW evidence transport directly.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_fire_smoke_alarm.*` | Typed incident state machine and semantic event emission |
| `vqec_vision_fire_smoke_factory.*` | Strict cold-path JSON validation and processor creation |

Zone IDs are rejected unless empty in the first source slice because this processor does not yet
receive authenticated scene geometry. This fails closed instead of claiming ROI enforcement from
an identifier alone.

## Limits and next work

- The current association is bounded IoU/track matching, not a qualified smoke/fire tracker.
- Model quality and candidate-floor compatibility need signed quality receipts.
- The AI-owned durable outbox, UDS client, retry/revocation worker and reference receiver are
  delivered. A released-FW receiver/media receipt and signed model-quality receipts remain open.

## See also

- [Feature event contract](../../../docs/architecture/feature_event_contract.md)
- [Feature processor registry](../../../docs/architecture/feature_processor_registry.md)
- [Fire/smoke product slice](../../../docs/planning/architecture_improvement/fire_smoke_product_slice_plan.md)
