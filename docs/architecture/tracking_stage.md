# Tracking stage

`tracking_stage` is the portable serialized boundary between decoded detections and a
concrete tracker. It owns no association algorithm and exposes no vendor types.

The stage validates decoder output as detections, where `track_id=0` is permitted. It
resets the tracker on the first epoch and each strictly newer source epoch; a stale epoch
is rejected rather than reset backward. It supplies monotonic process time and the
explicit source-gap flag, then validates the returned batch as tracked observations where
every track ID must be nonzero. Publication is transactional: an error never overwrites
the caller's prior tracked batch.

Tracker update failure is ambiguous because an implementation may have mutated temporal
state before returning. The stage therefore faults the current epoch and rejects further
updates. A later nonzero source epoch may recover only after `reset_epoch` succeeds. A
timeout or frame gap does not implicitly reset state; the caller declares the gap and the
tracker applies its model-specific policy.

This source does not implement ByteTrack, optical flow, cross-camera identity, persistence
or feature decisions. A concrete tracker must document its class compatibility, cadence,
gap policy, memory envelope and golden/replay evidence before activation.

Reset failure is also ambiguous. Before invoking reset, the stage records the attempted
epoch and monotonic time and latches a fault. A reset exception becomes io_error.
Neither a returned error nor an exception permits retry in that same epoch or rollback
to an older epoch. Only a strictly newer epoch with successful reset can recover.
