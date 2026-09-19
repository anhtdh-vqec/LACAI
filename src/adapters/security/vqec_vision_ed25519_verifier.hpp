#ifndef VQEC_VISION_AI_SECAD_EDSIG_ED25519_VERIFIER_HPP
#define VQEC_VISION_AI_SECAD_EDSIG_ED25519_VERIFIER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"

namespace vqec::vision::ai {

namespace ed25519_verifier_limits {
inline constexpr std::size_t g_signature_bytes = 64;
inline constexpr std::size_t g_max_public_key_file_bytes = 16U * 1024U;
}  // namespace ed25519_verifier_limits

struct ed25519_verifier_config {
    std::string public_key_path_;
    std::string key_id_;
};

[[nodiscard]] status vqec_vision_ai_secad_edsig_verify(
    const ed25519_verifier_config& _config,
    const std::vector<std::uint8_t>& _message,
    const std::vector<std::uint8_t>& _signature);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_SECAD_EDSIG_ED25519_VERIFIER_HPP
