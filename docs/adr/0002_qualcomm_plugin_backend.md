# ADR 0002 — Qualcomm plugin-backed adapter

Date: 2026-09-06. Status: implementation direction based on latest user instruction.
Owner/reviewer: AI APP lead (user); production validation remains pending.

## Context

User confirms Qualcomm plugins run and are optimized on QSC6490, Qualcomm Linux 1.8.
User requests source implementation now, without building or requiring a sysroot.

## Decision

Prefer wrapping installed qtimlvconverter + qtimlqnn behind a private GStreamer
adapter rather than initially duplicating their FastCV/QNN SDK loaders.
Neutral contracts stay free of Gst/vendor types. Converter engine is explicitly fcv.
Do not copy vendor implementation source. No OpenCV algorithm/path is selected;
audit optional transitive dependencies of the installed plugin binary at packaging.

ADR 0001 direct-SDK preference and blueprint section 8 restriction on starting
code without SDK are superseded for this plugin path. Direct SDK remains optional
for model capabilities the plugin cannot provide; it is not implemented now.

## Initial executable-source scope

Implement typed single-image plan validation and transactional NULL-state graph
construction: appsrc -> qtimlvconverter -> tensor capsfilter -> qtimlqnn -> appsink.
Probe actual factories, property types and enum nicks. Preserve prior graph when
new graph construction fails. Never transition to READY/PLAYING in this slice.

No frame submission, camera ACK, hardware cancellation, output translation,
entitlement runtime or application loop yet. Keeping this first graph slice in
NULL makes the incomplete buffer/drain contract explicit rather than unsafe.
The next slice adds a completion-owned frame bridge, bounded admission, bus and
source lifecycle, negotiated tensor validation and decoder integration.

## Limitations

Single image, batch1, NHWC RGB/BGR, linear NV12 input. Explicit model preprocessing
values are plugin coefficients, not automatic conversion from arbitrary mean/std.
Wrapper float32 output and first-graph behavior are acknowledged, not relabeled
as native multi-dtype/multi-graph support. No output subset/reordering in this slice.
Successful NULL linking is not successful model loading or inference.
No build/test execution requested; source checks only in this turn.
