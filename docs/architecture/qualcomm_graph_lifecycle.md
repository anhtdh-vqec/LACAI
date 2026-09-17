# Qualcomm graph model-load lifecycle

This document defines the model-load portion of the Qualcomm graph: the NULL/READY states, the
non-waiting transition polling, unload guards and bus-error handling. Streaming, submission and
armed retention are covered by their own documents.

**Status:** board-smoke — synthetic lifecycle logic and the standard-GStreamer fixture pass in the
117/117 QCS6490 target native result (2026-09-16); real model loading remains unqualified.
**Layer:** adapters. **Source:** `src/adapters/qualcomm/vqec_vision_plugin_graph.{hpp,cpp}`,
`vqec_vision_inference_graph.{hpp,cpp}`.

## Responsibility

- Construct and reconfigure the graph transactionally in NULL state.
- Load to READY and poll bus transitions without blocking or waiting.
- Guard unload on drained jobs and settled state.
- Do not claim tensor negotiation, warmup, correctness or hardware cancellation from READY.

## Model-load states

The model-load states include `empty`/`configured`/`loading`/`ready`/`unloading`/`faulted`.

- `configure` still constructs a NULL-state graph transactionally. Reconfiguration is allowed only
  in configured (NULL) state; loading/ready/faulted graphs must explicitly unload first.
- `load_model` requests READY; `poll_state` checks the transition without waiting and drains at most
  32 bus messages.
- READY source binding, PLAYING and bounded buffer submission follow through the integrated
  streaming lifecycle; see [stream control](qualcomm_stream_control.md) and
  [submission lifecycle](qualcomm_submission_lifecycle.md).

READY completion means the plugins accepted the state transition and no error was observed in that
poll. It is not tensor negotiation, warmup, correctness, HTP execution or guaranteed continuing
health. Subsequent `poll_state` can still report a bus error. A fault cannot silently become ready.
Submitted work must still reconcile actual input/result completion before guarded unload; a bus
error does not cancel hardware access.

## Blocking set_state

GStreamer `set_state` can synchronously load proprietary models or tear down SDKs. These calls have
NO hard time bound in this API; `poll_state` itself uses zero timeout. Call model load/unload on a
serialized backend worker, not on frame/control threads that must remain responsive. No detached
thread or fabricated cancellation is added. An external supervisor must handle a stuck loader;
process kill safety still requires BSP.

## Unload and armed retention

`unload` requests NULL and uses nonblocking state inspection. Errors do not authorize destroying
device resources while state is unsettled or jobs remain outstanding. Unarmed teardown can request
NULL and block inside vendor cleanup. An armed graph instead uses its reserved retention-domain slot
on destruction; restore permits late reconciliation. Production must explicitly drain/unload and
retain owners until quiescence is established. This is not a bounded shutdown SLA or BSP
cancellation mechanism.

## Bus messages

- Bus ERROR faults the graph; EOS is accepted during draining/drained and faults the graph when
  unexpected outside that lifecycle.
- Warnings are counted, not treated as success/failure by themselves.
- Last error records bounded source name, domain/code and message (not vendor debug strings). Avoid
  logging the message blindly: vendor errors may contain model paths.
- Callers poll repeatedly when status is pending, without busy-spinning. Saturated bus drain
  returns pending rather than declaring readiness while older errors might remain queued.

## Limits and next work

- Test source covers unconfigured guards; separate standard-GStreamer fixtures cover
  submission/drain/retention without Qualcomm models.
- Real model load/missing model, SDK ABI failure, bus injection and unload stress remain
  integration tests to run with a pinned model bundle and controlled FW source.
- The standard-GStreamer lifecycle fixture is part of the 117/117 target native result recorded in
  [QCS6490 board evidence](../testing/qsc6490_board.md).

## See also

- [Qualcomm stream control and output polling](qualcomm_stream_control.md)
- [Qualcomm submission lifecycle and retained faults](qualcomm_submission_lifecycle.md)
- [Qualcomm plugin adapter reference](qualcomm_plugin_adapter_reference.md)
- [QCS6490 board evidence](../testing/qsc6490_board.md)
