# models

Model integration metadata and verified artifact references/checksums. Do not commit model binaries.

output_fixture.json is synthetic parser input only. The digest/decoder are placeholders;
there is no corresponding released artifact. Never install it as production configuration.
Use the optional vqec_vision_ai_manifest_check executable for read-only syntax/metadata
validation, not artifact authentication. See docs/architecture/model_output_manifest.md.

model_catalog.example.json is the synthetic AI Model-team catalog paired with the
deployment example. It demonstrates input/preprocess, supported source envelope, cadence,
concurrency and memory declarations. Placeholder digest/resource numbers are not release
evidence. See docs/architecture/model_catalog.md.
