# Feature event dispatch

`feature_event_dispatch` is the portable final authorization boundary for one feature
event. It validates the complete event batch against its activation configuration, then
builds the authorization request from the event's actual field schema IDs. A caller
cannot provide a smaller attribute list to hide sensitive payload fields.

Dispatch requires the policy revision captured when the event was queued and calls
`output_gate` immediately before the synchronous sink operation. A stale/revoked/expired
policy therefore blocks both new delivery and retry. Events without fields require an
attribute-free rule for the exact source/feature pair.

`feature_event_sink_port` borrows the event only for the duration of the call. `ok` means
the sink copied or otherwise accepted ownership of all data needed for later delivery.
A non-`ok` return means the caller still owns retry responsibility. Remote acknowledgement
can remain ambiguous, so retry must preserve `event_id` and the downstream transport must
deduplicate it. An exception is converted to `io_error` and has the same retry rule.

The dispatcher handles one event index per call so a batch cannot be reported atomically
after only a prefix was accepted. It provides no queue, persistence, retry schedule,
policy signature verification or FW transport. An evidence request ID is correlation
metadata only and does not authorize evidence capture.
