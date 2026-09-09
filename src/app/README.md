# app

vqec_vision_encoder_preparation.cpp composes portable encoder admission with CPU pool
acquisition/rollback and explicit cancellation before submission. It is a separate
host-compatible target without Camera/GStreamer requirements. No encoder push or
rendering is added. See docs/architecture/encoder_preparation.md; tests are unexecuted.

Composition wiring belongs here; no feature rules in main.

camera_graph_pump now connects an already-started raw_source_port to a configured,
bound PLAYING plugin_graph, arms from the first receiver epoch and polls/submits one
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
docs/architecture. FW RAW-source resolution, service main and entitlement remain
unimplemented. camera_session implements this port for one model; multi-model frame fan-out
is the next source-session implementation and must not reacquire RAW input per model.
