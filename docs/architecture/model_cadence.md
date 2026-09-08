# Per-source model cadence scheduler

Status: portable source and test source delivered; build/runtime integration pending.

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

This scheduler decides eligibility only. The future multi-model source session must also
check graph capacity, outstanding-job state, entitlement and overload policy before
submission. It must share the one received frame owner across every accepted graph and ACK
FW only after the last hardware reader completes. No throughput or hardware-acceleration
claim follows from cadence selection.
