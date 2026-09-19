#ifndef VQEC_VISION_AI_PORTS_IMAGE_PATH_AUTHORIZER_HPP
#define VQEC_VISION_AI_PORTS_IMAGE_PATH_AUTHORIZER_HPP

#include <memory>
#include <string>

#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"

namespace vqec::vision::ai {

struct authorized_image_path {
    // Path may refer to a retained /proc/self/fd entry. Keep owner alive through decode.
    std::string path_;
    std::shared_ptr<const void> owner_;
};

class image_path_authorizer_port {
public:
    virtual ~image_path_authorizer_port() = default;
    // Resolves one FW path to an immutable authorized local path. Failure preserves output.
    [[nodiscard]] virtual status vqec_vision_ai_ports_ipath_authorize(
        const std::string& _requested_path, authorized_image_path& _authorized_path) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_IMAGE_PATH_AUTHORIZER_HPP
