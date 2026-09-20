# Feature fan-out

`feature_fanout` is the portable per-source coordinator from one tracked observation batch
to a bounded set of already-activated feature stages. This document defines its slot
identity, fault isolation and report semantics.

**Status:** source-delivered — the fan-out exists with contract tests. **Layer:** features.
**Source:** `src/app/pipeline/vqec_vision_feature_fanout.cpp`,
`tests/contract/application/vqec_vision_feature_fanout_test.cpp`.

## Responsibility

- Coordinates one tracked observation batch to a bounded set of already-activated feature
  stages.
- Keeps slot identity stable while allowing one serialized, prevalidated replacement of
  the borrowed stage set for an activation delta.
- Performs no activation, entitlement, output delivery or automatic replacement.
- Borrows unique stage owners.

## Scheduling and fault isolation

Activation binds 1..32 stable numeric slots; the ceiling allows the current catalog and
extensions without embedding a commercial feature list or string lookup in the frame path.
The cold path may replace that bounded array, including replacing it with an empty set.
Validation completes before any pointer or count changes; duplicate, null-in-range or
inactive stages leave the live fan-out unchanged. The caller serializes replacement with
frame processing and keeps both the old and candidate owners alive until the swap completes.

One serialized call validates the tracked batch and monotonic time before invoking any
stage. Each stage receives the same immutable observations and source-gap flag. Processing
continues after a stage error so one feature cannot starve healthy features. Successful
slots publish their event batch; a failed slot preserves its previous output through the
feature-stage transactional contract.

The report records processed and failed bitmasks, one status code per configured slot and
the first failing slot. The function returns the first failure after all slots have been
advanced. Therefore outputs and report remain meaningful on a non-`ok` return; there is no
cross-feature atomic publication claim. Slots outside the configured count are untouched.

## Limits and next work

- There is no cross-feature atomic publication claim.
- Direct single-model dependencies are connected by the multi-model feature pipeline.
- Replacement is process-local and serialized; it is not a lock-free hot-path mutation.

## See also

- [Multi-model feature pipeline](multi_model_feature_pipeline.md)
- [Feature stage](feature_stage.md)
- [Feature event contract](feature_event_contract.md)
