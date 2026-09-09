# outputs

vqec_vision_encoded_dispatch.cpp now provides synchronous one-AU dispatch through
output_gate, freshness/correlation and ring generation/demand checks. No internal queue,
retry. It also supplies one-event backend polling/preflight and dispatch/ledger handling;
delivery status is separate from event completion. An optional private FW ring sink exists
but is not runtime-wired into a service event loop.
Trusted renderer must supply complete scopes bound to pixels;
that integration is missing. See docs/architecture/encoded_dispatch.md.

Core now provides immutable owned H264 output and a neutral synchronous encoded_sink
port. Runtime routing/queue admission and FW sink lifecycle wiring remain missing; see
docs/architecture/encoded_output.md. No real output delivery is available yet.

Neutral overlay commands, preview demand/freshness policy, encoded-AU routing and
event delivery. AI owns preview overlay/encoding/ring production through private adapters;
FW owns RTSP/UI, recording, persistent evidence/search. End-to-end output is not implemented.
See docs/contracts/fw_release_compatibility.md and docs/planning/fw_compatibility_execution.md.

`vqec_vision_overlay_preparation.cpp` provides the portable metadata step before a
renderer adapter. It authorizes the complete requested scope, validates observations,
maps boxes to overlay metadata and publishes transactionally. It does not render pixels
or select a Qualcomm plugin.
The authorized variant returns the exact scope list used for authorization together with
the overlay, so encoded dispatch can reuse trusted scope context without inferring it
from pixels or H264 bytes.
The scoped variant supports several feature/attribute authorizations under one policy
revision and rejects mixed revisions or more than the shared rendered-scope ceiling.
