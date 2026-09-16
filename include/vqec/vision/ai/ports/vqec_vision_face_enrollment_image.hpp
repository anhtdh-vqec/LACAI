#ifndef VQEC_VISION_AI_PORTS_FACE_ENROLLMENT_IMAGE_HPP
#define VQEC_VISION_AI_PORTS_FACE_ENROLLMENT_IMAGE_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_frame_descriptor.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

struct face_enrollment_image_request {
    std::string image_path_;
    std::uint64_t buffer_id_{0};
    std::uint64_t session_epoch_{0};
    std::uint32_t width_{0};
    std::uint32_t height_{0};
};

struct face_enrollment_image {
    frame_descriptor descriptor_;
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
