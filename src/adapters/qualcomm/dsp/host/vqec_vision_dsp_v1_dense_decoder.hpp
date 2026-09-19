#ifndef VQEC_VISION_AI_QUALCOMM_DSP_V1_DENSE_DECODER_HPP
#define VQEC_VISION_AI_QUALCOMM_DSP_V1_DENSE_DECODER_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/inference/vqec_vision_inference_plan.hpp"
#include "vqec/vision/ai/contracts/inference/vqec_vision_model_decoder.hpp"
#include "vqec_vision_dsp_v1_client.hpp"

extern "C" {
#include "vqec_vision_dsp_v1_dense.h"
}

namespace vqec::vision::ai {

// Qualcomm-private binding from the neutral model decoder port to the negotiated v1
// dense_decode operation. Shape, class count, tensor roles, quantization and inverse transform
// are immutable activation data; no model/usecase identity participates in dispatch.
struct dsp_v1_dense_decoder_config {
    std::uint32_t source_width_{0};
    std::uint32_t source_height_{0};
    std::uint32_t tensor_width_{0};
    std::uint32_t tensor_height_{0};
    image_placement placement_{image_placement::centre};
    std::string box_tensor_;
    std::string score_tensor_;
    std::uint32_t prediction_count_{0};
    std::uint32_t class_count_{0};
    std::vector<std::string> class_names_;
    float confidence_threshold_{0.0F};
    float iou_threshold_{0.0F};
    std::uint32_t candidate_capacity_{0};
    std::uint32_t output_capacity_{0};
    std::shared_ptr<dsp_v1_client> client_;
};

class dsp_v1_dense_decoder final : public model_decoder_port {
public:
    explicit dsp_v1_dense_decoder(dsp_v1_dense_decoder_config _config);
    ~dsp_v1_dense_decoder() override = default;

    dsp_v1_dense_decoder(const dsp_v1_dense_decoder&) = delete;
    dsp_v1_dense_decoder& operator=(const dsp_v1_dense_decoder&) = delete;
    dsp_v1_dense_decoder(dsp_v1_dense_decoder&&) = delete;
    dsp_v1_dense_decoder& operator=(dsp_v1_dense_decoder&&) = delete;

    [[nodiscard]] status vqec_vision_ai_cntr_mddec_validate(
        const model_outputs& _outputs) const override;
    [[nodiscard]] status vqec_vision_ai_cntr_mddec_decode(
        const tensor_result& _result, const preview_frame_key& _expected_frame,
        observation_batch& _observations) override;

private:
    dsp_v1_dense_decoder_config config_;
    std::mutex mutex_;
    std::vector<std::uint8_t> packed_input_;
    std::vector<std::uint8_t> compact_output_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QUALCOMM_DSP_V1_DENSE_DECODER_HPP
