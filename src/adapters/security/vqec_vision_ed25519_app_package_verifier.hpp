#ifndef VQEC_VISION_AI_SECAD_EDVER_ED25519_APP_PACKAGE_VERIFIER_HPP
#define VQEC_VISION_AI_SECAD_EDVER_ED25519_APP_PACKAGE_VERIFIER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/ports/vqec_vision_app_package_verifier.hpp"

namespace vqec::vision::ai {

namespace ed25519_app_package_limits {
inline constexpr std::size_t g_signature_bytes = 64;
inline constexpr std::size_t g_max_public_key_file_bytes = 16U * 1024U;
inline constexpr char g_signing_domain[] = "VQEC-LACAI-VQAPP-1";
}  // namespace ed25519_app_package_limits

struct ed25519_app_package_verifier_config {
    std::string public_key_path_;
    std::string key_id_;
};

[[nodiscard]] status vqec_vision_ai_secad_edver_build_signing_payload(
    const std::vector<std::uint8_t>& _manifest_payload,
    const std::vector<std::uint8_t>& _configuration_payload,
    std::vector<std::uint8_t>& _signing_payload);

class ed25519_app_package_verifier final : public app_package_verifier_port {
public:
    explicit ed25519_app_package_verifier(
        ed25519_app_package_verifier_config _config);

    [[nodiscard]] status vqec_vision_ai_ports_apver_verify(
        const app_package_candidate& _candidate,
        verified_app_package& _package) const override;

private:
    ed25519_app_package_verifier_config config_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_SECAD_EDVER_ED25519_APP_PACKAGE_VERIFIER_HPP
