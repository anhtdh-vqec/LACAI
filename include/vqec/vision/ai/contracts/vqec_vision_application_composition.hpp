#ifndef VQEC_VISION_AI_CONTRACTS_APPLICATION_COMPOSITION_HPP
#define VQEC_VISION_AI_CONTRACTS_APPLICATION_COMPOSITION_HPP

#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

enum class application_composition_state {
    idle,
    validating,
    activated,
    running,
    stopping,
    stopped,
    faulted
};

struct application_composition_snapshot {
    application_composition_state state_{application_composition_state::idle};
    std::uint64_t deployment_revision_{0};
    std::uint64_t catalog_revision_{0};
    std::uint16_t declared_sources_{0};
    std::uint16_t admitted_sources_{0};
    status_code first_error_code_{status_code::ok};
    bool is_recovery_required_{false};
};

// Composition owns the activation revision and borrowed session owners. It must
// activate only after authentication, admission and complete owner construction.
// Destruction never performs RPC, cancellation or implicit buffer reclamation.
class application_composition_port {
public:
    virtual ~application_composition_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_cntr_acomp_validate() = 0;
    [[nodiscard]] virtual status vqec_vision_ai_cntr_acomp_activate() = 0;
    [[nodiscard]] virtual status vqec_vision_ai_cntr_acomp_step(
        std::uint64_t _steady_now_ns) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_cntr_acomp_request_stop(
        std::uint64_t _steady_now_ns) = 0;
    [[nodiscard]] virtual application_composition_snapshot
    vqec_vision_ai_cntr_acomp_get_snapshot() const noexcept = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_APPLICATION_COMPOSITION_HPP
