# Operations readiness — planned runbook requirements

This document lists the runbook requirements that must be replaced with tested commands for
the FW image before release. It is a planning checklist, not a production readiness probe.

**Status:** planned — the `vqec_ai_vision_applications` reference service harness and
service smoke test exist, but they are not a production FW readiness probe; live platform
operation remains open. **Layer:** docs. **Source:** `n/a`.

The `vqec_ai_vision_applications` reference service harness and service smoke test exist.
It is not a production FW readiness probe; live platform operation remains open. Before
release, replace this checklist with tested commands for the FW image.

## Responsibility

- Defines the readiness, fault, diagnostics and rollback topics the eventual runbook must
  cover.
- Keeps readiness distinct from process-alive and keeps documented recovery ahead of forced
  buffer recycling.
- Must not be treated as a tested runbook or a release acceptance result.

## Required runbook content

- Inventory: app/backend/model/SDK/image versions, active workload and config/license
  revision.
- Readiness distinct from process alive: model warmup, source connected, entitlement,
  admitted resources and event sink health.
- Startup failure: inspect structured reason; do not silently switch backend/model.
- Source loss: reconnect with backoff, epoch reset, expose gaps; verify counter behavior.
- Backend stall: stop admission, quarantine buffers, request documented BSP recovery;
  never force camera buffer recycling solely because a timeout elapsed.
- Overload: enforce admitted profile, bounded drops, expose age/thermal/copy metrics.
- Output outage: bounded retry/spool, dedup, quota/TTL alerts; respect entitlement revoke.
- Upgrade: verify coherent set, drain, activate, health-check; rollback coherent versions.
- Diagnostics: bounded logs/metrics; no face images/embeddings/keys by default.
- Release evidence: clean install, rollback, fault drills and agreed board soak report.

## Limits and next work

- Every item above is a requirement, not a tested command; live platform operation and a
  board soak report remain open.

## See also

- [operations](README.md)
- [FW release compatibility](../contracts/fw_release_compatibility.md)
- [FW control, outputs, entitlement and BSP handoff](../contracts/fw_control.md)
