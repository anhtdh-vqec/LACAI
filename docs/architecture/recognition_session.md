# Recognition session owner

`recognition_session` is the serialized owner between cascade embeddings and an
output-authorized FR consumer. It accepts the neutral `embedding_index_port`, so the
same policy runs with the exact reference index or the Qualcomm deployment's Zvec
adapter. The owner never includes Zvec, QNN, GStreamer or DBus types.

The gallery stores one metadata entry per template. A subject may therefore have
multiple samples without duplicating subject identity in the output. Each mutation
uses the index revision as a compare-and-swap value; a stale caller is rejected. The
session keeps bounded metadata for record-to-subject ownership, assigns monotonic
record IDs, and removes every template for a subject in a serialized sequence. If a
backend mutation succeeds but the owner cannot update its metadata, the session faults
instead of pretending that the gallery is consistent.

For a production gallery, `configure_persistent` loads one complete snapshot from the
AI-owned protected-store port, validates model/version/preprocess identity and rebuilds a fresh
derived index at the exact durable revision before accepting searches. Enrollment and
subject removal first perform a durable snapshot CAS, then update the index. An index
failure after durable commit faults the session; restart recovery replays the authoritative
snapshot rather than trusting a partially updated collection. The existing non-persistent
configure path remains a bounded reference/harness mode.

Recognition searches are pinned to one gallery revision and the configured model
identity. The policy groups the returned templates by opaque subject reference and
emits `known`, `unknown` or `ambiguous`; backend failures remain errors. Label
application requires the exact frame identity and gallery revision, then matches the
embedding track ID to the tracked observation. It writes only the opaque subject
reference into `overlay_box::label_`; the service authorizes the configured identity
attribute through `output_gate` before this label reaches the production renderer.
Resolving a human display name remains an output metadata responsibility.

This owner is the runtime seam for DBus enrollment. A control adapter should call
`add_template` only after the live cascade has supplied an accepted embedding for the
requested source/track, and must keep request identity, peer authorization, quality
policy and durable gallery storage outside this class. The current implementation is
an in-process owner. The optional GIO DBus adapter maps the FW control contract and
authenticates the configured peer. The AI-owned AES-256-GCM POSIX adapter implements
bounded authenticated persistence, private file ownership/modes, revision CAS and atomic
rename. Its filesystem key provider is not hardware-bound; hardware key qualification and
display-name metadata remain tracked work in the face-recognition completion plan.
