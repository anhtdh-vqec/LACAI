# Hardware admission profile

This startup schema supplies an explicit resource ceiling to runtime composition. It is a
traceable input to admission, not a claim that the limits have been measured or approved.

**Status:** logic-tested — strict loader and composition tests pass under eSDK/QEMU;
no approved QCS6490 profile exists. **Layer:** runtime.
**Source:** `src/runtime/admission/vqec_vision_hardware_admission_profile.{hpp,cpp}`,
`src/runtime/admission/vqec_vision_activation_snapshot.{hpp,cpp}`.

## Responsibility

- AI APP parses one bounded profile, rejects invalid or absent Qualcomm input and
  estimates resources against the declared ceiling before composing owners.
- BSP+FW owns board/FW concurrency, DMA/encoder pool and thermal measurements; AI Model
  owns model footprint and validated inference workload. AI APP lead reviews the combined
  envelope and its evidence reference.
- The parser does not authenticate the file, establish a signature, measure hardware or
  authorize a feature. Trusted provisioning of the file remains an external prerequisite.

## Schema and validation

Supply `--hardware-profile <path>` for `--mode production --platform qualcomm`. The file
is a single JSON object with exactly these fields; unknown/duplicate keys fail closed.
`schema_version` is the unsigned integer `1`, `revision` must be positive, strings must
be nonempty and at most 256 bytes without NUL, and numeric limits must be unsigned and
within their 32-/16-bit field width when applicable:

| Field | Unit / meaning |
|---|---|
| `profile_id`, `target_id`, `revision`, `measurement_reference` | Immutable identity, exact model target, generation and human-reviewable evidence reference |
| `max_total_resident_bytes` | Aggregate candidate memory ceiling (bytes) |
| `max_frame_pool_bytes`, `max_tensor_pool_bytes` | Corresponding source/model pool ceilings (bytes) |
| `max_encoder_pool_bytes`, `max_cascade_roi_bytes` | Preview and cascade ceilings (bytes) |
| `max_ddr_bandwidth_mbps` | Estimated DDR load ceiling in MiB/s as currently computed |
| `max_fw_concurrency_slots`, `max_worker_concurrency` | Concurrent source and worker ceilings |
| `min_thermal_headroom_pct` | Estimated minimum percent, inclusive range 0–100 |

All maxima must be positive. The loader reads at most 16 KiB, validates a depth limit and
preserves the caller's prior value on failure. The activation snapshot records the exact
supplied profile and rejects a catalog model whose `target_id` differs. The service's
fake/reference modes synthesize an explicitly named fixture; it must not be deployed as a
Qualcomm profile. A file with a plausible `measurement_reference` string is not itself a
signed or approved measurement.

## Measurement and acceptance

Before product use, the three teams must pin a workload and profile revision, capture
real memory allocations including sealed model copies, QNN/encoder/FW surfaces, gallery/
index, process overhead, source/frame concurrency, DDR and sustained thermal measurements,
and record units, headroom and owner approvals. On a profile revision change, recompose
after draining the old generation; never change limits in place beneath live owners.

## Limits and next work

- Current DDR and thermal estimates are heuristic, not direct instrumentation; pool and
  index/FR accounting is incomplete. Rejection proves only that a declared bound was
  exceeded, not that an admitted workload is safe under thermal/FW coexistence.
- The file is opened by path at startup without signed provenance verification. Provision
  it through an authenticated authority and version it with the deployment/model catalog.
- No accepted native `.98` profile or released-FW coexistence evidence is recorded.

## See also

- [Multi-source configuration](multi_source_configuration.md)
- [Runtime composition factory](runtime_composition_factory.md)
- [Plan 0 review](../development/production_composition_foundation_review.md)
