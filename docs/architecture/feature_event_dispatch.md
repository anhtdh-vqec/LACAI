# Feature event dispatch

`feature_event_dispatch` is the portable final authorization boundary for one feature
event. This document defines how it validates an event against its activation
configuration, checks output policy and hands the event to a sink.

**Status:** source-delivered — the dispatcher exists with contract tests. **Layer:**
outputs. **Source:** `src/outputs/events/vqec_vision_feature_event_dispatch.cpp`,
`tests/contract/outputs/vqec_vision_feature_event_dispatch_test.cpp`.

## Responsibility

- Validates the complete event batch against its activation configuration before delivery.
- Builds the authorization request from the event's actual field schema IDs so a caller
  cannot provide a smaller attribute list to hide sensitive payload fields.
- Calls `output_gate` immediately before the synchronous sink operation.
- Provides no queue, persistence, retry schedule, policy signature verification or FW
  transport.
- Accepts an evidence request ID as correlation metadata only; it does not authorize
  evidence capture.

## Authorization and delivery

Dispatch requires the policy revision captured when the event was queued. A
stale/revoked/expired policy therefore blocks both new delivery and retry. Events without
fields require an attribute-free rule for the exact source/feature pair.

`feature_event_sink_port` borrows the event only for the duration of the call. `ok` means
the sink copied or otherwise accepted ownership of all data needed for later delivery.
A non-`ok` return means the caller still owns retry responsibility. Remote
acknowledgement can remain ambiguous, so retry must preserve `event_id` and the downstream
transport must deduplicate it. An exception is converted to `io_error` and has the same
retry rule.

The dispatcher handles one event index per call so a batch cannot be reported atomically
after only a prefix was accepted.

## Limits and next work

- The dispatcher provides no queue, persistence, retry schedule, policy signature
  verification or FW transport.
- Remote acknowledgement can remain ambiguous; the downstream transport must deduplicate
  by `event_id`.
- An evidence request ID does not authorize evidence capture.

## See also

- [Feature event contract](feature_event_contract.md)
- [Output gate](output_gate.md)
- [Feature fan-out](feature_fanout.md)
