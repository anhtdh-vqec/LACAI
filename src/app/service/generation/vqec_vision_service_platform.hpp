#ifndef VQEC_VISION_AI_APPL_SERVICE_PLATFORM_HPP
#define VQEC_VISION_AI_APPL_SERVICE_PLATFORM_HPP

#include <memory>
#include <string>
#include <vector>

#include "vqec_vision_fake_platform.hpp"
#include "vqec_vision_production_platform.hpp"
#include "vqec_vision_reference_graph.hpp"
#include "vqec_vision_reference_platform.hpp"
#include "vqec_vision_reference_source.hpp"
#include "vqec_vision_runtime_composition_factory.hpp"
#include "vqec_vision_service_feature_registry.hpp"
#include "vqec_vision_service_options.hpp"

namespace vqec::vision::ai {

// Owns all platform-specific objects borrowed by one generation. Preparation validates metadata
// and composes recipes only; Qualcomm QNN/HTP and DSP remain closed until the first-frame gate.
class service_platform final {
public:
    [[nodiscard]] status vqec_vision_ai_appl_svplt_prepare(
        const parsed_arguments& _args, const deployment_config& _deployment,
        const model_catalog& _catalog, const feature_catalog& _features,
        const model_package_registry& _model_packages,
        bool _use_reference_platform, bool _use_production_platform);

    [[nodiscard]] production_platform&
    vqec_vision_ai_appl_svplt_get_production() noexcept;
    [[nodiscard]] model_decoder_registry&
    vqec_vision_ai_appl_svplt_get_decoders() noexcept;
    [[nodiscard]] tracker_registry&
    vqec_vision_ai_appl_svplt_get_trackers() noexcept;
    [[nodiscard]] feature_processor_registry&
    vqec_vision_ai_appl_svplt_get_features() noexcept;
    [[nodiscard]] runtime_composition_activation&
    vqec_vision_ai_appl_svplt_get_activation() noexcept;
    [[nodiscard]] const std::string&
    vqec_vision_ai_appl_svplt_get_tracker_contract() const noexcept;
    [[nodiscard]] const std::string&
    vqec_vision_ai_appl_svplt_get_attribute_schema() const noexcept;

private:
    fake_platform fake_;
    reference_platform reference_;
    production_platform production_;
    model_decoder_registry decoders_;
    tracker_registry trackers_;
    feature_processor_registry feature_registry_;
    service_feature_registry compiled_feature_factories_;
    feature_catalog platform_features_;
    std::vector<std::unique_ptr<reference_raw_source>> reference_sources_;
    std::vector<std::unique_ptr<reference_inference_graph>> reference_graphs_;
    std::vector<raw_source_port*> sources_;
    runtime_composition_activation activation_{};
    std::string tracker_contract_;
    std::string attribute_schema_id_;
    bool prepared_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_SERVICE_PLATFORM_HPP
