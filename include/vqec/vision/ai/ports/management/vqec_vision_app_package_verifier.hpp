#ifndef VQEC_VISION_AI_PORTS_APP_PACKAGE_VERIFIER_HPP
#define VQEC_VISION_AI_PORTS_APP_PACKAGE_VERIFIER_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/lifecycle/vqec_vision_app_lifecycle.hpp"

namespace vqec::vision::ai {

struct app_component_candidate {
    // Borrowed read-only descriptor. The order is the signed manifest order after
    // filtering configuration entries and components for other targets.
    int descriptor_{-1};
};

struct app_package_candidate {
    std::vector<std::uint8_t> manifest_payload_;
    std::string manifest_sha256_;
    std::vector<std::uint8_t> configuration_payload_;
    std::string configuration_sha256_;
    std::vector<std::uint8_t> signature_payload_;
    std::vector<app_component_candidate> components_;
};

struct verified_app_package {
    usecase_app_manifest manifest_;
    std::string manifest_sha256_;
    std::vector<std::uint8_t> configuration_payload_;
    std::string configuration_sha256_;
    std::string verification_receipt_id_;
};

class app_package_verifier_port {
public:
    virtual ~app_package_verifier_port() = default;
    // Implementations own trust-store/signature policy. A digest-only implementation must
    // never return success from this authority boundary.
    [[nodiscard]] virtual status vqec_vision_ai_ports_apver_verify(
        const app_package_candidate& _candidate,
        verified_app_package& _package) const = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_APP_PACKAGE_VERIFIER_HPP
