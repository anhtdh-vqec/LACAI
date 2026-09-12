# fw_control

Private adapter reserved for the legacy AI D-Bus compatibility server
(`SetModelEnabled`, `QueryModel`, `ListModels`, `ModelStateChanged`) and separately reviewed
future config/entitlement/health bindings.

- **Status:** planned — not implemented
- **Naming registry:** `fwctl`
- **Depends on:** [FW release compatibility](../../../docs/contracts/fw_release_compatibility.md)
- **Used by:** service control plane (future)

## Responsibility

- Expose only the reviewed external methods; no feature business rules.
- Enforce authorization for outputs and attributes, not only UI switches.

## Limits and next work

- Legacy task-enabled state is a compatibility flag, not a license grant.
- Read the FW release compatibility contract before implementing any external method.

## See also

- [FW control contract](../../../docs/contracts/fw_control.md)
- [FW release compatibility](../../../docs/contracts/fw_release_compatibility.md)
