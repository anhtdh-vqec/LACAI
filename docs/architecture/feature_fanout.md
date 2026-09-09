# Feature fan-out

`feature_fanout` is the portable per-source coordinator from one tracked observation
batch to a bounded set of already-activated feature stages. Activation binds 1..32 stable
numeric slots; the ceiling allows the current catalog and extensions without embedding a
commercial feature list or string lookup in the frame path.

One serialized call validates the tracked batch and monotonic time before invoking any
stage. Each stage receives the same immutable observations and source-gap flag. Processing
continues after a stage error so one feature cannot starve healthy features. Successful
slots publish their event batch; a failed slot preserves its previous output through the
feature-stage transactional contract.

The report records processed and failed bitmasks, one status code per configured slot and
the first failing slot. The function returns the first failure after all slots have been
advanced. Therefore outputs and report remain meaningful on a non-`ok` return; there is no
cross-feature atomic publication claim. Slots outside the configured count are untouched.

The fan-out borrows unique stage owners and performs no activation, entitlement, output
delivery or automatic replacement. Feature manager composition must keep slot identity
stable for its immutable configuration revision and replace the whole owner set for a new
revision.
