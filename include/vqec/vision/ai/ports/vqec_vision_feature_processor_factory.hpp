#ifndef VQEC_VISION_AI_PORTS_FEATURE_PROCESSOR_FACTORY_HPP
#define VQEC_VISION_AI_PORTS_FEATURE_PROCESSOR_FACTORY_HPP

#include <memory>

#include "vqec/vision/ai/contracts/vqec_vision_feature_catalog.hpp"
#include "vqec/vision/ai/ports/vqec_vision_feature_processor.hpp"

namespace vqec::vision::ai {

// Cold-path compiled-in feature package boundary. Configuration is a borrowed view for
// each call. A created processor is a distinct owner and must copy any retained data.
class feature_processor_factory_port {
public:
    virtual ~feature_processor_factory_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_ports_ftfac_validate_configuration(
        const feature_catalog_entry& _feature,
        const feature_processor_config& _processor_config,
        const feature_configuration& _configuration) const = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_ftfac_create_processor(
        const feature_catalog_entry& _feature,
        const feature_processor_config& _processor_config,
        const feature_configuration& _configuration,
        std::unique_ptr<feature_processor_port>& _processor) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_FEATURE_PROCESSOR_FACTORY_HPP
