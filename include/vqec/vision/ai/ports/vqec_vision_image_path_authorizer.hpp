#ifndef VQEC_VISION_AI_PORTS_IMAGE_PATH_AUTHORIZER_HPP
#define VQEC_VISION_AI_PORTS_IMAGE_PATH_AUTHORIZER_HPP

#include <string>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

class image_path_authorizer_port {
public:
    virtual ~image_path_authorizer_port() = default;
    // Resolves one FW path to an immutable authorized local path. Failure preserves output.
    [[nodiscard]] virtual status vqec_vision_ai_ports_ipath_authorize(
        const std::string& _requested_path, std::string& _authorized_path) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_IMAGE_PATH_AUTHORIZER_HPP
