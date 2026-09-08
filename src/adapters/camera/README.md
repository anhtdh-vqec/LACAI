# camera

Legacy third-stream media receiver source is now present (not built or board-tested).

- legacy_wire: portable native-endian 104-byte decoder with strict NV12 view validation;
  cold-path limits can be composed from a validated FW RAW deployment source. The released
  AI Camera binding is `third`; AI Box must expose the same consumer semantics after FW decode.
- frame_source: optional Linux SOCK_SEQPACKET/SCM_RIGHTS receiver with peer UID check.
- received_frame: shared completion ownership; final owner ACKs on the original session.
- camera_control: Start/Stop lease state machine with ambiguous-outcome reconciliation.
- dbus_rpc: optional private GIO binding for the current FW string-valued a{sv} contract.
- source_lifecycle: combined acquisition/connect/receive/drain/release for one cycle.
- src/app/camera_graph_pump connects this receiver to the Qualcomm graph while keeping
  this camera adapter free of GStreamer dependencies. No live FW inference run yet.

Read [combined lifecycle](../../../docs/architecture/camera_source_lifecycle.md).
Stop is explicit and returns pending while frame readers remain. A stopped lifecycle
is terminal; the supervisor supplies fresh request IDs for the next acquisition.

Read [control client contract](../../../docs/architecture/camera_control_client.md).
Build option VQEC_VISION_AI_ENABLE_CAMERA_DBUS enables the GIO transport independently
of the Linux media receiver. Pure state-machine tests do not require GIO or a real bus.

Read [implementation boundary](../../../docs/architecture/camera_legacy_adapter.md)
and [FW baseline](../../../docs/contracts/fw_camera_integration_requirements.md).
Caller must retain every frame owner until hardware completion, acquire the camera
lease before connect and release it only after all readers/sessions drain.
The process-level 1..16-source supervisor remains pending; one source lifecycle must be
shared by compatible model/feature consumers rather than recreated per consumer.
