#ifndef VQEC_VISION_AI_FW_OUTPUT_EVIDENCE_UDS_CLIENT_HPP
#define VQEC_VISION_AI_FW_OUTPUT_EVIDENCE_UDS_CLIENT_HPP

#include <cstdint>
#include <string>

#include "vqec/vision/ai/ports/vqec_vision_evidence_transport.hpp"

namespace vqec::vision::ai {

struct evidence_uds_client_config {
    std::string socket_path_;
    std::uint32_t expected_peer_uid_{0};
    int io_timeout_ms_{0};
};

class evidence_uds_client final : public evidence_transport_port {
public:
    explicit evidence_uds_client(evidence_uds_client_config _config);
    ~evidence_uds_client() noexcept override;

    evidence_uds_client(const evidence_uds_client&) = delete;
    evidence_uds_client& operator=(const evidence_uds_client&) = delete;

    [[nodiscard]] status vqec_vision_ai_ports_evtrn_exchange(
        const evidence_command& _command, evidence_receipt& _receipt) override;

private:
    [[nodiscard]] status vqec_vision_ai_fwout_evuds_connect();
    void vqec_vision_ai_fwout_evuds_disconnect() noexcept;

    evidence_uds_client_config config_;
    int socket_fd_{-1};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_FW_OUTPUT_EVIDENCE_UDS_CLIENT_HPP
