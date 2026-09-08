# scheduler

`model_cadence_scheduler` provides allocation-free per-source rational cadence selection
for 1..16 model slots, including sequence-gap skip accounting and checked arithmetic. It is
not yet wired to multi-graph submission. See docs/architecture/model_cadence.md.
