# Incremental application activation

This document defines the internal runtime boundary that applies one validated App Manager snapshot
without restarting unrelated application dependencies.

**Status:** source-delivered — planner, primary-slot and secondary-cascade deltas are
logic-tested; QCS6490 multi-app acceptance remains open.
**Layer:** runtime. **Source:** `n/a`.

## Responsibility

- Derive application consumers, feature instances and shared model references from complete trusted
  snapshots.
- Apply authority, feature and model-slot changes in a bounded serialized order.
- Preserve unrelated source, graph, tracker and temporal-feature state.
- Never invent hardware completion, mutate App Manager authority or hot-replace an incompatible
  artifact.

## Dependency identity and counts

The dependency key is not only `model_id`. It includes source/profile, target, model version and
artifact digest, semantic/output/preprocess contracts, tensor shape/type/quantization, cadence, ROI
and quality policy. Two consumers share only when every field that affects execution is equivalent.

For each effective `(source_id, app_id)` association, the resolver adds that application once to
each required root-model consumer set. `consumer_count` is the size of that derived set. Repeated
features inside one application cannot accidentally over-count the same root dependency.

An explicitly packaged secondary component is counted separately from its primary root. Its
identity includes the secondary model version/target/artifact/semantic/preprocess contract and the
immutable primary slot. A secondary graph is prepared for installed compatible applications but
remains stopped at count zero. The current production cascade owner admits one secondary graph per
source; a second conflicting secondary identity is rejected as unsupported instead of being
silently shared.

| Count transition | Runtime action |
|---|---|
| `0 -> 1` | Validate admission, then configure/load/bind/start the prepared slot after the first-frame gate |
| `N -> N+1` for `N > 0` | Retain the running slot; no graph lifecycle call |
| `N -> N-1` for `N-1 > 0` | Retain the running slot; remove only the application feature/output authority |
| `1 -> 0` | Stop new submissions for the slot, drain completion, release retained owners, unload the slot |

## Delta classes

| Class | Examples | Scope |
|---|---|---|
| Authority-only | output-scope reduction, entitlement revoke | Output gate first; no unrelated owner changes |
| Feature delta | desired toggle with already-running shared model, threshold/config change | One application feature owner and fan-out binding |
| Model delta | unique model `0 -> 1` or `1 -> 0` | One prepared model slot in one source session |
| Source-local replacement | model artifact/semantic identity changes, unsupported prepared capacity | Drain and replace that source owner set |
| Process/recovery boundary | uncertain DMA completion or platform-wide fatal state | Fail closed; external recovery, never fake hot replacement |

## Apply ordering

The control executor performs one transition at a time:

1. Validate a strictly newer complete snapshot and build the candidate dependency plan.
2. Build candidate feature processors, fan-outs and output policy off-path; reject the whole
   candidate if validation or allocation fails.
3. Publish the candidate output policy and feature bindings on the serialized executor. This
   removes revoked output before old model work can be delivered.
4. Close the execution gate and quiesce a changed secondary cascade before requesting its graph
   lifecycle transition. A `N -> N-1`, `N-1 > 0` transition makes no graph call.
5. Submit target primary-model masks to affected source sessions. Running shared slots continue
   normally.
6. Each session performs at most one bounded lifecycle step per progress call while its other
   active slots continue frame/result progress.
7. Reopen a secondary execution gate only after its graph reports `running`; an inactive root result
   retires the exact retained frame without alignment or secondary submission.
8. Publish the applied snapshot revision only when every requested slot and cascade is running or
   fully drained.
   A failed added slot faults only its consumer applications; an uncertain drain requires recovery.

The pipeline may still return a result accepted before disable. Its captured policy revision and
payload scopes are checked against the new output gate, so it cannot regain authorization by being
late.

## Prepared capacity and first-frame safety

At generation construction, the service may prepare neutral owners for installed, compatible and
admitted applications even when `desired=false`. Preparation may verify metadata and allocate
bounded host control objects, but it must not load QNN/HTP/DSP or allocate active tensor pools.
Only effective reference counts contribute active hardware admission.

An inactive slot can change `0 -> 1` only while another model keeps the source session live. The
valid source epoch/frame already satisfies the Qualcomm no-frame safety precondition. From the
all-off state, the service creates a fresh source generation and waits for a probe frame before any
accelerator activation.

## Failure and observability

The snapshot exposes current/desired model masks, per-slot consumer counts, transition phase,
failed slot, source epoch, applied snapshot revision and fallback reason. Required counters include
delta attempts/commits/rollbacks, graph load/drain/unload calls and forbidden unrelated source or
graph restarts.

Timeout never decrements a count or releases a retained buffer. A disable that cannot prove drain
completion remains `draining` or `recovery_required`; it does not publish `disabled` as hardware
fact even though output authority is already revoked.

## Limits and next work

- QCS6490 multi-app continuity and resource evidence are not yet complete.
- Model artifact updates and capacity additions use an explicit replacement path in the first
  implementation.
- More than one independent secondary graph on one source requires multi-consumer frame-retention
  admission and is rejected by this baseline; root-model sharing for the eighteen-app software
  ceiling is not subject to that restriction.
- Multi-model temporal joins need their own compatible feature-instance identity before sharing.

## See also

- [ADR 0012](../adr/0012_incremental_app_activation.md)
- [App Manager](app_manager.md)
- [Multi-model source session](multi_model_session.md)
- [Feature fan-out](feature_fanout.md)
- [FW usecase activation](../contracts/fw_usecase_control.md)
