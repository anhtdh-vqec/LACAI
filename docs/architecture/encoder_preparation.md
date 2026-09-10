# Encoder input preparation composition

Source-only portable app helper; not hardware submission, a renderer or a ring writer.
It composes encoder_window admission with preview_surface_pool acquisition.

drain_backend performs one nonblocking shutdown step: stop ledger admission, call backend
drain once, and return ok only when the backend reports quiescence AND the ledger has no
outstanding reservations. Backend errors/pending propagate without cleanup. Backend ok
with outstanding entries remains pending: process retained events and explicitly cancel
unsubmitted preparations rather than fabricate completion. Exceptions leave admission
closed. No loop, sleep, reset or destructor cleanup is hidden in this helper. An ok result
does not release caller-owned CPU surfaces or prove an unrelated ring writer has stopped.
Source tests cover an empty backend with a live preparation, explicit cancellation,
repeated drain and admission remaining closed. These portable cases pass on QCS6490.

The `encoder_preparation` facade now keeps its window, ticket and writer together.
Use prepare -> synchronous pixel population through borrow_data -> cancel OR
commit_input. It is non-copyable/non-movable, exposes no replaceable writer, and
cancel takes no external token. commit_input requires an empty shared owner destination;
it commits the ledger before no-throw sealing/owner transfer and publishes the ticket
only on success. After handoff, cancellation through the facade is rejected.
Do not mutate its borrowed window externally while preparation is active.

This facade is pre-submission only: mutable pointer borrows must end before either
terminal operation and may not be retained by asynchronous hardware. Destruction
releases only private CPU storage, not ledger entries; explicit cancellation is required.
After handoff the backend job must retain the sealed owner until actual input completion.
Failed push, missing output, shutdown and hardware quarantine remain runtime duties.
The low-level helpers below remain available; prefer the bound facade for new callers.

submit_backend now composes ledger begin_input with exactly one call to the neutral
encoder port. Guard failures never invoke the backend or synthesize completion. A
non-ok backend return, under its strict no-access/no-retention contract, reconciles
the committed ticket as input complete and terminal no-output. The original rejection
status is returned unless ledger reconciliation fails. It never drops the caller's
pixel owner. An accepted submit remains outstanding for independently polled events.
Exceptions stop admission and propagate without assuming rejection or completion;
caller and backend owners must remain retained for explicit recovery, with no retry.
Fake-backend test source covers acceptance, repeated attempt and pre-access rejection;
the fake-backend binary passes on QCS6490, while concrete encoder conformance remains pending.

Fault-injection test source also simulates a port violation: throw after retaining input.
It checks exception propagation, unchanged outstanding byte accounting, no repeated backend
call, and pool retention after the caller drops its owner. Only explicit simulated
quiescence releases the fake backend owner; this is not proof of device recovery.

For backend use, prepare_backend captures a nonzero output binding generation together
with the admitted frame. commit_backend emits the full encoder_input envelope and sealed
owner after successful ledger commit. It takes no replacement generation/frame arguments:
failed re-prepare cannot retag an occupied preparation. A plain prepare does not authorize
this envelope handoff (it still supports the older explicit ticket/pixels handoff).
Destination pixels must be empty; failures preserve the entire destination. Successful
handoff does not submit to a device or prove that rendered pixels match declared metadata.

Drain or a latched deadline fault between prepare and commit rejects handoff without
sealing, publishing a ticket or transferring the owner. The writer and reservation
remain together so explicit cancel can reclaim never-submitted work. This is distinct
from committed jobs, whose resources must survive timeout until actual completion.
The contract binary covers both transitions and passes on QCS6490; this remains synthetic
logic evidence rather than an executed hardware-encoder test.

prepare_input requires an empty writer and configured pool/ledger with exactly equal
width AND height (matching byte size is insufficient). It reserves the encoder ticket
before acquiring a surface. No demand or pre-reservation error leaves output ticket
and writer untouched. After successful reservation, the ticket is published, including
when subsequent acquisition fails; it then identifies a consumed/cancelled reservation.
This distinction makes cleanup failure diagnosable without losing the token.

On acquisition failure, cancel that unsubmitted reservation and return the pool error.
If cancellation unexpectedly fails, return that error and retain the token for recovery.
All operations must run on the same serialized owner with no concurrent/reentrant
ledger mutations; rollback should then always succeed. A cancelled attempt consumes
source PTS under the existing ledger contract; retry with a newer frame, not the same PTS.

cancel_input cancels only an uncommitted token, then releases the writer. Failure leaves
the writer intact. An empty, sealed or moved-from writer is rejected before any ledger
mutation, preserving the reservation for explicit ownership reconciliation. This guard
does not detect a different nonempty writer. Caller must pass the writer belonging to that reservation; this
helper does not infer token ownership from a raw pointer. Do not call after submitting
the image to hardware. Mutable borrows must end before cancellation or sealing.

Successful preparation grants neither permission to render sensitive attributes nor
hardware completion. Next steps: populate full NV12, validate authorized overlay,
seal, commit before push, retain the sealed input in the encoder job, and report input
and result completion separately. These steps are NOT automatically performed here.
Pool memory can outlive ledger completion if readers remain; preparation then fails
with resource_exhausted and rolls its reservation back instead of allocating a fallback.
