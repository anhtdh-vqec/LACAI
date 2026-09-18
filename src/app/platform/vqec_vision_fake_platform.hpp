#ifndef VQEC_VISION_AI_APP_FAKE_PLATFORM_HPP
#define VQEC_VISION_AI_APP_FAKE_PLATFORM_HPP

#include <cstdint>
#include <memory>
#include <string>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_version_registry.h"
#include "vqec_vision_feature_catalog.hpp"
#include "vqec_vision_feature_processor_registry.hpp"
#include "vqec_vision_model_catalog.hpp"
#include "vqec_vision_model_decoder_registry.hpp"
#include "vqec_vision_tracker_registry.hpp"

namespace vqec::vision::ai {

// Device-free fake platform owners used by `--mode production --platform fake` and by the
// development harness. They are explicitly selected, never an implicit fallback: production
// without a named platform fails closed. They prove wiring and lifecycle only and produce no
// real detection, so they are not model or hardware acceptance evidence.
struct fake_platform_config {
    std::uint32_t source_width_{0};
    std::uint32_t source_height_{0};
    std::string tracker_contract_{"fake.tracker.v1"};
    std::string event_schema_id_{"fake.event"};
    std::string event_schema_version_{VQEC_VISION_AI_BASELINE_VERSION_TEXT};
    std::string attribute_schema_id_{"fake.attribute"};
    std::string attribute_schema_version_{VQEC_VISION_AI_BASELINE_VERSION_TEXT};
};

class fake_platform final {
public:
    fake_platform();
    ~fake_platform() noexcept;
    fake_platform(const fake_platform& _other) = delete;
    fake_platform& operator=(const fake_platform& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_fkplt_configure(
        const fake_platform_config& _config);
    [[nodiscard]] status vqec_vision_ai_appl_fkplt_register_decoders(
        const model_catalog& _catalog, model_decoder_registry& _decoders);
    [[nodiscard]] status vqec_vision_ai_appl_fkplt_register_tracker(
        tracker_registry& _trackers);
    [[nodiscard]] status vqec_vision_ai_appl_fkplt_register_features(
        const feature_catalog& _features, feature_processor_registry& _registry);
    [[nodiscard]] const std::string&
    vqec_vision_ai_appl_fkplt_get_tracker_contract() const noexcept;
    [[nodiscard]] const fake_platform_config&
    vqec_vision_ai_appl_fkplt_get_config() const noexcept;

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_FAKE_PLATFORM_HPP
