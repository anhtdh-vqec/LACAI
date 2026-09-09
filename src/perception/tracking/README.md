# tracking

Per-source tracking state, association and ID continuity; source epoch resets.

`tracker_port` defines the neutral boundary after model decoding. Implementations receive
explicit monotonic time and source-gap information, return observation batches with track
IDs and reset state on a new source epoch. Qualcomm/GStreamer types and association
algorithms stay behind the implementation boundary.

`vqec_vision_tracking_stage` is the serialized portable coordinator. It validates
detections, resets the tracker on a new source epoch, enforces monotonic process time and
publishes only validated tracked batches. An ambiguous update failure faults that epoch;
only a successful reset for a later epoch permits processing to resume.

`tracker_registry` resolves an activation-supplied tracker contract to a borrowed
factory and creates one tracker owner for each source/model binding. It is bounded and
keeps implementation and vendor choices behind the neutral `tracker_port` boundary;
composition remains responsible for authenticating the binding and selecting the
contract.
