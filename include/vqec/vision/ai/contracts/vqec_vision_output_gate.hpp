#ifndef VQEC_VISION_AI_CONTRACTS_OUTPUT_GATE_HPP
#define VQEC_VISION_AI_CONTRACTS_OUTPUT_GATE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

namespace output_policy_limits {
// AI metadata safety ceilings, not FW wire limits or commercial feature counts.
// Changing these requires output-boundary review and matching boundary tests.
inline constexpr std::size_t g_max_identifier_bytes = 128;
inline constexpr std::size_t g_max_attributes_per_scope = 64;
inline constexpr std::size_t g_max_policy_rules = 64;
inline constexpr std::size_t g_max_rendered_scopes = 16;
}  // namespace output_policy_limits

struct output_scope_rule {
    std::string source_id_;
    std::string feature_id_;
    std::vector<std::string> attributes_;
};

// Trusted provider projection, not a signed grant or externally deserializable authority.
struct output_policy {
    std::uint64_t revision_{0};
    std::uint64_t not_before_ns_{0};
    std::uint64_t expires_ns_{0};
    std::vector<output_scope_rule> rules_;
};

struct output_authorization {
    std::uint64_t policy_revision_{0};
    std::string source_id_;
    std::string feature_id_;
    std::vector<std::string> attributes_;
};

// Serialized with policy updates AND final delivery. No I/O or cryptographic verification.
// Updates allocate bounded metadata and may throw bad_alloc; no partial policy commit.
class output_gate {
public:
    output_gate() = default;
    output_gate(const output_gate& _other) = delete;
    output_gate& operator=(const output_gate& _other) = delete;
    [[nodiscard]] status vqec_vision_ai_core_otgat_apply_policy(
        const output_policy& _policy, std::uint64_t _expected_revision);
    [[nodiscard]] status vqec_vision_ai_core_otgat_authorize(
        const output_authorization& _request, std::uint64_t _steady_now_ns);
    // Emergency deny; preserves revision watermark. No frame/job release.
    void vqec_vision_ai_core_otgat_invalidate() noexcept;
    [[nodiscard]] std::uint64_t vqec_vision_ai_core_otgat_get_revision() const noexcept;

private:
    output_policy policy_;
    std::uint64_t last_now_ns_{0};
    bool is_active_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_OUTPUT_GATE_HPP
