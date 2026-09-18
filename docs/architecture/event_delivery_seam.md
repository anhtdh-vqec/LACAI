# Event delivery seam

`event_delivery_seam` establishes a bounded outbox and handoff boundary between the
runtime execution pipeline and downstream firmware/system event transport. It ensures that
production pipelines do not bind device-free reference sinks, and explicitly distinguishes
in-memory handoff acceptance from actual durable delivery.

Status: normative — source delivered and contract tested.

## Responsibility

- Implement `feature_event_sink_port` as a neutral, non-blocking event handoff seam.
- Maintain a bounded queue of accepted events to prevent uncontrolled memory growth.
- Clearly differentiate between `events_accepted_`, `events_pending_`,
  `events_handed_off_`, `events_dropped_`, `events_discarded_`, and `events_rejected_`.
- Reject new event submissions fail-closed with `unavailable` when stopping.
- Reject new event submissions with `resource_exhausted` when the bounded outbox capacity
  is reached, without blocking inference threads.
- Transfer one queued event at a time to a downstream transport owner without claiming a
  durable or remote acknowledgement.
- Record an explicit shutdown discard separately from handoff.

## Lifecycle and states

```text
[Pipeline] -> deliver_event() -> [event_delivery_seam] -> take_next() -> [Transport owner]
                 |                       |
                 v                       v
          events_accepted_        events_handed_off_
          events_pending_         (not durable delivery)
```

1. **Active**: Event submissions within configured capacity are copied into the bounded
   queue and marked as `accepted_pending`. The method returns `status_code::ok`.
2. **Queue Saturation**: When pending event count reaches `max_queued_events_`, further
   submissions fail immediately with `resource_exhausted` and increment `events_dropped_`.
3. **Shutdown**: Upon `request_stop()`, new event submissions are immediately rejected.
   Pending events remain until a transport takes them or the owner explicitly calls
   `discard_pending()`. Discard never increments handoff or delivery counters.

## Limits and next work

- Plan 0 delivers the in-memory bounded handoff seam and metrics separation.
- Full durable UDS IPC transport, persistent outbox journaling, and firmware evidence
  receipt correlation are delivered in Plan 3.

## See also

- [Feature event dispatch](feature_event_dispatch.md)
- [Feature event contract](feature_event_contract.md)
- [Production composition foundation review](../development/production_composition_foundation_review.md)
