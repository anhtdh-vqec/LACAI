#ifndef VQEC_VISION_AI_QCOM_FACE_ENROLLMENT_IMAGE_SOURCE_HPP
#define VQEC_VISION_AI_QCOM_FACE_ENROLLMENT_IMAGE_SOURCE_HPP

#include <cstdint>

#include "vqec/vision/ai/ports/vqec_vision_face_enrollment_image.hpp"

namespace vqec::vision::ai {

struct qcom_face_enrollment_image_source_config {
    // Deployment-selected installed elements; no implicit decoder/converter fallback.
    std::string jpeg_decoder_factory_;
    std::string converter_factory_;
    std::string scaler_factory_;
    std::uint64_t max_image_bytes_{0};
    std::uint32_t timeout_ms_{0};
    bool require_dmabuf_{false};
};

class qcom_face_enrollment_image_source final
    : public face_enrollment_image_source_port {
public:
    explicit qcom_face_enrollment_image_source(
        qcom_face_enrollment_image_source_config _config) noexcept;
    [[nodiscard]] status vqec_vision_ai_ports_feimg_load(
        const face_enrollment_image_request& _request,
        face_enrollment_image& _image) override;

private:
    qcom_face_enrollment_image_source_config config_{};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QCOM_FACE_ENROLLMENT_IMAGE_SOURCE_HPP
