# models

Model integration metadata and verified artifact references/checksums. Do not commit model binaries.

- **Status:** real-metadata packages (board-probe ABI, no golden yet) plus synthetic examples

| File | Purpose |
|---|---|
| `output_fixture.json` | Synthetic parser input; digest/decoder are placeholders |
| `model_catalog.example.json` | Synthetic Model-team catalog paired with the deployment example |
| `model_catalog.face.example.json` | Face chain catalog: SCRFD primary plus EdgeFace secondary dependency |
| `model_catalog.person_face.example.json` | Combined person and face catalog used to prove concurrent overlay composition |
| `yolov8n_person/` | Approved YOLOv8n-person kit (M0–M4) |
| `scrfd_500m_bnkps/` | SCRFD face detector package (M0; runtime ABI from the QCS6490 probe) |
| `edgeface_s_gamma_05/` | EdgeFace-S gamma=0.5 embedding package with decoder/alignment metadata |
| `VERIFICATION_CHECKLIST.md` | Per-model fields to verify before a package is accepted |

The SCRFD and EdgeFace directories hold metadata only; their `.so` artifacts stay outside
Git. They are not artifact authentication: a digest match is not signature verification.
The face catalog assigns only SCRFD to the full-frame source mask. EdgeFace is activated
through its dependency and is never listed in deployment `model_ids`. The matching face
deployment example is `config/defaults/deployment.face.example.json`. Its resource values
are conservative planning envelopes, not measured board capacity.

`model_catalog.person_face.example.json` pairs with
`config/defaults/deployment.person_face.example.json`. The deployment assigns both
YOLOv8n-person and SCRFD to the source, so the overlay merger can retain person boxes while
the SCRFD/EdgeFace chain adds face boxes and recognition labels. It is still an example;
commercial usecase selection must derive an effective deployment before model loading.

Never install a synthetic example as production configuration. Use the optional
`vqec_vision_ai_manifest_check` executable for read-only syntax/metadata validation, not
artifact authentication.

## See also

- [Model output manifest](../../docs/architecture/model_output_manifest.md), [model catalog](../../docs/architecture/model_catalog.md), [face recognition plan](../../docs/planning/face_recognition_completion_plan.md)
