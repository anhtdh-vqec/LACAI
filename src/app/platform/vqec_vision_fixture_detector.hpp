#ifndef VQEC_VISION_AI_APPL_FIXTURE_DETECTOR_HPP
#define VQEC_VISION_AI_APPL_FIXTURE_DETECTOR_HPP

#include <cstdint>
#include <string>

#include "vqec/vision/ai/contracts/inference/vqec_vision_model_decoder.hpp"

namespace vqec::vision::ai {

namespace fixture_detector_limits {
inline constexpr std::uint32_t g_default_width = 640;
inline constexpr std::uint32_t g_default_height = 480;
inline constexpr float g_default_confidence = 0.5F;
inline constexpr char g_default_class_id[] = "person";
// Deterministic box as a fraction of the source, so the fixture sits inside any
// full-frame zone without a source-specific pixel literal.
inline constexpr float g_box_x_fraction = 0.25F;
inline constexpr float g_box_y_fraction = 0.25F;
inline constexpr float g_box_width_fraction = 0.5F;
inline constexpr float g_box_height_fraction = 0.5F;
}  // namespace fixture_detector_limits

struct fixture_detector_config {
    std::uint32_t width_{fixture_detector_limits::g_default_width};
    std::uint32_t height_{fixture_detector_limits::g_default_height};
    std::string class_id_{fixture_detector_limits::g_default_class_id};
    float confidence_{fixture_detector_limits::g_default_confidence};
};

// Device-free deterministic detector shared by the development fake platform and the
// reference platform. It emits one fixed box per result and performs no tensor
// semantics: it exists to exercise decode -> track -> feature composition without a
// model. It is not model, accuracy or hardware evidence.
class fixture_detector final : public model_decoder_port {
public:
    fixture_detector() = default;
    explicit fixture_detector(fixture_detector_config _config) noexcept;

    [[nodiscard]] status vqec_vision_ai_cntr_mddec_validate(
        const model_outputs& _outputs) const override;
    [[nodiscard]] status vqec_vision_ai_cntr_mddec_decode(
        const tensor_result& _result, const preview_frame_key& _expected_frame,
        observation_batch& _observations) override;

private:
    fixture_detector_config config_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_FIXTURE_DETECTOR_HPP
