#ifndef VQEC_VISION_AI_CONTRACTS_TENSOR_RESULT_HPP
#define VQEC_VISION_AI_CONTRACTS_TENSOR_RESULT_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace vqec::vision::ai {

struct float_tensor_spec {
    std::string name_;
    std::vector<std::uint32_t> dimensions_;
};

struct float_tensor_result {
    float_tensor_spec spec_;
    std::vector<float> values_;
};

struct tensor_result {
    std::uint64_t pipeline_pts_ns_{UINT64_MAX};
    std::vector<float_tensor_result> tensors_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_TENSOR_RESULT_HPP
