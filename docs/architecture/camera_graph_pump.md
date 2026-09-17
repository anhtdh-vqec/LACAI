# Camera third-stream to Qualcomm graph pump

This document defines the single-model progress primitive that couples one FW RAW
acquisition cycle to one inference graph: it starts Camera control/media, configures the graph
from the effective FW profile and then advances receive/submit/result one step at a time.

**Status:** source-delivered — composition wiring in `src/app` with synthetic target tests; no
live FW execution yet. It is not a vendor dependency in neutral runtime/core and not an
executable service. **Layer:** app. **Source:**
`src/app/vqec_vision_camera_graph_pump.{hpp,cpp}`.

## Responsibility

- Advance exactly one acquisition cycle for the supervisor.
- Exclusively own submission/polling for that graph; never attach a fresh pump to an
  already-armed graph or reuse it across a source acquisition cycle.
- Depend only on `raw_source_port` and `inference_graph_port` in neutral code.

The supervisor owns the `raw_source_port`/`inference_graph_port` implementations; the platform
adapter owns any vendor graph and safety-retention domain. For one acquisition cycle the pump
first starts Camera control/media, uses the effective FW profile to
configure/load/bind/start the graph, then calls `pump_step` with steady-clock nanoseconds. The
first received descriptor supplies the receiver epoch to `arm_submission`; the cycle ID comes
from the supervisor, never `buf_id`.

## Pump cycle and frame path

Each call either consumes one tensor result, polls pending work, or receives at most one camera
frame with `timeout=0` and submits it.

- No pixel copy is added on input: the `raw_frame` shared owner is passed through the FD bridge;
  normal final-owner release still ACKs on the original FW socket.
- Output is the existing explicit CPU copy.
- No per-frame D-Bus, second raw stream, `qmmfsrc`, internal worker or unbounded queue.
- The existing FW socket/pinned-buffer policy can still accumulate older frames; this pump does
  not promise latest-frame latency or alter FW drop policy.
- The pump checks graph completion/deadlines before receiving another input and never holds an
  extra unsubmitted frame across calls. No-data timeout maps to pending.
- Submit failures report whether a ticket was committed; a failure is latched and stops future
  receives. Rejected unsubmitted frames may release normally.
- Outstanding work is still polled after a fault, but its outputs are not published by the pump.
- Successful result output includes the stored source epoch/frame ID/source PTS plus
  pipeline/job ticket for correlation.
- The report is reset on every call; tensor output is modified only when `has_result` is true.

## Stop, fault and teardown

`begin_stop` permanently stops receives, without an RPC or forced graph teardown. Subsequent
pump calls continue result/completion polling. The supervisor calls graph `request_drain`, polls
until EOS plus zero jobs, unloads, then calls Camera stop steps until stopped. If the graph is
faulted, keep polling late completions; unload only when its guard permits. If unresolved, retain
objects and escalate to BSP; never force ACK. The pump destructor owns no source/graph resources
and performs no shutdown.

## Limits and next work

- Does not implement configuration/entitlement, model decoder, event routing,
  reconnect/reacquisition, durable recovery, service main or IPK packaging.
- Existing FW memory/sync and model golden sign-off requirements remain unchanged.

## See also

- [Single-camera session coordinator](camera_session.md)
- [Combined Camera control/media lifecycle](camera_source_lifecycle.md)
- [Legacy camera FD to private GStreamer memory bridge](dmabuf_memory_bridge.md)
- [Qualcomm submission lifecycle and retained faults](qualcomm_submission_lifecycle.md)
