#ifndef VQEC_VISION_AI_PORTS_FACE_ENROLLMENT_IMAGE_HPP
#define VQEC_VISION_AI_PORTS_FACE_ENROLLMENT_IMAGE_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_frame_descriptor.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/ports/vqec_vision_raw_source.hpp"

namespace vqec::vision::ai {

struct face_enrollment_image_request {
    std::string image_path_;
    std::uint64_t buffer_id_{0};
    std::uint64_t session_epoch_{0};
    std::uint32_t width_{0};
    std::uint32_t height_{0};
};

struct face_enrollment_image {
    // Production adapters return an importable native frame. The optional packed copy is
    // for portable/reference adapters and contract tests; hot Qualcomm paths leave it null.
    raw_frame frame_;
    std::shared_ptr<const std::vector<std::uint8_t>> nv12_;
};

class face_enrollment_image_source_port {
public:
    virtual ~face_enrollment_image_source_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_ports_feimg_load(
        const face_enrollment_image_request& _request,
        face_enrollment_image& _image) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_FACE_ENROLLMENT_IMAGE_HPP
