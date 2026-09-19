#ifndef VQEC_VISION_AI_PORTS_EVIDENCE_TRANSPORT_HPP
#define VQEC_VISION_AI_PORTS_EVIDENCE_TRANSPORT_HPP

#include "vqec/vision/ai/contracts/output/vqec_vision_evidence_transport.hpp"

namespace vqec::vision::ai {

class evidence_transport_port {
public:
    virtual ~evidence_transport_port() = default;
    // One bounded request/receipt exchange. A timeout or I/O error is ambiguous and
    // callers retry the identical request ID and payload.
    [[nodiscard]] virtual status vqec_vision_ai_ports_evtrn_exchange(
        const evidence_command& _command, evidence_receipt& _receipt) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_EVIDENCE_TRANSPORT_HPP
