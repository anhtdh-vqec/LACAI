#ifndef VQEC_VISION_AI_CONTRACTS_OUTPUT_GENERATION_HPP
#define VQEC_VISION_AI_CONTRACTS_OUTPUT_GENERATION_HPP

#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

// One serialized runtime owner across all sink rebuilds. No reset or ID reclamation.
// Watermark is trusted highest issued ID, never an SDK-local mapping counter.
class output_generation {
public:
    explicit output_generation(std::uint64_t _issued_watermark = 0) noexcept;
    output_generation(const output_generation&) = delete;
    output_generation& operator=(const output_generation&) = delete;
    output_generation(output_generation&&) = delete;
    output_generation& operator=(output_generation&&) = delete;

    // No I/O. Success consumes ID even if later binding setup fails.
    // Exhaustion preserves _generation; IDs never wrap to zero.
    [[nodiscard]] status vqec_vision_ai_core_otgen_issue(std::uint64_t& _generation);

private:
    std::uint64_t issued_watermark_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_OUTPUT_GENERATION_HPP
