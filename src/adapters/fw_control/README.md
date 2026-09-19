# FW control adapters

Private adapter reserved for the legacy AI D-Bus compatibility server
(`SetModelEnabled`, `QueryModel`, `ListModels`, `ModelStateChanged`) and separately reviewed
future config/entitlement/health bindings.

- **Status:** logic-tested — application-manager, face-enrollment and usecase-control D-Bus
  adapters are isolated by authority domain and covered by their eSDK test targets
- **Layer:** adapters
- **Naming registry:** `fwctl`
- **Depends on:** [FW release compatibility](../../../docs/contracts/fw_release_compatibility.md)
- **Used by:** service control plane (future)

## Responsibility

- Expose only the reviewed external methods; no feature business rules.
- Enforce authorization for outputs and attributes, not only UI switches.

## Contents

| Path | Purpose |
|---|---|
| `app_manager/vqec_vision_app_manager_dbus.*` | Authenticated AI-owned application lifecycle facade |
| `enrollment/vqec_vision_face_enrollment_dbus.*` | Face-enrollment server delegating to `face_enrollment_port` |
| `enrollment/vqec_vision_image_path_authorizer.*` | POSIX enrollment image authorization with retained FD |
| `usecase/vqec_vision_usecase_control_dbus.*` | Compatibility desired-state facade over `usecase_control_port` |

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
