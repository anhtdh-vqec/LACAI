# Camera third-stream to Qualcomm graph pump

Source candidate, no live FW/board execution. This is composition wiring in src/app,
not a vendor dependency in neutral runtime/core and not an executable service.

Supervisor owns raw_source_port/inference_graph_port implementations; the platform adapter
owns any vendor graph and safety-retention domain.
and camera_graph_pump for one acquisition cycle. It first starts Camera control/media,
uses the effective FW profile to configure/load/bind/start the graph, then calls
pump_step with steady-clock nanoseconds. The first received descriptor supplies the
receiver epoch to arm_submission; the cycle ID comes from the supervisor, never buf_id.
The pump must exclusively own submission/polling for that graph; never attach a fresh
pump to an already-armed graph or reuse it across a source acquisition cycle.

Each call either consumes one tensor result, polls pending work, or receives at most
one camera frame with timeout=0 and submits it. No pixel copy is added on input:
the raw_frame shared owner is passed through the FD bridge; normal final-owner
release still ACKs on the original FW socket. Output is the existing explicit CPU copy.
No per-frame D-Bus, second raw stream, qmmfsrc, internal worker or unbounded queue.
The existing FW socket/pinned-buffer policy can still accumulate older frames;
this pump does not promise latest-frame latency or alter FW drop policy.

The pump checks graph completion/deadlines before receiving another input and never
holds an extra unsubmitted frame across calls. No-data timeout maps to pending.
Submit failures report whether a ticket was committed; a failure is latched and stops
future receives. Rejected unsubmitted frames may release normally. Outstanding work
is still polled after a fault, but its outputs are not published by the pump.
Successful result output includes the stored source epoch/frame ID/source PTS plus
pipeline/job ticket for correlation.
The report is reset on every call; tensor output is modified only when has_result is true.

begin_stop permanently stops receives, without a RPC or forced graph teardown.
Subsequent pump calls continue result/completion polling. Supervisor calls graph
request_drain, polls until EOS plus zero jobs, unloads, then calls Camera stop steps
until stopped. If graph is faulted, keep polling late completions; unload only when
its guard permits. If unresolved, retain objects and escalate to BSP; never force ACK.
The pump destructor owns no source/graph resources and performs no shutdown.

This does not implement configuration/entitlement, model decoder, event routing,
reconnect/reacquisition, durable recovery, service main or IPK packaging. Existing
FW memory/sync and model golden sign-off requirements remain unchanged.
