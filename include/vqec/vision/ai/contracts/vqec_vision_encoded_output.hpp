#ifndef VQEC_VISION_AI_CONTRACTS_ENCODED_OUTPUT_HPP
#define VQEC_VISION_AI_CONTRACTS_ENCODED_OUTPUT_HPP

#include <memory>
#include <cstdint>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_preview_contract.hpp"

namespace vqec::vision::ai {

class owned_h264_output {
public:
    owned_h264_output(const owned_h264_output&) = delete;
    owned_h264_output& operator=(const owned_h264_output&) = delete;
    // Borrow valid only while this owner is retained. No pointers are transferred.
    [[nodiscard]] h264_access_unit_view vqec_vision_ai_core_encot_borrow_view() const noexcept;
    // Synchronous copy; source bytes live/immutable until return. Failure preserves _output.
    [[nodiscard]] static status vqec_vision_ai_core_encot_copy_output(
        const h264_access_unit_view& _input, const preview_frame_key& _expected_frame,
        const preview_geometry& _expected_geometry,
        std::shared_ptr<const owned_h264_output>& _output);

private:
    owned_h264_output() = default;
    preview_frame_key frame_;
    preview_geometry geometry_;
    bool is_keyframe_{false};
    std::vector<std::uint8_t> payload_;
    std::vector<std::uint8_t> sps_;
    std::vector<std::uint8_t> pps_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_ENCODED_OUTPUT_HPP
