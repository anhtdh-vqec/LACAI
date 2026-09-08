# admission

Admission by measured workload/camera/model/ROI/memory budgets; reject with reason.

`vqec_vision_activation_snapshot` now materializes a validated deployment/catalog into
fixed-capacity numeric source/model slots. This removes string lookup and container growth
from future per-frame scheduling while binding every index to exact revisions. It is a
cold-path source component, not the multi-source supervisor or board capability admission.
See docs/architecture/multi_source_configuration.md and model_catalog.md.
