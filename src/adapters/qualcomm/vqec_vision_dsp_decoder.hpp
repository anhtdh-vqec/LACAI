#ifndef VQEC_VISION_AI_QUALCOMM_DSP_DECODER_HPP
#define VQEC_VISION_AI_QUALCOMM_DSP_DECODER_HPP

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_inference_plan.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_model_decoder.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_observation.hpp"
#include "vqec_vision_dsp_session.hpp"

namespace vqec::vision::ai {

enum class dsp_decoder_kind {
    yolov8,
    scrfd
};

struct dsp_decoder_config {
    dsp_decoder_kind kind_{dsp_decoder_kind::yolov8};
    std::uint32_t source_width_{0};
    std::uint32_t source_height_{0};
    std::uint32_t tensor_width_{640};
    std::uint32_t tensor_height_{640};
    image_placement placement_{image_placement::centre};
    float confidence_threshold_{0.25F};
    float iou_threshold_{0.45F};
    std::string class_id_{"person"};
    std::string box_tensor_{"boxes_out"};
    std::string score_tensor_{"conf_out"};
    std::string landmark_schema_id_{"scrfd.5point"};
    std::string landmark_schema_version_{"1"};
    std::size_t landmark_count_{5};
    std::shared_ptr<dsp_session> session_;
};

class dsp_decoder final : public model_decoder_port {
public:
    dsp_decoder() = default;
    explicit dsp_decoder(dsp_decoder_config _config);
    ~dsp_decoder() override = default;

    dsp_decoder(const dsp_decoder&) = delete;
    dsp_decoder& operator=(const dsp_decoder&) = delete;
    dsp_decoder(dsp_decoder&&) noexcept = delete;
    dsp_decoder& operator=(dsp_decoder&&) noexcept = delete;

    [[nodiscard]] status vqec_vision_ai_cntr_mddec_validate(
        const model_outputs& _outputs) const override;

    [[nodiscard]] status vqec_vision_ai_cntr_mddec_decode(
        const tensor_result& _result, const preview_frame_key& _expected_frame,
        observation_batch& _observations) override;

    [[nodiscard]] const dsp_decoder_config& vqec_vision_ai_qcom_dspdc_config() const noexcept;

private:
    dsp_decoder_config config_;
    std::shared_ptr<dsp_session> owned_session_;
    std::mutex decode_mutex_;
    dsp_post_result post_workspace_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QUALCOMM_DSP_DECODER_HPP
