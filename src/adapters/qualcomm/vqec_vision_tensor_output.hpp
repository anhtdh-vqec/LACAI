#ifndef VQEC_VISION_AI_QUALCOMM_TENSOR_OUTPUT_HPP
#define VQEC_VISION_AI_QUALCOMM_TENSOR_OUTPUT_HPP

#include <cstdint>
#include <vector>

#include <gst/gst.h>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp"

namespace vqec::vision::ai {

// Private adapter API. Borrowed sample, called AFTER device completion and CPU visibility.
// Ordered expected outputs come from the pinned model manifest, not user caps guesses.
// 1..16 outputs, rank 1..8; max_bytes 1..64 MiB. Transactional destination.
// Performs CPU copies. No ownership transfer, SDK sync or job completion notification.
[[nodiscard]] status vqec_vision_ai_qcom_tnout_copy_sample(
    GstSample* _sample, const std::vector<float_tensor_spec>& _expected,
    std::uint64_t _max_bytes, tensor_result& _result);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QUALCOMM_TENSOR_OUTPUT_HPP
