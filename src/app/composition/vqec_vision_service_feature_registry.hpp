#ifndef VQEC_VISION_AI_APPL_SFREG_SERVICE_FEATURE_REGISTRY_HPP
#define VQEC_VISION_AI_APPL_SFREG_SERVICE_FEATURE_REGISTRY_HPP

#include "vqec_vision_feature_processor_registry.hpp"
#include "vqec_vision_fire_smoke_factory.hpp"

namespace vqec::vision::ai {

// Owns product feature factories for the full lifetime of one runtime generation.
// Platform adapters receive only contracts not owned by the shared service.
class service_feature_registry final {
public:
    [[nodiscard]] status vqec_vision_ai_appl_sfreg_register_compiled(
        const feature_catalog& _catalog, feature_processor_registry& _registry,
        feature_catalog& _platform_catalog);

private:
    fire_smoke_factory fire_smoke_factory_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_SFREG_SERVICE_FEATURE_REGISTRY_HPP
