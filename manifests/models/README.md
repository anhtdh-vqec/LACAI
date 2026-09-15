# models

Model integration metadata and verified artifact references/checksums. Do not commit model binaries.

- **Status:** real-metadata packages (board-probe ABI, no golden yet) plus synthetic examples

| File | Purpose |
|---|---|
| `output_fixture.json` | Synthetic parser input; digest/decoder are placeholders |
| `model_catalog.example.json` | Synthetic Model-team catalog paired with the deployment example |
| `model_catalog.face.example.json` | Face detector catalog with the SCRFD package binding |
| `yolov8n_person/` | Approved YOLOv8n-person kit (M0–M4) |
| `scrfd_500m_bnkps/` | SCRFD face detector package (M0; runtime ABI from the QCS6490 probe) |
| `edgeface_s_gamma_05/` | EdgeFace embedding package (M0; decoder/alignment are M4/M5) |
| `VERIFICATION_CHECKLIST.md` | Per-model fields to verify before a package is accepted |

The SCRFD and EdgeFace directories hold metadata only; their `.so` artifacts stay outside
Git. They are not artifact authentication: a digest match is not signature verification.
EdgeFace is not bindable in a production catalog until the M5 embedding decoder exists.

Never install a synthetic example as production configuration. Use the optional
`vqec_vision_ai_manifest_check` executable for read-only syntax/metadata validation, not
artifact authentication.

## See also

- [Model output manifest](../../docs/architecture/model_output_manifest.md), [model catalog](../../docs/architecture/model_catalog.md), [face recognition plan](../../docs/planning/face_recognition_completion_plan.md)
