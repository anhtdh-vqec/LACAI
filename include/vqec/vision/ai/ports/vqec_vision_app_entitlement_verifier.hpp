#ifndef VQEC_VISION_AI_PORTS_APP_ENTITLEMENT_VERIFIER_HPP
#define VQEC_VISION_AI_PORTS_APP_ENTITLEMENT_VERIFIER_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_app_lifecycle.hpp"

namespace vqec::vision::ai {

struct app_entitlement_candidate {
    std::vector<std::uint8_t> grant_payload_;
    std::string grant_sha256_;
    std::vector<std::uint8_t> signature_payload_;
};

struct verified_app_entitlement {
    app_entitlement_grant grant_;
    std::string grant_sha256_;
    std::string verification_receipt_id_;
};

class app_entitlement_verifier_port {
public:
    virtual ~app_entitlement_verifier_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_ports_entvr_verify(
        const app_entitlement_candidate& _candidate,
        verified_app_entitlement& _entitlement) const = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_APP_ENTITLEMENT_VERIFIER_HPP
