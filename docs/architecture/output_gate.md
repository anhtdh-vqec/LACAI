# Revision-aware output authorization gate

Implementation candidate; security/lead and FW review pending. Pure C++ internal
policy evaluator, NOT signature verification or a complete entitlement runtime.

Trusted FW policy integration supplies a validated grant projected into source/feature
rules and process-local monotonic validity interval [not_before, expires). Each rule
allows one exact source_id/feature_id pair and an explicit set of attribute IDs.
No wildcard, inheritance, default camera, implicit FR or traffic permission. Empty
rule list denies all. A rule with no attributes permits only attribute-free output.

Limits: 64 rules, 64 attributes per rule/request, identifier length 1..128 ASCII
letters/digits/underscore/hyphen/dot/colon. Duplicate pairs and duplicate attributes
are rejected. Granular scopes such as human.face_embedding, human.estimated_age,
human.clothing and traffic.plate_text must be explicitly provisioned by policy owner;
this component does not define the ontology or inspect result payload contents.

apply_policy(policy, expected_revision) validates then atomically replaces policy.
Revision must be greater than current and expected_revision must match current (CAS).
Revision zero is unset. Failed update leaves old policy unchanged; caller must not
interpret malformed update rejection as successful revocation. Revoke explicitly with
a higher-revision empty policy, or call invalidate to deny immediately without clearing
the revision watermark. After invalidate, only a newer valid policy reopens access.

authorize(request, steady_now) checks active policy, its time interval, exact queued
policy revision and every requested attribute against one rule (no union across rules).
Backward monotonic time invalidates the policy until a newer policy is applied; equal
times are allowed. Time continues to advance on denied calls. Across reboot, monotonic
validity and revision anti-replay must be reconstructed by trusted FW integration;
this in-memory gate does not provide durable anti-rollback or remote offline revocation.

Caller must populate scopes from the ACTUAL typed payload, not a caller-provided subset.
Call immediately before synchronous delivery on the serialized control/output executor;
policy apply/invalidate must not interleave between check and delivery. A queued retry
retains its original revision; never relabel it to bypass a revocation. On mismatch,
drop or recompute under current policy according to the eventual router contract.
This strict policy conservatively drops old results even when a new grant is broader.

No compute admission, installed/desired state, graph cancellation, delivery queue,
payload redaction, transport or feature implementation is supplied. Not yet wired to
camera tensor output (raw tensors do not provide trustworthy attribute scopes).
It cannot authorize biometric/raw-tensor export simply because inference is permitted.
Tests cover CAS/revoke/expiry/rollback and human/traffic attribute isolation; no build yet.
