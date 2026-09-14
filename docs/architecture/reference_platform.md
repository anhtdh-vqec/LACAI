# Device-free platform owners

`--mode production` never substitutes a fixture implicitly. The service selects a
platform owner by explicit name; an unset or unwired name fails closed with exit 3. Two
device-free owners exist, and neither is model, accuracy or hardware evidence.

## `--platform fake` (development)

`fake_platform` registers a deterministic fixture detector, a fake tracker that assigns a
fresh id per detection and a fake feature that emits one snapshot per track. It proves
only that decode, tracking, feature fan-out and event delivery are wired. It is the
harness default outside production mode.

## `--platform reference` (M5)

`reference_platform` wires the real device-free implementations behind the neutral ports:

- decoder: the shared `fixture_detector` (deterministic box, no tensor semantics),
- tracker: `reference_tracker` through a `tracker_factory_port` (IoU association, miss
  aging, lost-timeout expiry),
- feature: `reference_zone_feature` through a `feature_processor_factory_port`
  (ROI presence, dwell, line crossing and count), configured with a full-frame zone when
  the context supplies no extent.

The owner is configured with the source dimensions and tracker contract, then registers a
decoder for every catalog decoder contract, one tracker factory for the contract and one
feature factory for every processor contract. Registration is non-owning: the owner must
outlive the registries and the runtime bundle.

## Ownership and clock domains

The bundle borrows the registries and platform owners; they must reach stopped before the
bundle or the platform is destroyed. Feature event time is the source frame's PTS, not the
monotonic step clock: the step clock is used only for dwell and cooldown arithmetic inside
the processor. Mixing the two domains fails the feature-event contract and produced a zero
delivery before the fix.

## Evidence boundary

`service_production_reference_smoke` asserts that the reference owner routes, emits at
least one zone event and stops cleanly. This is logic/wiring evidence. It does not load a
model, exercises no camera or FW transport and proves nothing about accuracy, Qualcomm
support or hardware completion.
