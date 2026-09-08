# Qualcomm stream control and output polling

Current implementation: submit, internal ticket correlation and independent completion
are now integrated. Read [current lifecycle](qualcomm_submission_lifecycle.md).
The notes below describe prior slices; their no-submit/destructor limitations are
superseded for explicitly armed graphs by the bounded retention-domain contract.

Private frame_submission now implements the appsrc push transaction separately.
plugin_graph does not call it yet; do not bypass the graph's closed input boundary.
Its existing fallback destructor is pre-submission only. See
[submission primitive](frame_submission.md) for ownership and integration prerequisites.

Update: READY -> bind_source -> start_stream is now mandatory. Source geometry/FPS
must match the graph plan; bound colorimetry/chroma-site are applied to appsrc caps.
Unload clears the binding. See [source binding](source_binding.md) for evidence
requirements and limitations. This supersedes the missing-source-policy note below;
bounded input submission and failure/destructor recovery remain unimplemented.

This slice extends READY model loading with PLAYING, EOS/drain and appsink polling.
It intentionally still exposes NO input push API. That boundary remains closed
until root-memory completion is connected to submission_window and the source
sync/color contract is explicit. Thus it cannot yet run a camera inference job.

States: ready -> starting -> playing -> draining -> drained -> unloading -> configured.
start_stream accepts an ordered FLOAT32 model output contract and a per-result
byte budget, validated before mutating state. appsink async=false avoids waiting
for an input preroll before the caller can observe PLAYING. sync=false remains.
The appsrc uses block=false in preparation for future externally bounded admission;
block=false alone is NOT a bounded queue guarantee, so it is not exposed for pushing.
appsink enable-last-sample=false avoids a hidden retained output sample.

poll_state remains zero-wait with a 32-message drain budget. request_drain calls
appsrc end_of_stream once; it does not set NULL or flush. EOS is expected only in
draining. A drained state requires observed EOS and an empty appsink, not merely
that the EOS request was accepted. Caller polls outputs during draining. Output
sampling uses try_pull_sample(timeout=0), validates/copies to owned tensor_result
and drops the GstSample reference before returning. This will eventually be the
place to forward result completion to the job ledger; correlation is not wired yet.

The output API checks the caller's expected pipeline PTS BEFORE copying. It is not
a replacement for the ledger: caller must pass the current ticket's PTS, and future
input completion must be reported independently. Invalid shape/PTS/GAP/map failure
faults the graph; result destination stays unchanged. Success is a CPU copy, not
device/cache synchronization or model accuracy validation.

unload is rejected while starting/playing/draining. Request drain and consume queued
outputs first. Fault recovery still supports NULL unload only because no input API
exists; this allowance MUST be tightened before camera jobs can be submitted.
Destructor fallback likewise remains pre-submission only and may synchronously
block in SDK teardown. No output consumer thread or unbounded retry loop is added.

Tests currently cover guards on an empty graph. Vendor model start/EOS, caps and
bus fault integration are NOT tested. All new code remains unbuilt by request.
