#ifndef VQEC_VISION_AI_CONTRACTS_ENCODED_SINK_HPP
#define VQEC_VISION_AI_CONTRACTS_ENCODED_SINK_HPP

#include "vqec/vision/ai/contracts/vqec_vision_preview_contract.hpp"

namespace vqec::vision::ai {

struct encoded_sink_demand {
    // Runtime-unique binding identity across sink instances, not an SDK-local reopen count.
    std::uint64_t mapping_generation_{0};
    unsigned active_consumers_{0};
};

// Internal synchronous-copy C++ port. Caller serializes calls and retains sink lifetime.
// No implementation, ring ABI, thread or implicit open/close is supplied here.
class encoded_sink {
public:
    virtual ~encoded_sink() = default;
    // Failure preserves _demand; success must report a nonzero ready mapping generation.
    [[nodiscard]] virtual status vqec_vision_ai_cntr_encsk_query_demand(
        encoded_sink_demand& _demand) = 0;
    // Reject wrong generation before write. Validate configured source/geometry and AU bounds.
    // ok = bytes synchronously copied into ring, NOT merely queued or delivered to a viewer.
    // Never retain borrowed pointers beyond return. No implicit retries or authorization grant.
    [[nodiscard]] virtual status vqec_vision_ai_cntr_encsk_write(
        const h264_access_unit_view& _unit, std::uint64_t _expected_generation) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_ENCODED_SINK_HPP
