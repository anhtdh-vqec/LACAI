# scheduler

`model_cadence_scheduler` provides allocation-free per-source rational cadence selection
for 1..16 model slots, including sequence-gap skip accounting and checked arithmetic. It is
integrated into multi_model_pump and multi_model_session; live execution remains unverified. See docs/architecture/model_cadence.md.
