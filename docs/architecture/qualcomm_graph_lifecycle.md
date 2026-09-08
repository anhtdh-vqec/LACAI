# Qualcomm graph model-load lifecycle (no streaming yet)

plugin_graph now exposes empty/configured/loading/ready/unloading/faulted states.
configure still constructs a NULL-state graph transactionally. Reconfiguration is
allowed only in configured (NULL) state; loading/ready/faulted graphs must explicitly
unload first. load_model requests READY; poll_state checks the transition without
waiting and drains at most 32 bus messages. No PAUSED/PLAYING or buffer submission
is exposed in this slice, hence no camera frames can be read by this graph yet.

READY completion means the plugins accepted the state transition and no error was
observed in that poll. It is not tensor negotiation, warmup, correctness, HTP execution
or guaranteed continuing health. Subsequent poll_state can still report a bus error.
After errors, only unload is allowed; fault state cannot silently become ready.

GStreamer set_state can synchronously load proprietary models or tear down SDKs.
These calls have NO hard time bound in this API; poll_state itself uses zero timeout.
Call model load/unload on a serialized backend worker, not on frame/control threads
that must remain responsive. No detached thread or fabricated cancellation is added.
An external supervisor must handle a stuck loader; process kill safety still requires BSP.

unload requests NULL and uses nonblocking state inspection. Errors do not authorize
destroying device resources while state is unsettled. In this pre-streaming slice the
destructor requests NULL as fallback because SDK resources may be loaded; it can block
inside vendor cleanup. Production must use explicit unload before destruction.
This is a documented current limitation, not compliance with a bounded shutdown SLA.
Do not extend this destructor fallback to graphs with submitted camera buffers.

Bus ERROR faults the graph; EOS is unexpected without streaming and faults it too.
Warnings are counted, not treated as success/failure by themselves. Last error records
bounded source name, domain/code and message (not vendor debug strings). Avoid logging
the message blindly: vendor errors may contain model paths. Callers poll repeatedly
when status is pending, without busy-spinning. Saturated bus drain returns pending
rather than declaring readiness while older errors might remain queued.

Tests in this slice cover unconfigured state guards without Qualcomm factories.
Real model load/missing model, SDK ABI failure, bus injection and unload stress remain
integration tests to run on a controlled plugin installation/board. No tests have run.
