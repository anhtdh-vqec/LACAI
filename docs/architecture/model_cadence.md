# Per-source model cadence scheduler

Status: portable source and test source delivered; integrated into the portable
multi-model pump and multi_model_session; target logic tests pass and live integration
remains pending.

One RAW source may feed 1..16 assigned models, each with its own rational inference rate
from the AI Model-team catalog. `model_cadence_scheduler` converts those cold configuration
values into fixed 64-bit increments/thresholds and uses a 16-bit due mask on the frame path.
It performs no string lookup, vector growth, heap allocation, wall-clock call or vendor API
operation while selecting models.

The arithmetic is a rational phase accumulator:

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

Model slot order is exactly the validated deployment source's `model_ids` order and remains
stable for the immutable activation revision. The cold composer resolves those IDs against
the Model-team catalog once; the frame path uses only numeric slots. A model cadence above
the source frame rate is rejected.

This scheduler decides eligibility only. `multi_model_pump` now checks graph capacity,
skips a due selection when that graph remains busy, shares one received frame owner across
accepted graphs and reports fixed-slot submission masks.

Dispatch QoS is explicit per model through `model_dispatch_policy`. Only `drop_if_busy`
(newest-frame-wins for live detection) is implemented; `latest_wins`, `must_process_once`
and `event_triggered` need a bounded per-model queue and are rejected as `unsupported` at
activation rather than silently behaving like `drop_if_busy`. This keeps the OCR/face/
event-model QoS gap visible instead of implicit. The implemented multi_model_session
owns graph lifecycle and source drain. Authenticated admission/entitlement activation remains
external integration work; the session does not enforce grants. FW may ACK only
after the last hardware reader completes. No throughput or hardware-acceleration claim
follows from cadence selection.
