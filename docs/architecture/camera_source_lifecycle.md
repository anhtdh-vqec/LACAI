# Combined Camera control/media lifecycle

source_lifecycle owns camera_control and frame_source for one acquisition cycle.
Constructor configuration is immutable: camera IDs must agree; Start/Stop request IDs
are distinct and provided by the supervisor. A completed object cannot restart;
create a new cycle with fresh request IDs after the previous cycle is fully stopped.
Frame correlation across objects must include the acquisition-cycle identity supplied
by the supervisor: receiver-local session_epoch restarts in each new receiver object.

```text
idle -> starting -> connecting -> running
          |             |          |
          +-------------+----------> draining -> releasing -> stopped
```

start performs at most one StartStream RPC and one nonblocking socket connect attempt.
If the socket is not ready, repeat start from connecting without another acquisition.
No internal sleep, retry thread or autonomous profile mutation is introduced.
running means connected, not proof that a first frame/model is ready.

receive accepts only running state and an empty output. The advertised profile must
fit configured resource limits. Frame dimensions must match the acquired profile;
a change is detected before delivering the new frame and triggers drain/release.
The supervisor then builds a new acquisition/graph for the new effective profile.
This implementation does not pretend caps renegotiation or source epoch detection
exists on the legacy wire. Same-size source discontinuities still need FW events.

stop is a progress operation: it permanently stops new receives and returns pending
while readers still own frames. It never waits for hardware or manufactures ACKs.
The caller releases readers only after real completion, then calls stop again.
Per-call timeout applies to its RPC, not the entire drain period. The supervisor owns
the overall shutdown deadline and BSP escalation; expiry is not permission to recycle.

frame_source now maintains a shared count across current and detached sessions and
enforces a total cap of four live frames per receiver, not four per reconnect.
Final received_frame destruction attempts ACK, closes its FD, releases its session
reference and only THEN decrements the cross-session count. Thus observing zero
does not race ahead of the old-session reference release. The receiver closes its
own current-session reference before control StopStream. Failed ACK remains a
transport fault; zero local readers does not assert that FW processed every ACK.

When StartStream's outcome is unknown and stop was requested, stop replays the same
Start request to recover the handle; it does not connect the raw socket. Once the
handle is known it returns pending, and a subsequent stop call releases it. This
keeps at most one RPC per stop call and prevents loss of an uncertain acquisition.
Malformed profile with valid handle still permits cleanup. Persistent reconciliation
errors remain visible; there is no force-forget path.

Source methods are serialized by the caller; frame shared owners may be destroyed
on completion threads. Destruction does not call StopStream. Destroying before
stopped violates the supervisor lifecycle contract and can leave a FW control lease;
outstanding frame owners still retain their socket references. Callers must inspect
the state and preserve/reconcile the object rather than using destructor as shutdown.

The [GstMemory wrapper](dmabuf_memory_bridge.md) now retains a shared frame owner;
the [camera pump](camera_graph_pump.md) now wires it into the graph submission path.
Still not included: verified hardware reader completion, bus-owner
change handling, durable crash recovery, BSP deadline escalation and live board validation.
The existing FW P0 requirements remain a production gate.
