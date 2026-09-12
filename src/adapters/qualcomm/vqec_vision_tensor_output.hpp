#ifndef VQEC_VISION_AI_QUALCOMM_TENSOR_OUTPUT_HPP
#define VQEC_VISION_AI_QUALCOMM_TENSOR_OUTPUT_HPP

#include <cstdint>
#include <vector>

#include <gst/gst.h>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp"

namespace vqec::vision::ai {

// Maps between the neutral element type and the reviewed GstML caps type spelling
// ("INT8".."FLOAT32"). Unknown/unsupported types map to unknown / nullptr.
[[nodiscard]] tensor_element_type
vqec_vision_ai_qcom_tnout_element_type_from_ml(const char* _name) noexcept;
[[nodiscard]] const char*
vqec_vision_ai_qcom_tnout_ml_type_name(tensor_element_type _type) noexcept;

// Private adapter API. Borrowed sample, called AFTER device completion and CPU visibility.
// Ordered expected outputs come from the pinned model manifest, not user caps guesses.
// 1..16 outputs, rank 1..8; max_bytes 1..64 MiB. Transactional destination.
// Supports every element type the caps can carry, provided all tensors share the single
// caps type; genuine per-tensor mixed dtype is rejected (the reviewed caps cannot express
// it). Bytes are copied verbatim; the model contract supplies dtype/quantization identity.
// No ownership transfer, SDK sync or job completion notification.
[[nodiscard]] status vqec_vision_ai_qcom_tnout_copy_sample(
    GstSample* _sample, const std::vector<tensor_spec>& _expected,
    std::uint64_t _max_bytes, tensor_result& _result);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QUALCOMM_TENSOR_OUTPUT_HPP
