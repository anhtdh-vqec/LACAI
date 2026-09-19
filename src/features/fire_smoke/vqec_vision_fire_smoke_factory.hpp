#ifndef VQEC_VISION_AI_FIRES_FIRE_SMOKE_FACTORY_HPP
#define VQEC_VISION_AI_FIRES_FIRE_SMOKE_FACTORY_HPP

#include "vqec/vision/ai/ports/vqec_vision_feature_processor_factory.hpp"
#include "vqec_vision_fire_smoke_alarm.hpp"

namespace vqec::vision::ai {

class fire_smoke_factory final : public feature_processor_factory_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_ftfac_validate_configuration(
        const feature_catalog_entry& _feature,
        const feature_processor_config& _processor_config,
        const feature_configuration& _configuration) const override;
    [[nodiscard]] status vqec_vision_ai_ports_ftfac_create_processor(
        const feature_catalog_entry& _feature,
        const feature_processor_config& _processor_config,
        const feature_configuration& _configuration,
        std::unique_ptr<feature_processor_port>& _processor) override;
};

[[nodiscard]] status vqec_vision_ai_fires_fsfac_parse_configuration(
    const feature_configuration& _configuration, fire_smoke_alarm_config& _config);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_FIRES_FIRE_SMOKE_FACTORY_HPP

