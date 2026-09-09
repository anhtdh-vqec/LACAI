# ports

Vendor-neutral runtime interfaces used by application composition. No GStreamer, QNN,
FastCV, Camera Service wire or product-origin types may cross this directory.

`vqec_vision_raw_source.hpp` defines one logical FW RAW-source lifecycle and a moveable
metadata/native-handle/shared-owner envelope. The current Camera adapter implements it;
other FW/platform adapters must preserve the same ownership and epoch semantics.

`vqec_vision_inference_graph.hpp` defines neutral model-graph lifecycle and shared-frame
submission. Qualcomm implements it privately; application code must not call plugin APIs.
