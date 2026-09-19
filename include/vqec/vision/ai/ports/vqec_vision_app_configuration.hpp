#ifndef VQEC_VISION_AI_PORTS_APP_CONFIGURATION_HPP
#define VQEC_VISION_AI_PORTS_APP_CONFIGURATION_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

class app_configuration_validator_port {
public:
    virtual ~app_configuration_validator_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apcfg_validate(
        const std::string& _schema_id, std::uint64_t _revision,
        const std::vector<std::uint8_t>& _payload) const = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_APP_CONFIGURATION_HPP

