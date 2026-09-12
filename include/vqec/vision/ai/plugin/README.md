# plugin

Versioned C-compatible ABI for independently packaged trusted modules.

- **Status:** planned — not implemented
- **Naming registry:** `plug`
- **Requirements:** opaque handles, explicit struct size and ABI major/minor, no STL objects
  or exceptions crossing the boundary

## Limits and next work

- No plugin is loaded or packaged yet.
- Do not expose provider types or C++ standard-library objects across this ABI.

## See also

- [System architecture](../../../../../docs/architecture/system_architecture.md)
