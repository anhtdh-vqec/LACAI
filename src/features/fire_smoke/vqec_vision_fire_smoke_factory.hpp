#ifndef VQEC_VISION_AI_FIRES_FIRE_SMOKE_FACTORY_HPP
#define VQEC_VISION_AI_FIRES_FIRE_SMOKE_FACTORY_HPP

#include "vqec/vision/ai/ports/vqec_vision_feature_processor_factory.hpp"
#include "vqec/vision/ai/ports/vqec_vision_app_configuration.hpp"
#include "vqec_vision_fire_smoke_alarm.hpp"

namespace vqec::vision::ai {

namespace fire_smoke_app_contract {
inline constexpr char g_app_id[] = "security.fire_smoke_detection";
inline constexpr char g_configuration_schema_id[] =
    "security.fire_smoke.configuration";
inline constexpr char g_processor_contract[] = "fire_smoke_alarm";
}  // namespace fire_smoke_app_contract

class fire_smoke_factory final : public feature_processor_factory_port,
                                 public app_configuration_validator_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_apcfg_validate(
        const std::string& _schema_id, std::uint64_t _revision,
        const std::vector<std::uint8_t>& _payload) const override;
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
