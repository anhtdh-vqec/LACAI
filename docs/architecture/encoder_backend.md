# Encoder backend lifecycle port

Status: neutral interface source, no concrete encoder implementation or executed test.
Backend-specific construction/configuration remains private to the adapter. A ready
instance binds one profile/source/cycle and an explicit bounded resource budget.
No vendor types, driver mode numbers or GStreamer property strings enter this port.

Pure `vqec_vision_ai_core_encct_validate_input` now checks an input against independently
supplied frame, geometry, ticket and generation. Exact packed NV12 size is required;
equal byte counts with different dimensions do not match. Zero frame ID/source PTS and
pipeline PTS are valid; missing owner, invalid token and unknown source PTS are not.
Validation does not prove immutable provenance, policy authorization, ledger commit,
backend admission, color negotiation or hardware completion. Adapter must establish
those separately and invoke validation before device access. Unit source is unexecuted.

## Submission and ownership

The caller commits encoder_window before submit. Request includes original frame,
geometry, ticket, dispatch generation and sealed tightly packed CPU NV12 owner.
Backend validates source/profile/cycle, nonzero generation/token, ticket source
epoch/frame ID/PTS
matching frame PTS, image byte count and admission before touching a device. No mutable
borrow may survive sealing. This v1 CPU surface port is not a DMA-BUF import contract.

submit returns ok only when responsibility is accepted. It retains the owner before
any asynchronous read and reserves bounded completion capacity before acceptance.
Any non-ok return guarantees no retained owner, no device access and no future events
for that attempt. Thus a preflight rejection of an already committed ledger ticket
can be explicitly reconciled as input done plus terminal no-output by the caller.
No cancellation of committed ledger entries is used.

If a driver push fails AFTER ownership/access may have begun, submit MUST return ok
and report fault through polling; it cannot report a rejection. Ambiguous acceptance
belongs to backend recovery, not caller guesswork. Failure after acquisition never
permits freeing an owner without completion/quiescence evidence. No exception may
escape an accepted submission path and obscure ownership disposition.

## Events and drain

poll is nonblocking and transactional: pending means no event; failure leaves the
destination unchanged and grants no completion. Successful events are:

- input_complete: token identifies all input reads finished; no AU attached.
- output_ready: token and immutable AU, with original frame/PTS metadata restored.
- output_dropped: terminal proof no AU can still arrive for this token.
- fault: health failure, never either completion; zero token denotes whole backend.

Exactly one input completion and one terminal output event per accepted job; their
order is independent. Event delivery must not be lost on queue pressure. Backend
must retain undelivered completions/results within admitted budgets and reject new
work when capacity is exhausted. Returning output_ready transfers a shared immutable
owner, not a borrowed SDK sample. Runtime still validates correlation and authorization.
Token uniquely correlates to the admitted dispatch generation; the runtime must not
relabel an old AU with a new sink generation.

`vqec_vision_ai_core_encct_validate_event` rejects incomplete tokens, missing/unexpected
AU owners, completion events carrying error status, faults carrying ok/pending, and
unknown event kinds. Completion details use ok; fault details require an error code.
Only a fault may have the whole-backend zero token. Validation is structural, not proof
of completion or duplicate protection: runtime must still correlate and update its
ledger exactly once. Tests are source-only and do not establish vendor conformance.

begin_drain stops acceptance and starts backend EOS/drain. pending means drain in
progress; ok requires no device readers, pending results or undelivered events.
Repeat calls are allowed. poll continues during drain. Timeout/fault does not imply
completion. No automatic resume/reconfigure/retry on this object.

All calls are serialized and non-reentrant; vendor callbacks enqueue internally and
never call the runtime inline. Destruction requires proven drain/quiescence. A process
supervisor must retain a faulted backend and its resources until BSP recovery proves
quiescence; the interface does not invent a generic hardware cancellation mechanism.

Implementation tests must cover rejected admission, failure after possible acceptance,
reordered completions, duplicated/stale tokens, full queues, result ownership, EOS,
timeout and shutdown. The interface alone proves none of these backend properties.
