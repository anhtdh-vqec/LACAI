# Feature stage

`feature_stage` is the portable serialized coordinator around one configured
`feature_processor_port`. The processor is constructed by composition from trusted
configuration; the stage validates the matching source/feature identity, revision and
resource ceilings before accepting observations.

Processing accepts only a valid tracked observation batch and a finite monotonic time.
The stage resets temporal processor state on the first source epoch and on each strictly
newer epoch. A stale or backward epoch is rejected and never reset into the processor.
The source-gap flag is explicit and does not itself reset state.

Processor output is built in a candidate batch and validated against the exact input
frame/geometry and activation configuration before publication. Any failure or exception
preserves the caller's previous event batch. Because a failed processor may already have
mutated temporal state, the stage faults that epoch and rejects further updates until a
strictly newer epoch resets successfully.

This stage does not establish entitlement, resource admission, event delivery or feature
effective state. The feature manager must gate activation and final output independently.
Concrete processors remain responsible for preallocated bounded working memory and their
algorithm-specific replay/golden evidence.

A reset attempt records its epoch and monotonic time before calling the processor.
Reset errors and exceptions quarantine that attempted epoch; neither same-epoch retry
nor rollback to an earlier epoch is allowed. Recovery requires a strictly newer epoch
and successful reset. No event batch is published after a failed reset.
