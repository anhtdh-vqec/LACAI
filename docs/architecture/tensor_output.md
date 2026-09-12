# QNN tensor input/output boundary

Neutral element types follow the reviewed GstML/QNN set: INT8, UINT8, INT16, UINT16,
INT32, UINT32, INT64, UINT64, FLOAT16, FLOAT32. `tensor_spec` carries name, row-major
shape, dtype and an explicit affine quantization (`real = (stored - zero_point) * scale`)
for integer tensors. `tensor_blob` owns packed little-endian bytes; consumers never assume
float or reinterpret across dtype.

## Input caps

`make_tensor_caps` maps the model plan's input element type to the matching
`neural-network/tensors` caps `type` string, so the converter/inference path can negotiate
any reviewed input dtype the model declares. Only FLOAT32 input accepts explicit mean/sigma
coefficients; integer/fp16 input requires identity normalization on this path. The plugin
reads the input type from the model's first input tensor, so multiple inputs with different
element types are not representable through the current caps and must be rejected.

## Output extraction

`vqec_vision_ai_qcom_tnout_copy_sample` maps the caps `type` to a neutral element type and
supports every reviewed caps dtype. The model contract supplies ordered names and shapes;
the extractor verifies caps type/count/shape and one-memory-per-tensor layout, computes
packed bytes from the element size, checks each memory's valid size, and copies bytes
verbatim into owned blobs. GAP/CORRUPTED output is rejected. A model contract whose dtype
differs from the negotiated caps type is rejected as unsupported, never silently cast.

The reviewed caps carry a single element type for the whole tensor frame, so genuine
per-tensor mixed dtype cannot be expressed and is rejected. The installed Qualcomm plugin
`ml-qnn-engine.cc` hardcodes `outinfo->type = GST_ML_TYPE_FLOAT32` and converts native
outputs to float, so on the plugin path output contracts must be FLOAT32; a native int/
fp16 output requires the direct-SDK backend and its own golden evidence. This adapter does
not claim native output dtype where the plugin does not provide it.

Sample PTS is preserved as pipeline PTS, not UTC or the original camera timestamp. Job/
source correlation remains in the submission ticket. No detection/NMS, feature events or
license checks occur here.

## Safety

Input is a borrowed completed GstSample; the caller must establish device completion AND
CPU visibility before extraction (mapping is not an acquire fence). Copies are explicit
CPU copies, not zero-copy. Count/rank/bytes are bounded before mapping or copying; failure
leaves the destination unchanged. C++ allocation failures are reported, and RAII unmaps on
every path. Owned blobs need a separate bounded output queue; this per-call budget does not
cap caller retention across calls. Synthetic system-memory tests verify caps/layout/copy
logic only, not QNN accuracy, DMA cache coherency or native multi-dtype support.
