# contracts

Neutral public C++17 descriptors and interfaces. No vendor, GStreamer or OpenCV types.
Frame descriptors, inference/source plans, submission ledger, tensor results/output
metadata and pure output policy are source-delivered, not built. Overlay/encoded-AU
contracts now exist, along with the encoder_backend lifecycle port and observation
batches. Decoder output may use zero track IDs; tracked/feature input requires nonzero
IDs. Bounded feature events now bind source/frame/config/schema/model provenance before
output routing. Concrete hardware encoder and feature algorithms remain pending.
Multi-source deployment/model-catalog contracts and their cross-admission rules are
source-delivered; they contain no vendor types or runtime credentials. Declaration method
names follow the logical owner rule in naming registry, independently of vqec_vision_
filenames.
