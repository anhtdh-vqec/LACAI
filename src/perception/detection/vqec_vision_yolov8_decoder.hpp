#ifndef VQEC_VISION_AI_DETEC_YOLOV8_DECODER_HPP
#define VQEC_VISION_AI_DETEC_YOLOV8_DECODER_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_inference_plan.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_model_decoder.hpp"

namespace vqec::vision::ai {

// Backend-independent YOLOv8 detection decoder. It consumes the export's flat, channel-first
// head: a box tensor [1,4,A] holding xywh (centre) and a score tensor [1,C,A] holding sigmoid
// probabilities, both in the letterboxed tensor pixel space. It dequantizes with the tensor
// spec, applies inverse letterbox to the source frame, clips, and runs greedy per-class NMS.
// No vendor type and no model name are involved.
struct yolov8_decoder_config {
    std::uint32_t source_width_{0};
    std::uint32_t source_height_{0};
    std::uint32_t tensor_width_{0};
    std::uint32_t tensor_height_{0};
    image_placement placement_{image_placement::centre};
    std::string box_tensor_{"boxes_out"};
    std::string score_tensor_{"conf_out"};
    std::size_t class_count_{1};
    std::vector<std::string> class_names_;
    float confidence_threshold_{0.25F};
    float iou_threshold_{0.45F};
    // Only xywh in tensor pixels is supported by this head.
    coordinate_convention box_convention_{coordinate_convention::tensor_pixels_xywh};
};

class yolov8_decoder final : public model_decoder_port {
public:
    yolov8_decoder() = default;
    explicit yolov8_decoder(yolov8_decoder_config _config);

    [[nodiscard]] status vqec_vision_ai_cntr_mddec_validate(
        const model_outputs& _outputs) const override;
    [[nodiscard]] status vqec_vision_ai_cntr_mddec_decode(
        const tensor_result& _result, const preview_frame_key& _expected_frame,
        observation_batch& _observations) override;

private:
    yolov8_decoder_config config_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_DETEC_YOLOV8_DECODER_HPP
