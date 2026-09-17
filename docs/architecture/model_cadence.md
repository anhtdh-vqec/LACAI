# Per-source model cadence scheduler

`model_cadence_scheduler` converts cold per-source model inference rates into fixed
arithmetic and selects which assigned models are due on each frame. This document defines
the rational phase arithmetic, the slot identity and the dispatch QoS policy.

**Status:** source-delivered — portable source and test source delivered; integrated into
the portable multi-model pump and multi_model_session; target logic tests pass and live
integration remains pending. **Layer:** runtime. **Source:**
`src/runtime/scheduler/vqec_vision_model_cadence.cpp`,
`tests/unit/vqec_vision_model_cadence_test.cpp`.

## Responsibility

- Selects due models on the frame path without string lookup, vector growth, heap
  allocation, wall-clock calls or vendor API operations.
- Converts cold configuration values into fixed 64-bit increments/thresholds and uses a
  16-bit due mask.
- Decides eligibility only; it does not check graph capacity, own graph lifecycle or
  enforce grants.
- Keeps model slot order equal to the validated deployment source's `model_ids` order and
  stable for the immutable activation revision.

## Rational phase arithmetic

One RAW source may feed 1..16 assigned models, each with its own rational inference rate
from the AI Model-team catalog.

```text
increment = model_fps_numerator * source_fps_denominator
threshold = source_fps_numerator * model_fps_denominator
phase += frame_sequence_delta * increment
due when phase >= threshold; phase %= threshold
```

All assigned models are due on the first observed frame. A frame-sequence jump advances the
phase by the full delta but emits only one latest-frame decision per model; missed cadence
intervals are counted as skips rather than creating a burst of stale submissions. Duplicate,
backward, zero and maximum sequence IDs fail without changing scheduler state or output.
Multiplication/addition and skip counters are checked before commit.

The cold composer resolves model IDs against the Model-team catalog once; the frame path
uses only numeric slots. A model cadence above the source frame rate is rejected.

## Pump integration and dispatch QoS

`multi_model_pump` now checks graph capacity, skips a due selection when that graph remains
busy, shares one received frame owner across accepted graphs and reports fixed-slot
submission masks.

Dispatch QoS is explicit per model through `model_dispatch_policy`. `drop_if_busy` skips a
due-but-busy graph. `latest_wins` and `replace_pending` are served by the pump's bounded
one-slot mailbox: the newest due preprocessed input is parked and submitted when the graph
frees up, while a free sibling graph keeps the source progressing. `must_process_once` and
`event_triggered` need a durable queue and are rejected as `unsupported` at activation
rather than silently behaving like `drop_if_busy`.

## Lifecycle and admission

The implemented `multi_model_session` owns graph lifecycle and source drain. Authenticated
admission/entitlement activation remains external integration work; the session does not
enforce grants. FW may ACK only after the last hardware reader completes. No throughput or
hardware-acceleration claim follows from cadence selection.

## Limits and next work

- Live integration remains pending.
- Authenticated admission/entitlement activation remains external integration work; the
  session does not enforce grants.
- `must_process_once` and `event_triggered` remain `unsupported` until a durable queue
  exists.
- No throughput or hardware-acceleration claim follows from cadence selection.

## See also

- [Multi-model pump](multi_model_pump.md)
- [Model catalog](model_catalog.md)
- [Multi-model session](multi_model_session.md)
