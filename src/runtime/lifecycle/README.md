# lifecycle

Start/stop/drain/recovery orchestration; timeout is not hardware completion.

The bounded startup-only deployment JSON loader is source-delivered. It accepts 1..16
explicit sources and calls the neutral validator transactionally. It does not authenticate
the document, resolve FW RAW sources/model catalogs, allocate pools or supervise sessions.
See docs/architecture/multi_source_configuration.md.
