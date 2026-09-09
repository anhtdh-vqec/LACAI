# FW RAW-source resolution

Status: adapter contract/source delivered; released FW registry RPC and Linux/board
verification are pending.

`raw_source_ref` is the only deployment-level input identity. Before session construction,
the composition root resolves it to a `raw_source_route` supplied by the FW integration
layer. The route contains only what the existing RAW adapter needs:

- the exact reference and logical `camera_id/channel_id` correlation;
- an absolute Unix `SOCK_SEQPACKET` attachment path;
- the expected producer UID for peer-credential validation;
- the pinned numeric NV12 ABI value used by the legacy frame-header decoder.

It intentionally contains no RTSP URI, credentials, demuxer, codec, decoder state or
Camera/Box discriminator. On AI Box, FW must finish RTSP decode before registering the RAW
route. On AI Camera, FW registers the post-ISP RAW route in the same shape.

## Bounded registry and composition

`static_raw_source_resolver` is a startup-only, fixed 16-slot registry. Adding a route
rejects duplicate references, duplicate logical source identities and duplicate socket
paths. Resolve is transactional and performs no per-frame work. Strings are acceptable
here because registry population and lookup occur once per activation revision, not on the
frame path.

The lifecycle composer cross-checks the resolved reference and logical IDs against the
validated deployment source. It then creates `camera_lifecycle_config` with exact request
IDs, route, producer identity, deployment-derived geometry/allocation bounds and FPS. The
legacy Camera1 response reports integer FPS, so this compatibility adapter rejects a
non-integral rational FPS instead of rounding it. Future versioned FW RAW contracts may
carry rational rates without changing the deployment contract.

The media receiver accepts the resolved socket path directly. It no longer reconstructs a
path from camera ID, which prevents hidden product topology in the receive path. The
released-FW helper is the only place that knows the current naming convention:
`<socket_dir>/0_third_ai[_camN].sock`. It rejects non-zero channel because the inspected FW
release does. This restriction belongs to that compatibility helper, not the neutral
deployment model or supervisor.

## Trust and lifecycle

The static registry is not an authentication mechanism. Production population must come
from a versioned/authenticated FW registry or a protected local compatibility binding.
The resolver does not open the socket; `frame_source` still validates `SO_PEERCRED` before
accepting frames. A profile or endpoint change requires a new deployment/source epoch and
a full drain/rebind; routes must never be swapped under a running session.

No route lookup proves DMA-BUF import, synchronization, zero-copy or source capacity. Those
remain separate BSP/FW memory-contract and board-admission gates.

After route composition, session code accesses the source only through the
[vendor-neutral RAW-source port](raw_source_port.md); it does not depend on the legacy
socket/control class.
