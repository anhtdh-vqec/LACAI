# app

vqec_vision_encoder_preparation.cpp composes portable encoder admission with CPU pool
acquisition/rollback, cancellation, sealed input handoff, guarded neutral-backend submission
and combined backend/ledger drain. It is a separate host-compatible target without
Camera/GStreamer requirements. Concrete hardware encoder and rendering remain missing. See docs/architecture/encoder_preparation.md; tests are unexecuted.

Composition wiring belongs here; no feature rules in main.

camera_graph_pump now connects an already-started raw_source_port to a configured,
running inference_graph_port, arms from the first receiver epoch and polls/submits one
bounded step at a time. No thread, per-frame RPC or executable service is added.
See docs/architecture/camera_graph_pump.md for supervisor ownership and stop ordering.
camera_session now drives one acquisition through validation/start/pump/drain/release
with explicit startup/stop deadlines and uncertain-RPC reconciliation. It borrows owners.
multi_source_supervisor now binds exactly 1..16 already-composed source_session_port owners and
advances one non-blocking source slot per call with bounded round-robin selection. A source
fault is reported and drained without stopping healthy slots; a global stop is latched for
all slots. The supervisor borrows sessions exclusively and performs no destructor shutdown,
thread creation, FW RPC construction, source resolution or BSP reset. See
multi_source_supervisor.md, camera_session.md and multi_source_configuration.md in
docs/architecture. Bounded FW RAW-reference resolution exists in the camera adapter. Authenticated registry
RPC, transactional owner construction, service main and full entitlement remain missing. camera_session implements this port for one model. The separate portable
multi_model_pump now receives one frame, applies fixed-capacity cadence and shares the owner
with every accepting graph. It does not own graph/FW lifecycle; a multi-model source session
now supplies that orchestration: all graphs are preflighted before one FW acquisition,
started by stable model slot, then drained/unloaded before source release. See
docs/architecture/multi_model_pump.md and docs/architecture/multi_model_session.md.

`vqec_vision_perception_result_stage` reconstructs exact source frame identity from a
completed submission ticket, validates tensor pipeline PTS, then composes the configured
decoder and tracker transactionally. Multi-model routing selects this stage by immutable
model slot.
