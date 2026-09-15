# Model package registry

The model catalog declares what may run. The model package registry binds each immutable
catalog identity to the package metadata directory and executable model artifact selected
for one deployment. It removes the former production assumption that every catalog entry
uses one shared package directory and model library.

Schema version 1 contains one bounded `packages` array. Every entry repeats
`model_id`, `model_version`, `target_id` and `artifact_ref`; runtime validation requires an
exact match with one catalog model, rejects duplicates and requires complete coverage with
no extra entries. `package_dir` supplies that model's `io_manifest.json`, `decoder.json`
and referenced label file. `model_library` supplies the vendor artifact to the private
backend adapter.

The service accepts `--model-package-registry <json>`. The older
`--model-package <directory> --model-library <path>` pair remains a compatibility input
only when the catalog contains exactly one model. It is rejected for multi-model catalogs
and cannot be combined with the registry option.

Parsing is startup-only, strict on unknown keys and bounded by document depth, bytes,
entry count, identifier length and path length. Loading a registry does not authenticate
its file or the referenced artifacts. Product composition must authenticate the catalog
and registry, resolve immutable contained files and verify the catalog digest before
activation; a configured path alone is not trusted evidence.

Qualcomm production loads package metadata separately for every model identity and looks
up graph/processor owners by model ID instead of assuming source model-slot order equals
catalog order. Vendor-specific graph construction stays behind the production platform;
the registry and validation contract contain no QNN or GStreamer types.

The current owner still has one graph per catalog model. Sharing the same model across
multiple simultaneously active sources therefore needs a separate admission-aware graph
instance policy before that topology is production-safe.
