# Legacy camera FD to private GStreamer memory bridge

## Completion observation update

wrap_tracked_frame additionally binds a submission ticket to a read_completion
observer. Its private owner wrapper retains the original frame owner. When the last
GstMemory branch releases that wrapper, its destructor drops the original owner FIRST,
then publishes an atomic release flag. The observer does not retain the input frame
and cannot publish completion itself. It forwards exactly one input completion to the
serialized submission_window when polled; it never changes result completion.

The observer must remain alive and be polled by the job executor; do not destroy it
before reconciliation. Callback threads only publish an atomic flag, never mutate
the non-thread-safe submission ledger. A failed wrap produces no observer/buffer;
the caller cancels the uncommitted reservation. Commit before submitting to a backend.
Ticket source PTS must match the descriptor; mapped pipeline PTS is applied to the
GstBuffer and DTS is left unavailable for this raw single-image path. Source timestamps
remain in the descriptor/ticket. The byte-view and plane layout are unchanged.

This notification proves release of this bridge's memory owners, NOT a hardware
fence and NOT release of unrelated application owners. Correctness still requires
the backend to retain GstMemory through device reads; Camera lifecycle separately
counts received_frame owners. A released callback cannot cure unsafe vendor behavior.
Input push remains unavailable until graph failure/destructor recovery is integrated.

This slice constructs a GstBuffer only. It does not submit, change graph state,
wait/import an acquire fence, or claim hardware completion/compatibility.
The optional bridge target uses standard GStreamer allocators/video libraries;
unit/contract fixtures do not need Qualcomm plugins or a model.

Input: neutral frame_descriptor, borrowed FD and shared const owner. The application
passes the raw_frame shared owner, never just its borrowed FD. The Camera adapter's
owner still retains the original received_frame and legacy ACK session. The Qualcomm
bridge has no camera-adapter dependency; core never sees GStreamer types.
The bridge validates geometry/layout/ranges and an explicit expected source profile.
It duplicates the FD with CLOEXEC and wraps the full allocation with GstDmaBufAllocator,
then sets the memory's valid view to memory_offset_bytes/view_size_bytes.
GstVideoMeta offsets remain relative to that view, not shifted again by mem_offset.
No mmap, pixel copy, color conversion or tensor allocation occurs here.

Ownership:

```text
GstBuffer -> root GstMemory -> qdata shared frame owner -> original frame FD + ACK session
                 ^
           shared memory child (parent ref)
```

The owner is attached to root GstMemory, not merely the initial GstBuffer.
Shallow buffer copies/reference sharing and gst_memory_share children must retain
that root. Final memory release destroys the attached shared owner; the original
received_frame ACK still waits for other application/branch owners, if any.
The allocator owns the duplicated FD; the camera frame owner owns the original FD.
Memory is marked read-only. This is an API contract, not a security boundary against
vendor code that bypasses mapping flags. No component may overlay/write shared input.

Important limitation: GstMemory lifetime is NOT independently a device completion
event. Every backend must retain the input memory/owner until its actual device
reads finish. A vendor that keeps only an integer FD after releasing memory violates
this contract. Timeout, pipeline error, EOS or a forced state transition do not prove
DMA stopped. A future graph lifecycle must enforce this before using the bridge.

Local Qualcomm evidence at dcb4b825:

- mlvconverter.c setup_composition retains input through mapped blit frames even
  after dropping the queue's buffer reference.
- transform calls compose then cleanup_composition; the selected FCV implementation
  performs compose synchronously and warns that async composition is unsupported.
- The FCV wait_fence/flush stubs are not recovery guarantees. These source findings
  inform the design but do not replace BSP sync sign-off or a board lifetime test.

Timestamp values are copied unchanged, including GST_CLOCK_TIME_NONE. Buffer offset
is not overloaded with buf_id/epoch. The retained owner contains frame correlation;
the future runtime must explicitly map source PTS to pipeline running time and carry
job identity to outputs. This bridge neither creates caps nor guesses colorimetry.

The graph now permits explicit READY model loading but no PAUSED/PLAYING or submit.
To stream, still implement source color/sync
binding, bounded submit, bus/error handling, negotiated caps, output extraction and
drain/recovery. The FW may independently recycle buffers on disconnect; this bridge
does not fix the FW-AI-01 production gate.

Contract test source uses an ordinary temporary file FD to test wrapping, views,
refcounts and close behavior. It is NOT proof that that FD is DMA-BUF or hardware
importable. Board test must use the actual FW allocator and verified completion path.
