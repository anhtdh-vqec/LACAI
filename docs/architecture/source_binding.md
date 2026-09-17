# Source binding before streaming

Source binding binds the effective FW RAW profile to a READY graph before streaming. It is
a vendor-neutral trusted deployment policy that removes implicit color/sync assumptions
without introducing a new per-frame message or changing the Camera Service wire.

**Status:** source-delivered — source implementation contract; AI APP lead and BSP
review/sign-off pending. Tests cover policy rejection, bounds, equivalent FPS and the
empty-graph binding guard, but do not prove DMA interoperability, color accuracy or board
compatibility. **Layer:** core. **Source:**
`src/core/vqec_vision_source_binding.cpp`,
`tests/unit/vqec_vision_source_binding_test.cpp`.

## Responsibility

- Binds the effective FW RAW profile to a READY graph before `start_stream`.
- Requires geometry and rational FPS to match the inference plan.
- Rejects rather than mislabels unsupported memory or caps.
- Must not resize, reconfigure the source or infer color during binding.
- Must not change binding while in PLAYING.

## Binding rules

The composition root must bind the effective FW RAW profile to a READY graph. The current
AI Camera release names this stream `third`; AI Box must normalize its inputs before this
boundary. Before `start_stream`, geometry and rational FPS must match the inference plan.
No resizing, source reconfiguration or color inference happens during binding.
A rejected binding preserves the previous binding and graph state. Binding cannot
change in PLAYING. NULL unload clears it, requiring a fresh bind before the next run.

`source_binding` is vendor-neutral and describes a trusted deployment policy, NOT
metadata discovered from the legacy 104-byte packet. Missing values fail closed.
Initial policy accepts linear NV12 DMA-BUF, implicit readiness, and explicit
BT.601 or BT.709 limited-range colorimetry with MPEG-2 or JPEG chroma siting.
Full range, UBWC/opaque FD and explicit-fence modes are not implemented; reject
them instead of mislabeling memory/caps. Numeric GstVideoFormat still belongs to
the legacy decoder, not this policy.

## Mandatory contract references

Three bounded printable reference IDs are mandatory, supplied by trusted local
configuration, never arbitrary remote flags:

- `fw_memory_contract_`: BSP/FW evidence for the allocator being importable DMA-BUF,
  the producer publishing ready pixels, cache/device ordering, and no pool reuse
  until all AI readers finish, including disconnect/recovery behavior.
- `backend_memory_contract_`: exact plugin/BSP evidence for retaining input memory
  through all device reads and making tensor output CPU-visible before sample mapping.
- `preprocess_contract_`: model-team golden acceptance of the exact input colorimetry,
  chroma siting, FastCV conversion, placement, channel order and normalization plan.

Validation only checks policy shape and reference syntax. It does NOT verify a
signature, read reports, execute fences/cache operations or certify the hardware.
The future trusted platform/model registry resolves these IDs against approved
artifacts; it must not auto-fill placeholder IDs in production. A bounded multi-source
deployment parser/schema now exists, but the example is not an approved product
configuration and trusted artifact resolution is still pending. See
[multi-source configuration](multi_source_configuration.md).

## Colorimetry labeling versus conversion

The Qualcomm graph advertises the bound colorimetry/chroma-site on appsrc caps.
This labels pixels; it does not convert them or prove that FastCV honors arbitrary
color matrices. In the pinned `fcv-video-converter.c`, the NV12-to-RGB call uses a
fixed SDK conversion entrypoint without matrix/range arguments. A comment about
BT.601 in `fill_background` is not evidence of the input conversion coefficients.
Hence preprocess golden sign-off is a separate requirement, not inferred from caps.

## Submission wiring

Input push is now connected through explicit `arm_submission` and a bounded retention
domain. See [graph submission lifecycle](qualcomm_submission_lifecycle.md). This binding
removes implicit color/sync assumptions but is still not hardware synchronization.

## Limits and next work

- AI APP lead and BSP review/sign-off remain pending.
- Full range, UBWC/opaque FD and explicit-fence modes are not implemented.
- Tests do not prove DMA interoperability, color accuracy or board compatibility.
- Trusted platform/model registry artifact resolution remains pending.

## See also

- [Multi-source configuration](multi_source_configuration.md)
- [Graph submission lifecycle](qualcomm_submission_lifecycle.md)
- [RAW-source port](raw_source_port.md)
