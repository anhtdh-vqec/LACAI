#ifndef VQEC_VISION_AI_APPL_PERCEPTION_RESULT_STAGE_HPP
#define VQEC_VISION_AI_APPL_PERCEPTION_RESULT_STAGE_HPP

#include <cstdint>

#include "vqec_vision_model_decode_stage.hpp"
#include "vqec_vision_tracking_stage.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_submission_window.hpp"

namespace vqec::vision::ai {

struct perception_result_config {
    std::uint32_t camera_id_{0};
    std::uint32_t channel_id_{0};
    preview_geometry geometry_;
};

class perception_result_stage final {
public:
    perception_result_stage(model_decode_stage& _decoder, tracking_stage& _tracker) noexcept;
    perception_result_stage(const perception_result_stage& _other) = delete;
    perception_result_stage& operator=(const perception_result_stage& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_prstg_configure(
        const perception_result_config& _config);
    [[nodiscard]] status vqec_vision_ai_appl_prstg_process(
        const tensor_result& _result, const submission_ticket& _ticket,
        std::uint64_t _now_monotonic_ns, bool _is_source_gap,
        observation_batch& _tracked);

private:
    model_decode_stage& decoder_;
    tracking_stage& tracker_;
    perception_result_config config_;
    bool is_configured_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_PERCEPTION_RESULT_STAGE_HPP
