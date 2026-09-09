# tracking

Per-source tracking state, association and ID continuity; source epoch resets.

`tracker_port` defines the neutral boundary after model decoding. Implementations receive
explicit monotonic time and source-gap information, return observation batches with track
IDs and reset state on a new source epoch. Qualcomm/GStreamer types and association
algorithms stay behind the implementation boundary.
