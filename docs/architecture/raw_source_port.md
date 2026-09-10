# Vendor-neutral RAW-source port

Status: interface and Camera compatibility implementation delivered; multi-platform
implementations and live Camera/board verification are pending. Linux socket/FD fixtures
pass natively on the QCS6490 target.

`raw_source_port` removes Camera Service types from source-session orchestration. It owns
one logical RAW source and one acquisition epoch through explicit start/receive/stop calls.
The interface exposes exact effective geometry/rational FPS, state and outstanding-reader
count. It does not expose whether FW obtained pixels from a sensor/ISP or RTSP decode.

`raw_frame` contains a small copied descriptor, an adapter-native signed handle and a shared
owner. On the current Linux adapter the handle is a DMA-BUF FD. Copying descriptor metadata
is not a pixel copy; the pixel allocation remains in FW memory. Every asynchronous model
submission must retain `owner_` until its proven hardware input completion. Closing or
resetting a handle/owner does not cancel a device read.

The current `source_lifecycle` implements the port by forwarding its existing serialized
state machine. Receive converts `shared_ptr<const received_frame>` to
`shared_ptr<const void>` without creating another ownership domain. This keeps the legacy
ACK bound to the original socket and defers ACK/FD close until the final reader releases.

The port intentionally does not define retry threads, queues, transport discovery,
authentication, profile fallback or source restart. The composition layer resolves an
authenticated route first; a per-source executor drives bounded retries and replacement.
No zero-copy claim is valid until the selected platform proves import, cache/fence and
completion behavior on board.
