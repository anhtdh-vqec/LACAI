# Hardware admission profile

This startup schema supplies an explicit resource ceiling to runtime composition. It is a
traceable input to admission, not a claim that the limits have been measured or approved.

**Status:** board-smoke — schema and single-source observation profile cross-tested;
QCS6490 `.98` workload observations recorded below. **Layer:** runtime.
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

The service rejects a preview surface count that differs from the deployment's admitted
`preview_surface_count` before constructing vendor owners. An independent CLI pool size
must not silently bypass the encoder envelope.
Tensor-pool admission sums each source's declared `max_tensor_bytes`; model-context
resident bytes are accounted separately. Model context bytes must not stand in for tensor
storage. The example's tensor and worker ceilings match the actual candidate configuration.

Before product use, the three teams must pin a workload and profile revision, capture
real memory allocations including sealed model copies, QNN/encoder/FW surfaces, gallery/
index, process overhead, source/frame concurrency, DDR and sustained thermal measurements,
and record units, headroom and owner approvals. On a profile revision change, recompose
after draining the old generation; never change limits in place beneath live owners.

## QCS6490 .98 observation 2026-09-17

AI APP collected these observations on the user-authorized Qualcomm RB3 Gen2 vision
mezzanine board, QCS6490, Qualcomm Linux 1.8, kernel `6.6.119`. This is a compatibility-source
smoke, not released-Camera-Service coexistence or a maximum-capacity benchmark.
The profile identity is `qcs6490_rb3gen2_single_source_observed`, revision 1.

Workload: one 1920x1080 NV12 source at 30 FPS; YOLOv8n person and SCRFD primary graphs,
exact-frame FastCV alignment and EdgeFace secondary embedding, portable IoU tracking,
QTI overlay/H.264 ring preview at 4 Mbit/s, two admitted preview surfaces, protected-gallery
and derived-index configuration. Models and biometric fixtures are not repository assets.
The final candidate and native results are pinned in the [Plan 0 review](
../development/production_composition_foundation_review.md).

| Observation | Method and result | Interpretation |
|---|---|---|
| System RAM | `/proc/meminfo`: `MemTotal=5496292 kB` | OS-visible RAM, not AI-exclusive capacity |
| CPUs | Eight online processors | Not eight guaranteed free AI workers |
| Process memory | Final ten live samples: maximum `VmHWM=382144 kB`, `VmRSS=380316 kB`, 47–48 threads; earlier four-surface diagnostic HWM `384816 kB` | Process-resident pages, not all device allocations |
| Thermal | Final CPU/DDR/camera samples 57.7–59.3 C; initial 51–53 C | Short run; earlier live sample 54.6–56.6 C, not sustained thermal acceptance |
| Trip points | CPU: 110/118/125 C; DDR: 118/125 C; camera: 100/118/125 C, from thermal sysfs | Reported thresholds, not an AI thermal-control contract |
| CPU memcpy | Native `perf bench mem memcpy -l 1000 -s 16MB`: `5.431499 GB/s` | Cached CPU-copy benchmark, not a measured sustained DDR bandwidth budget |
| Copy-test thermal | CPU/DDR/camera from 51.2/51.5/51.5 C to 57.7/57.0/55.4 C | Stress-test context only |

The JSON values are conservative startup ceilings for this observed workload. They are
not all direct hardware measurements. Fields that cannot be attributed to a device counter
use the actual validated workload's declared envelopes, with that limitation explicit:

| Profile field | Value | Provenance |
|---|---|---|
| Total resident | 512 MiB | Actual deployment ceiling, above final sampled 373.2 MiB process HWM; not total system RAM |
| Frame pool | 8 MiB | Deployment: two admitted frames at a 4 MiB maximum allocation each |
| Tensor pool | 32 MiB | Actual source `max_tensor_bytes` budget, covering declared model tensors; not a measured QNN allocation trace |
| Encoder pool | 6220800 bytes | Two packed 1920x1080 NV12 surfaces; GPU padding, ring and vendor internal surfaces are additional overhead |
| Cascade ROI | 16 MiB | Actual candidate's configured bounded cascade byte budget |
| DDR estimate | 1200 MiB/s | Startup estimated-load ceiling for the candidate; memcpy is only context and does not prove this limit |
| FW concurrency | One source | Only one source was exercised; additional sources fail admission |
| Worker concurrency | Four | One source + two root models + one cascade scheduler slot; not process thread count or measured NPU parallelism |
| Thermal headroom | 20 percent minimum | Conservative policy input; runtime estimate is not live thermal feedback |

Do not multiply this profile for traffic or multi-source workloads. Re-measure with the
actual models, source rate, preview demand, gallery size and concurrent FW recording/encoding.
Do not claim measured DSP preprocessing, end-to-end zero-copy, CPU-target attainment or
long-duration thermal safety from this record.

## Limits and next work

- Current DDR and thermal estimates are heuristic, not direct instrumentation; pool and
  index/FR accounting is incomplete. Rejection proves only that a declared bound was
  exceeded, not that an admitted workload is safe under thermal/FW coexistence.
- The file is opened by path at startup without signed provenance verification. Provision
  it through an authenticated authority and version it with the deployment/model catalog.
- Native `.98` observations support this single-source startup baseline. Independent
  BSP+FW/AI Model owner acceptance, sustained DDR/thermal telemetry and released-FW
  coexistence remain Plan 1/5 gates; this is not a general accepted hardware-capacity profile.

## See also

- [Multi-source configuration](multi_source_configuration.md)
- [Runtime composition factory](runtime_composition_factory.md)
- [Plan 0 review](../development/production_composition_foundation_review.md)
