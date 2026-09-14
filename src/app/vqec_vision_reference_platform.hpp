#ifndef VQEC_VISION_AI_APPL_REFERENCE_PLATFORM_HPP
#define VQEC_VISION_AI_APPL_REFERENCE_PLATFORM_HPP

#include <cstdint>
#include <memory>
#include <string>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec_vision_feature_catalog.hpp"
#include "vqec_vision_feature_processor_registry.hpp"
#include "vqec_vision_model_catalog.hpp"
#include "vqec_vision_model_decoder_registry.hpp"
#include "vqec_vision_reference_feature.hpp"
#include "vqec_vision_reference_tracker.hpp"
#include "vqec_vision_tracker_registry.hpp"

namespace vqec::vision::ai {

namespace reference_platform_limits {
inline constexpr char g_default_tracker_contract[] = "reference.tracker.v1";
inline constexpr char g_default_event_schema_id[] = "reference.zone";
inline constexpr char g_default_event_schema_version[] = "1";
}  // namespace reference_platform_limits

// Device-free production platform owner. It wires the real reference tracker and the
// real reference zone feature plus a deterministic fixture detector, so the
// decode -> track -> feature pipeline runs end to end without a board. It produces no
// model or hardware acceptance evidence and never loads an artifact.
struct reference_platform_config {
    std::uint32_t source_width_{0};
    std::uint32_t source_height_{0};
    std::string tracker_contract_{reference_platform_limits::g_default_tracker_contract};
    reference_tracker_config tracker_{};
    // A zone with no extent is filled to the whole frame during configure.
    reference_feature_params feature_{};
};

class reference_platform final {
public:
    reference_platform();
    ~reference_platform() noexcept;
    reference_platform(const reference_platform& _other) = delete;
    reference_platform& operator=(const reference_platform& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_rplat_configure(
        const reference_platform_config& _config);
    [[nodiscard]] status vqec_vision_ai_appl_rplat_register_decoders(
        const model_catalog& _catalog, model_decoder_registry& _decoders);
    [[nodiscard]] status vqec_vision_ai_appl_rplat_register_tracker(
        tracker_registry& _trackers);
    [[nodiscard]] status vqec_vision_ai_appl_rplat_register_features(
        const feature_catalog& _features, feature_processor_registry& _registry);
    [[nodiscard]] const std::string&
    vqec_vision_ai_appl_rplat_get_tracker_contract() const noexcept;
    [[nodiscard]] const std::string&
    vqec_vision_ai_appl_rplat_get_attribute_schema_id() const noexcept;
    [[nodiscard]] const reference_platform_config&
    vqec_vision_ai_appl_rplat_get_config() const noexcept;

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_REFERENCE_PLATFORM_HPP
