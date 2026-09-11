# Operations readiness — planned runbook requirements

The `vqec_ai_vision_applications` reference service harness and service smoke test exist. It is not a production FW readiness probe; live platform operation remains open.
Before release, replace this checklist with tested commands for the FW image.

- Inventory: app/backend/model/SDK/image versions, active workload and config/license revision.
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
