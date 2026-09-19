#ifndef VQEC_VISION_AI_SECAD_EDENT_ED25519_APP_ENTITLEMENT_VERIFIER_HPP
#define VQEC_VISION_AI_SECAD_EDENT_ED25519_APP_ENTITLEMENT_VERIFIER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/ports/management/vqec_vision_app_entitlement_verifier.hpp"
#include "vqec_vision_ed25519_verifier.hpp"

namespace vqec::vision::ai {

namespace ed25519_app_entitlement_limits {
inline constexpr std::size_t g_signature_bytes = ed25519_verifier_limits::g_signature_bytes;
inline constexpr char g_signing_domain[] = "VQEC-LACAI-ENTITLEMENT-1";
}  // namespace ed25519_app_entitlement_limits

struct ed25519_app_entitlement_verifier_config {
    std::string public_key_path_;
    std::string key_id_;
};

[[nodiscard]] status vqec_vision_ai_secad_edent_build_signing_payload(
    const std::vector<std::uint8_t>& _grant_payload,
    std::vector<std::uint8_t>& _signing_payload);

class ed25519_app_entitlement_verifier final : public app_entitlement_verifier_port {
public:
    explicit ed25519_app_entitlement_verifier(
        ed25519_app_entitlement_verifier_config _config);

    [[nodiscard]] status vqec_vision_ai_ports_entvr_verify(
        const app_entitlement_candidate& _candidate,
        verified_app_entitlement& _entitlement) const override;

private:
    ed25519_app_entitlement_verifier_config config_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_SECAD_EDENT_ED25519_APP_ENTITLEMENT_VERIFIER_HPP
