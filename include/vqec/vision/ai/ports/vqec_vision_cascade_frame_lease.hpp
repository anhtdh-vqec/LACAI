#ifndef VQEC_VISION_AI_PORTS_CASCADE_FRAME_LEASE_HPP
#define VQEC_VISION_AI_PORTS_CASCADE_FRAME_LEASE_HPP

#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_preview_contract.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/ports/vqec_vision_raw_source.hpp"

namespace vqec::vision::ai {

// Session-owned retained-frame lease used by the cascade coordinator. Acquire returns the
// exact retained owner and a completion ticket; retire closes admission for the key once no
// more dependents will acquire; complete releases a ticket only after the dependent hardware
// read finished. Timeout and stop are not completion.
class cascade_frame_lease_port {
public:
    virtual ~cascade_frame_lease_port() = default;

    [[nodiscard]] virtual status vqec_vision_ai_ports_cflse_acquire(
        const preview_frame_key& _key, raw_frame& _frame, std::uint64_t& _ticket) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_cflse_retire(
        const preview_frame_key& _key) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_cflse_complete(
        std::uint64_t _ticket) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_CASCADE_FRAME_LEASE_HPP
