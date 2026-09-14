#ifndef VQEC_VISION_AI_DETEC_DENSE_DECODER_HPP
#define VQEC_VISION_AI_DETEC_DENSE_DECODER_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_inference_plan.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_model_decoder.hpp"

namespace vqec::vision::ai {

namespace dense_decoder_limits {
inline constexpr std::size_t g_max_stages = 4;
inline constexpr std::size_t g_max_class_count = 1024;
inline constexpr std::size_t g_max_anchors = 200000;
}  // namespace dense_decoder_limits

// One anchor-free prediction stage: a box tensor (cx, cy, width, height already in tensor
// pixels) and a score tensor (per-class probability) over grid_height x grid_width cells.
struct dense_decoder_stage {
    std::string box_tensor_;
    std::string score_tensor_;
    std::uint32_t grid_width_{0};
    std::uint32_t grid_height_{0};
    // Kept for provenance/completeness; the grid already fixes the cell count.
    std::uint32_t stride_{0};
};

struct dense_decoder_config {
    std::uint32_t source_width_{0};
    std::uint32_t source_height_{0};
    std::uint32_t tensor_width_{0};
    std::uint32_t tensor_height_{0};
    image_placement placement_{image_placement::centre};
    std::size_t class_count_{0};
    std::vector<std::string> class_names_;
    std::vector<dense_decoder_stage> stages_;
    float confidence_threshold_{0.25F};
    float iou_threshold_{0.45F};
};

// Backend-independent dense (anchor-free) detector decoder. It consumes typed float32
// tensors by name, reverses the configured letterbox/stretch transform to source pixels,
// clips to image bounds and applies greedy per-class NMS. The tensor layout comes from the
// model package, never from a model name; quantization is the engine's concern.
class dense_decoder final : public model_decoder_port {
public:
    dense_decoder() = default;
    explicit dense_decoder(dense_decoder_config _config);

    [[nodiscard]] status vqec_vision_ai_cntr_mddec_validate(
        const model_outputs& _outputs) const override;
    [[nodiscard]] status vqec_vision_ai_cntr_mddec_decode(
        const tensor_result& _result, const preview_frame_key& _expected_frame,
        observation_batch& _observations) override;

private:
    dense_decoder_config config_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_DETEC_DENSE_DECODER_HPP
