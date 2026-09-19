#ifndef VQEC_VISION_AI_PORTS_EVIDENCE_OUTBOX_HPP
#define VQEC_VISION_AI_PORTS_EVIDENCE_OUTBOX_HPP

#include <cstddef>
#include <cstdint>

#include "vqec/vision/ai/contracts/output/vqec_vision_evidence_transport.hpp"

namespace vqec::vision::ai {

struct evidence_outbox_record {
    evidence_command command_;
    std::size_t attempt_count_{0};
    std::uint64_t next_attempt_monotonic_ns_{0};
};

struct evidence_outbox_stats {
    std::uint64_t committed_commands_{0};
    std::uint64_t idempotent_commands_{0};
    std::uint64_t completed_commands_{0};
    std::uint64_t retry_schedules_{0};
    std::uint64_t recovered_claims_{0};
    std::size_t pending_commands_{0};
};

class evidence_outbox_port {
public:
    virtual ~evidence_outbox_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_ports_evobx_open() = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_evobx_enqueue(
        const evidence_command& _command) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_evobx_claim_due(
        std::uint64_t _now_monotonic_ns, evidence_outbox_record& _record) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_evobx_retry(
        const std::string& _request_id, std::size_t _attempt_count,
        std::uint64_t _next_attempt_monotonic_ns, const std::string& _reason) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_evobx_complete(
        const evidence_receipt& _receipt) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_evobx_get_stats(
        evidence_outbox_stats& _stats) const = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_EVIDENCE_OUTBOX_HPP
