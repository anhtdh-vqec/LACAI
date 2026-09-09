# Qualcomm stream control and output polling

Current source boundary — 2026-09-09. Submission, ticket correlation, independent
input/result completion and bounded retention are integrated into plugin_graph.
See [submission lifecycle](qualcomm_submission_lifecycle.md) for the normative ownership
contract. Source and synthetic test fixtures are delivered; no executed build or board
validation is claimed.

## Startup and submission

Configure the private graph, load to READY, bind the admitted source, then start_stream.
Source geometry/FPS must match the plan; bound colorimetry/chroma-site are applied to
appsrc caps. Unload clears binding. start_stream validates ordered FLOAT32 output specs
and a per-result byte budget. See [source binding](source_binding.md).

The graph uses frame_submission for reserve/wrap/commit/appsrc push. Arming reserves a
retention-domain slot before input access. One outstanding job per graph bounds application
submission; appsrc block=false alone does not bound hidden plugin allocations. The root
GstMemory retains the shared RAW owner through downstream reads; hardware correctness
still depends on the backend memory-retention/synchronization contract.

States include ready -> starting -> playing -> draining -> drained -> unloading ->
configured. appsink async=false avoids waiting for input preroll to observe PLAYING;
sync=false and enable-last-sample=false avoid clock waiting and last-sample retention.

## Results and drain

poll_state uses zero-wait state polling and a bounded 32-message bus drain. Result polling
uses try_pull_sample(timeout=0), validates current ticket PTS and ordered tensor metadata,
and copies bounded FLOAT32 output into owned tensor_result before releasing the sample.
Internal pipeline PTS is distinct from the original source timestamp. Successful extraction
is a CPU copy, not a device/cache synchronization or model-accuracy result.

The submission ledger tracks input and result completion independently. Invalid results
fault the graph without fabricating completion or releasing submitted readers. request_drain
sends EOS; EOS acceptance alone is insufficient. Drain requires actual output/input progress,
observed EOS and no outstanding jobs. Unload is guarded while jobs or active transitions remain.

Armed destruction transfers retained resources to the reserved supervisor-domain slot
instead of assuming NULL teardown cancels DMA. Explicit restore supports late reconciliation.
The four-slot domain must outlive retained work; a full domain rejects admission. Timeout,
disconnect and destructor execution are not BSP recovery. Vendor state calls may still
block internally; no bounded hardware teardown guarantee is supplied.

Synthetic standard-GStreamer lifecycle/ownership fixtures and graph guard test sources are
registered in CMake under the applicable options. Real plugin model start/EOS, negotiated
caps, SDK faults and board synchronization remain unverified. A service event loop and
model decoder are still missing.
