# lifecycle

Start/stop/drain/recovery orchestration boundary. Timeout is never hardware completion.

- **Status:** source-delivered bounded deployment JSON loader — supervision missing
- **Naming registry:** `life` (`dpcfg`)
- **Depends on:** `src/core/` deployment validation
- **Used by:** service startup and runtime composition

## Responsibility

- Load the bounded startup-only deployment JSON with 1..16 explicit sources.
- Call the neutral validator transactionally and preserve output on failure.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_deployment_config.cpp` | Bounded deployment JSON loader delegating to the neutral validator |

## Limits and next work

- Does not authenticate the document, resolve FW RAW sources/model catalogs, allocate pools
  or supervise sessions.
- Stop request, timeout, source disconnect and FD close are not completion.

## See also

- [Multi-source configuration](../../../docs/architecture/multi_source_configuration.md)
