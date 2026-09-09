#ifndef VQEC_VISION_AI_APPL_SOURCE_PERCEPTION_FACTORY_HPP
#define VQEC_VISION_AI_APPL_SOURCE_PERCEPTION_FACTORY_HPP

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include "vqec_vision_multi_model_result_router.hpp"
#include "vqec_vision_perception_stage_factory.hpp"

namespace vqec::vision::ai {

struct perception_model_activation {
    std::string model_id_;
    std::string tracker_contract_;
};

// Owns every perception chain and the router for one deployment source. Slots
// preserve source.model_ids_ order, matching scheduler/pump/result slot identity.
class source_perception_bundle final {
public:
    source_perception_bundle(const source_perception_bundle& _other) = delete;
    source_perception_bundle& operator=(const source_perception_bundle& _other) = delete;

    [[nodiscard]] multi_model_result_router*
    vqec_vision_ai_appl_spfac_get_result_router() noexcept;
    [[nodiscard]] perception_stage_bundle*
    vqec_vision_ai_appl_spfac_get_stage_bundle(std::uint16_t _slot) noexcept;
    [[nodiscard]] std::uint16_t
    vqec_vision_ai_appl_spfac_get_model_count() const noexcept;

private:
    source_perception_bundle() = default;

    std::array<std::unique_ptr<perception_stage_bundle>,
        deployment_limits::g_max_models_per_source> stages_{};
    multi_model_result_router router_;
    std::uint16_t model_count_{0};

    friend status vqec_vision_ai_appl_spfac_create_source_bundle(
        const model_catalog& _models, const source_deployment_config& _source,
        const std::array<perception_model_activation,
            deployment_limits::g_max_models_per_source>& _activations,
        std::uint16_t _activation_count,
        const model_decoder_registry& _decoder_registry,
        const tracker_registry& _tracker_registry,
        std::unique_ptr<source_perception_bundle>& _bundle);
};

[[nodiscard]] status vqec_vision_ai_appl_spfac_create_source_bundle(
    const model_catalog& _models, const source_deployment_config& _source,
    const std::array<perception_model_activation,
        deployment_limits::g_max_models_per_source>& _activations,
    std::uint16_t _activation_count,
    const model_decoder_registry& _decoder_registry,
    const tracker_registry& _tracker_registry,
    std::unique_ptr<source_perception_bundle>& _bundle);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_SOURCE_PERCEPTION_FACTORY_HPP
