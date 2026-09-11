#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

#include <limits>

namespace vqec::vision::ai {

status vqec_vision_ai_core_tnctr_validate_outputs(
    const std::vector<float_tensor_spec>& _outputs, std::uint64_t _max_output_bytes,
    std::uint64_t& _required_bytes) {
    if (_outputs.empty() || _outputs.size() > tensor_contract_limits::g_max_outputs ||
        _max_output_bytes == 0 ||
        _max_output_bytes > tensor_contract_limits::g_max_output_bytes) {
        return {status_code::invalid_argument, "invalid output count or byte budget"};
    }
    std::uint64_t total = 0;
    for (std::size_t index = 0; index < _outputs.size(); ++index) {
        const auto& output = _outputs[index];
        if (output.name_.empty() ||
            output.name_.size() > tensor_contract_limits::g_max_name_bytes ||
            output.name_.find('\0') != std::string::npos || output.dimensions_.empty() ||
            output.dimensions_.size() > tensor_contract_limits::g_max_rank) {
            return {status_code::invalid_argument, "invalid output name or rank"};
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (_outputs[previous].name_ == output.name_) {
                return {status_code::invalid_argument, "duplicate output contract name"};
            }
        }
        std::uint64_t bytes = 4;  // Contract FLOAT32 storage, independent of host sizeof(float).
        for (const auto dimension : output.dimensions_) {
            if (dimension == 0 ||
                dimension > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
                bytes > _max_output_bytes / dimension) {
                return {status_code::invalid_argument, "invalid output shape or excessive size"};
            }
            bytes *= dimension;
        }
        if (bytes > _max_output_bytes - total) {
            return {status_code::resource_exhausted, "output contract exceeds byte budget"};
        }
        total += bytes;
    }
    _required_bytes = total;
    return {};
}

}  // namespace vqec::vision::ai
