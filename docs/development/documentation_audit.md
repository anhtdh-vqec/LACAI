# Documentation consistency audit — 2026-09-11

All Markdown under `docs/`, top-level README/AGENTS and module READMEs were searched
against `src/`, `include/`, `tests/` and `CMakeLists.txt`. Current source-delivered
items are described as such; historical evidence remains dated and is not promoted to
current HEAD evidence.

The current verified inventory is: runtime executor, reference service executable,
feature activation and slot mapping, model/feature/tracker registries, multi-source and
multi-model orchestration, Qualcomm plugin graph, output helpers, structural checker and
conditional eSDK CI workflow. The expanded eSDK configuration registers 65 tests and
passes 65/65 in this workspace.

Known open boundaries are intentionally retained in architecture and contract documents:
real Camera/FW registry and stream, authenticated artifact/path resolution, concrete
model decoder/tracker/usecase packages, live QNN model load/HTP completion, renderer/
encoder/ring integration, durable event transport, BSP recovery, packaging, and board
performance/soak. These are implementation gates, not documentation omissions.

Numbers in dated evidence files may refer to the configuration available on that date.
When a document says “missing”, its scope is the named production boundary; it does not
mean the reference harness or neutral contract is absent. The source of truth for the
next work sequence is [base audit](base_audit.md) and [base completion plan](../planning/base_completion_plan.md).

The audit also records a limitation of the current structural checker: it verifies file
names, quoted includes and CMake paths, but not literal semantics, ABI, ownership,
threading, synchronization or model correctness. Those require review and targeted
runtime evidence.
