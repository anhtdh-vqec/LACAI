# ADR 0001 — architecture baseline

Status: proposed baseline for team adoption; external contracts pending sign-off.
Date: 2026-09-06. Owner: AI APP lead.

## Context

Greenfield 3–4 person team, 12-week release, 13 minimum features, future traffic,
independent IPK packages and multiple vendor platforms.

## Decisions

C++17; snake_case and mandated function prefixes; direct vendor SDK adapters;
FastCV explicit Qualcomm image path + QNN inference; portable model decoders;
no OpenCV production dependency; neutral contracts; one service process at v1;
bounded queues and completion-owned buffers; shared compatible perception;
entitlement at scheduling and output boundaries; controlled restart updates.

## Alternatives

Full GStreamer plugin application: useful reference/benchmark but vendor types
must not become core contracts. Per-feature process: isolation benefits but
duplicated perception/resource costs; defer until justified by measured need.
Pure C: possible ABI boundary, not selected for internal RAII/concurrency code.
Universal abstraction exposing all SDK APIs: reject; capability-driven small
ports preserve portability without pretending every vendor is identical.

## Consequences

Long method names are deliberate; C++ special members and interface overrides
need explicit exceptions. SDK-specific performance paths require board gates.
13 feature integration remains dependent on qualified model delivery.
Second vendor and traffic are extension seams, not completed support.
