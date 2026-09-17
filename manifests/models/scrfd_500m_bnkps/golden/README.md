# Golden references (pending)

M1/M0 requires permitted, privacy-safe golden data: an input NV12 frame or its expected
uint16 input tensor, the raw `score_*`/`bbox_*`/`kps_*` outputs, and the expected decoded
boxes/landmarks with tolerance. None is present yet. Do not commit model binaries,
biometric samples or faces that are not explicitly cleared for use.
- **Status:** planned — no golden fixtures present

