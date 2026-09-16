# fw_control

Private adapter reserved for the legacy AI D-Bus compatibility server
(`SetModelEnabled`, `QueryModel`, `ListModels`, `ModelStateChanged`) and separately reviewed
future config/entitlement/health bindings.

- **Status:** face-enrollment DBus adapter is source-delivered behind
  `VQEC_VISION_AI_ENABLE_FACE_ENROLLMENT_DBUS`. The usecase-control v1 adapter is
  source-delivered behind `VQEC_VISION_AI_ENABLE_USECASE_CONTROL_DBUS`; service-level live
  generation replacement remains pending. Legacy model-toggle compatibility methods
  remain planned.
- **Naming registry:** `fwctl`
- **Depends on:** [FW release compatibility](../../../docs/contracts/fw_release_compatibility.md)
- **Used by:** service control plane (future)

## Responsibility

- Expose only the reviewed external methods; no feature business rules.
- Enforce authorization for outputs and attributes, not only UI switches.

## Limits and next work

- Legacy task-enabled state is a compatibility flag, not a license grant.
- Read the FW release compatibility contract before implementing any external method.
- Face enrollment follows [FW face-enrollment contract](../../../docs/contracts/fw_face_enrollment.md)
  and delegates all mutations to the neutral `face_enrollment_port`.
- Usecase control follows [FW usecase-control contract](../../../docs/contracts/fw_usecase_control.md),
  delegates desired-plan mutations to `usecase_control_port`, and cannot grant entitlement.

## See also

- [FW control contract](../../../docs/contracts/fw_control.md)
- [FW release compatibility](../../../docs/contracts/fw_release_compatibility.md)
