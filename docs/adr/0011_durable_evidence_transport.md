# ADR 0011 — Durable evidence transport

Status: proposed
Date: 2026-09-19
Owner: AI APP lead; BSP+FW review required for released receiver acceptance

## Context

Fire/smoke alarms need evidence even when the backend, FW evidence service or UI starts late,
restarts, or is temporarily unavailable. D-Bus control calls and the existing bounded RAM event
seam do not provide durable retry or request deduplication. Qualcomm graph and HTP startup must
remain gated by real frame arrival and must not be coupled to evidence-service availability.

## Decision

LACAI version 1 uses a durable AI-owned SQLite outbox and a worker behind a neutral evidence
transport port. The production adapter uses Unix-domain `SOCK_SEQPACKET`; each command and receipt
uses a bounded, explicitly encoded LACAI version-1 envelope. The request ID and exact serialized
payload form the deduplication identity. Reusing an ID with different bytes is a conflict.

The final output authorization boundary copies its policy revision into the event. Enqueue occurs
only after authorization and retry revalidates the same payload fields and policy revision. A local
commit, FW acceptance, recording, and ready/partial/failed receipt are distinct states. Missing FW
keeps work pending with bounded backoff and never blocks service startup or causes model loading.

AI-owned reference receiver tests framing, peer credentials, duplicate/lost acknowledgement and
restart behavior. Only the released FW receiver can supply production evidence acceptance.

## Alternatives

- D-Bus event delivery was rejected because the lifecycle API has no durable request/receipt
  semantics and is not the high-rate data plane.
- A RAM queue was rejected because process failure loses accepted alarms.
- Shared memory was deferred until measurement shows that bounded UDS messages exceed budget;
  alarm durability would still require an outbox.

## Consequences

- Alarm enqueue performs a small durable database transaction; work after commit is off the frame
  path. Queue and retry limits are deployment configuration.
- The runtime can start before FW/backend and can continue while either is absent.
- ABI changes require a migration ADR; all LACAI version fields remain at baseline version 1.

## Approve after (gates)

1. AI-side malformed, duplicate, lost-ACK, restart, revocation and capacity tests pass.
2. QCS6490 no-receiver and late-receiver tests preserve inference FPS/CPU and outbox recovery.
3. BSP+FW signs the receiver contract before production evidence is marked accepted.
