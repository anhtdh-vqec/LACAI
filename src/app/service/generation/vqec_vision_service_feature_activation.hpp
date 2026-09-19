#ifndef VQEC_VISION_AI_APPL_SERVICE_FEATURE_ACTIVATION_HPP
#define VQEC_VISION_AI_APPL_SERVICE_FEATURE_ACTIVATION_HPP

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include "vqec_vision_feature_activation_manager.hpp"
#include "vqec_vision_feature_fanout.hpp"
#include "vqec_vision_runtime_composition_factory.hpp"
#include "vqec_vision_service_options.hpp"
#include "vqec_vision_service_startup.hpp"

namespace vqec::vision::ai {

// Owns every processor, stage and fan-out borrowed by one runtime generation. Configuration is
// completed and frozen before the composition bundle borrows its wiring.
class service_feature_activation final {
public:
    [[nodiscard]] status vqec_vision_ai_appl_svfac_configure(
        const service_startup_resolution& _startup, const parsed_arguments& _args,
        const deployment_config& _deployment, const model_catalog& _catalog,
        const feature_catalog& _features, const feature_processor_registry& _registry,
        const std::string& _fallback_attribute_schema, output_gate& _output_gate,
        std::uint16_t _source_count);

    [[nodiscard]] const runtime_feature_activation*
    vqec_vision_ai_appl_svfac_get_wiring() const noexcept;

private:
    feature_activation_manager manager_;
    std::array<std::unique_ptr<feature_fanout>,
        deployment_limits::g_max_sources * deployment_limits::g_max_models_per_source>
        fanouts_{};
    runtime_feature_activation wiring_{};
    bool has_wiring_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_SERVICE_FEATURE_ACTIVATION_HPP
