#ifndef VQEC_VISION_AI_CAMERA_RAW_SOURCE_RESOLVER_HPP
#define VQEC_VISION_AI_CAMERA_RAW_SOURCE_RESOLVER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "vqec_vision_source_lifecycle.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_deployment_config.hpp"

namespace vqec::vision::ai {

struct raw_source_route {
    std::string raw_source_ref_;
    std::uint32_t camera_id_{0};
    std::uint32_t channel_id_{0};
    std::string media_socket_path_;
    std::uint32_t producer_uid_{UINT32_MAX};
    std::uint32_t nv12_format_value_{0};
};

struct raw_source_cycle_identity {
    std::string consumer_id_;
    std::string start_request_id_;
    std::string stop_request_id_;
};

class raw_source_resolver {
public:
    virtual ~raw_source_resolver() = default;
    [[nodiscard]] virtual status vqec_vision_ai_camer_rsrsv_resolve_source(
        const std::string& _raw_source_ref, raw_source_route& _route) const = 0;
};

// Startup-only fixed-capacity resolver. Population is not authentication.
class static_raw_source_resolver final : public raw_source_resolver {
public:
    [[nodiscard]] status vqec_vision_ai_camer_rsrsv_add_route(
        const raw_source_route& _route);
    [[nodiscard]] status vqec_vision_ai_camer_rsrsv_resolve_source(
        const std::string& _raw_source_ref, raw_source_route& _route) const override;
    [[nodiscard]] std::uint16_t vqec_vision_ai_camer_rsrsv_get_count() const noexcept;

private:
    std::array<raw_source_route, deployment_limits::g_max_sources> routes_{};
    std::uint16_t route_count_{0};
};

// Compatibility helper for the inspected Camera Service release only. It is the sole
// owner of the legacy third-socket naming rule and rejects unsupported channel IDs.
[[nodiscard]] status vqec_vision_ai_camer_rsrsv_make_legacy_route(
    const source_deployment_config& _source, const std::string& _socket_dir,
    std::uint32_t _producer_uid, std::uint32_t _nv12_format_value,
    raw_source_route& _route);

// Cold-path composition after deployment validation and route resolution. Failure
// preserves output. The existing Camera1 adapter supports integer FPS responses only.
[[nodiscard]] status vqec_vision_ai_camer_rsrsv_compose_lifecycle(
    const source_deployment_config& _source, const raw_source_route& _route,
    const raw_source_cycle_identity& _identity, camera_lifecycle_config& _config);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CAMERA_RAW_SOURCE_RESOLVER_HPP
