#ifndef VQEC_VISION_AI_DETEC_ANCHOR_DISTANCE_DECODER_HPP
#define VQEC_VISION_AI_DETEC_ANCHOR_DISTANCE_DECODER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_inference_plan.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_model_decoder.hpp"

namespace vqec::vision::ai {

namespace anchor_distance_decoder_limits {
inline constexpr std::size_t g_max_stages = 8;
inline constexpr std::size_t g_max_landmarks = 16;
inline constexpr std::size_t g_max_candidates = 4096;
inline constexpr std::uint32_t g_max_stride = 1024;
}  // namespace anchor_distance_decoder_limits

struct anchor_distance_stage_config {
    std::string score_tensor_;
    std::string box_tensor_;
    std::string landmark_tensor_;
    std::uint32_t stride_{0};
    std::uint32_t grid_width_{0};
    std::uint32_t grid_height_{0};
    std::uint32_t anchors_per_cell_{0};
};

// Configured anchor-centre distance decoder. Tensor layouts are [1,N,1], [1,N,4]
// (left/top/right/bottom) and [1,N,2*K] (x/y offsets), all in stride units.
struct anchor_distance_decoder_config {
    std::uint32_t source_width_{0};
    std::uint32_t source_height_{0};
    std::uint32_t tensor_width_{0};
    std::uint32_t tensor_height_{0};
    image_placement placement_{image_placement::centre};
    std::string class_id_;
    std::string landmark_schema_id_;
    std::string landmark_schema_version_;
    std::size_t landmark_count_{0};
    float anchor_offset_cells_{0.0F};
    float confidence_threshold_{0.0F};
    float iou_threshold_{0.0F};
    std::size_t max_candidates_{0};
    std::vector<anchor_distance_stage_config> stages_;
};

class anchor_distance_decoder final : public model_decoder_port {
public:
    explicit anchor_distance_decoder(anchor_distance_decoder_config _config);

    [[nodiscard]] status vqec_vision_ai_cntr_mddec_validate(
        const model_outputs& _outputs) const override;
    [[nodiscard]] status vqec_vision_ai_cntr_mddec_decode(
        const tensor_result& _result, const preview_frame_key& _expected_frame,
        observation_batch& _observations) override;

private:
    struct candidate {
        float score_{0.0F};
        float x_{0.0F};
        float y_{0.0F};
        float width_{0.0F};
        float height_{0.0F};
        std::size_t order_{0};
        std::array<landmark_point, anchor_distance_decoder_limits::g_max_landmarks> landmarks_{};
    };
    anchor_distance_decoder_config config_;
    std::vector<candidate> candidates_;
    std::array<bool, anchor_distance_decoder_limits::g_max_candidates> suppressed_{};

};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_DETEC_ANCHOR_DISTANCE_DECODER_HPP
