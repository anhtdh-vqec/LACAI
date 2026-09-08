#include "vqec/vision/ai/contracts/vqec_vision_output_generation.hpp"

#include <limits>

namespace vqec::vision::ai {

output_generation::output_generation(std::uint64_t _issued_watermark) noexcept
    : issued_watermark_(_issued_watermark) {}

status output_generation::vqec_vision_ai_core_otgen_issue(std::uint64_t& _generation) {
    if (issued_watermark_ == std::numeric_limits<std::uint64_t>::max()) {
        return {status_code::resource_exhausted, "output binding identity space exhausted"};
    }
    _generation = ++issued_watermark_;
    return {};
}

}  // namespace vqec::vision::ai
