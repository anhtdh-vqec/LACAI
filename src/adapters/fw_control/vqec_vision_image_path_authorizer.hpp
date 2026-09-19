#ifndef VQEC_VISION_AI_FWCTL_IMAGE_PATH_AUTHORIZER_HPP
#define VQEC_VISION_AI_FWCTL_IMAGE_PATH_AUTHORIZER_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/ports/perception/vqec_vision_image_path_authorizer.hpp"

namespace vqec::vision::ai {

namespace image_path_authorizer_limits {
inline constexpr std::size_t g_max_allowed_roots = 16;
inline constexpr std::uint64_t g_max_file_bytes = 64ULL * 1024 * 1024;
}

struct image_path_authorizer_config {
    std::vector<std::string> allowed_roots_;
    std::uint64_t max_file_bytes_{0};
};

class image_path_authorizer final : public image_path_authorizer_port {
public:
    [[nodiscard]] status vqec_vision_ai_fwctl_ipath_configure(
        const image_path_authorizer_config& _config);
    [[nodiscard]] status vqec_vision_ai_ports_ipath_authorize(
        const std::string& _requested_path,
        authorized_image_path& _authorized_path) override;

private:
    std::vector<std::string> canonical_roots_;
    std::uint64_t max_file_bytes_{0};
    bool is_configured_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_FWCTL_IMAGE_PATH_AUTHORIZER_HPP
