# outputs

vqec_vision_encoded_dispatch.cpp now provides synchronous one-AU dispatch through
output_gate, freshness/correlation and ring generation/demand checks. No internal queue,
retry. An optional private FW ring sink exists but is not runtime-wired.
Trusted renderer must supply complete scopes bound to pixels;
that integration is missing. See docs/architecture/encoded_dispatch.md.

Core now provides immutable owned H264 output and a neutral synchronous encoded_sink
port. Runtime routing/queue admission and FW sink lifecycle wiring remain missing; see
docs/architecture/encoded_output.md. No real output delivery is available yet.

Neutral overlay commands, preview demand/freshness policy, encoded-AU routing and
event delivery. AI owns preview overlay/encoding/ring production through private adapters;
FW owns RTSP/UI, recording, persistent evidence/search. End-to-end output is not implemented.
See docs/contracts/fw_release_compatibility.md and docs/planning/fw_compatibility_execution.md.
