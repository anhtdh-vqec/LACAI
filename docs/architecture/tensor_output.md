# QNN output extraction boundary

Local source evidence: ml-info.c serializes nested dimensions arrays and one type;
gstmlpool.c allocates one memory per tensor unless continuous mode is explicitly
selected; mlqnn.c creates its DMA pool without requesting continuous mode. The
reviewed QNN wrapper produces FLOAT32 outputs. This extractor supports only that
specific packed, one-memory-per-tensor layout. Continuous/mixed dtype outputs are
rejected, not guessed. No Qualcomm private metadata struct is copied into this repo.

Input is a borrowed completed GstSample plus a model-team output contract: ordered
names and shapes, per-result byte budget. Caps type/count/shapes and memory valid
sizes must match exactly. Names are supplied by the model contract, NOT discovered
from caps: caller must pin model artifact and output ordering. No subset/reordering.
GAP/CORRUPTED output is rejected. The mapped valid view includes its memory offset;
the extractor does not reinterpret allocation padding as tensor values.

The function copies output floats into neutral owned vectors then unmaps. This is
an explicit CPU copy, not zero-copy postprocessing. It decouples decoder ownership
from vendor buffer pools. Input/output count/rank/bytes are bounded before mapping
or copying; failure leaves destination unchanged. C++ allocation exceptions may
escape with RAII unmapping. Output float representation must be native IEEE binary32.

The caller MUST establish device completion AND CPU visibility before extraction.
Mapping is not an acquire fence or cache-sync guarantee. The current vendor execute
is synchronous in source; actual allocator/cache behavior needs BSP verification.
No universal DMA_BUF_IOCTL_SYNC policy is invented here. A future appsink consumer
must apply that policy, extract the sample, release its sample ref, and only then
report result completion to submission_window. Owned vectors need a separate bounded
output queue; this per-call budget does not cap caller retention across calls.

Sample PTS is preserved as pipeline PTS, not UTC or original camera timestamp.
Job/source correlation remains in the submission ticket and must be matched by
the future graph runtime. No detection/NMS, feature events or license checks occur here.

Current slice has no appsink polling or PLAYING activation. Synthetic system-memory
tests verify caps/layout/copy logic only, not QNN accuracy or DMA cache coherency.
