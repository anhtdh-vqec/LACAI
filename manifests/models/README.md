# models

Model integration metadata and verified artifact references/checksums. Do not commit model binaries.

- **Status:** synthetic examples only

| File | Purpose |
|---|---|
| `output_fixture.json` | Synthetic parser input; digest/decoder are placeholders |
| `model_catalog.example.json` | Synthetic Model-team catalog paired with the deployment example |

Never install either as production configuration. Use the optional
`vqec_vision_ai_manifest_check` executable for read-only syntax/metadata validation, not
artifact authentication.

## See also

- [Model output manifest](../../docs/architecture/model_output_manifest.md), [model catalog](../../docs/architecture/model_catalog.md)
