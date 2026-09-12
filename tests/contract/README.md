# contract

Cross-module contract-test source for the camera receiver/session/pump, the GStreamer
bridge/graph/submission/tensor path, encoders, perception, feature and output boundaries.

- **Status:** source-delivered — built and executed under the eSDK QEMU configurations
- **Depends on:** neutral ports and the reference backend

## Limits and next work

- These fixtures do not prove real FW reader/writer ABI, hardware completion or RTSP/UI compatibility.
- Add FW01–FW09 acceptance coverage from
  [FW release compatibility](../../docs/contracts/fw_release_compatibility.md) during replacement slices.
- Fake sinks and graphs prove wiring and lifecycle only.

## See also

- [Review checklist](../../docs/development/review_checklist.md)
