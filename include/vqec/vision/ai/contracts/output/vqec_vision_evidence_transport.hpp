#ifndef VQEC_VISION_AI_CONTRACTS_EVIDENCE_TRANSPORT_HPP
#define VQEC_VISION_AI_CONTRACTS_EVIDENCE_TRANSPORT_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/features/vqec_vision_feature_event.hpp"
#include "vqec/vision/ai/contracts/base/vqec_vision_version_registry.h"

namespace vqec::vision::ai {

namespace evidence_transport_limits {
inline constexpr std::uint32_t g_protocol_major = VQEC_VISION_AI_BASELINE_ABI_MAJOR;
inline constexpr std::uint32_t g_protocol_minor = VQEC_VISION_AI_BASELINE_ABI_MINOR;
inline constexpr std::size_t g_max_message_bytes = 64U * 1024U;
inline constexpr std::size_t g_max_media_id_bytes = 256U;
inline constexpr std::size_t g_max_reason_bytes = 256U;
inline constexpr std::size_t g_max_attempts = 32U;
}  // namespace evidence_transport_limits

enum class evidence_receipt_state : std::uint8_t {
    fw_accepted = 1,
    recording,
    ready,
    partial,
    failed,
    rejected,
    expired
};

struct evidence_command {
    std::uint32_t protocol_major_{evidence_transport_limits::g_protocol_major};
    std::uint32_t protocol_minor_{evidence_transport_limits::g_protocol_minor};
    std::string request_id_;
    std::string event_id_;
    std::uint64_t event_revision_{0};
    std::string source_id_;
    std::string feature_id_;
    std::string schema_id_;
    std::string schema_version_;
    preview_frame_key frame_;
    std::uint64_t occurred_at_ns_{0};
    std::uint64_t policy_revision_{0};
    std::uint64_t config_revision_{0};
    std::vector<feature_event_field> fields_;
};

struct evidence_receipt {
    std::uint32_t protocol_major_{evidence_transport_limits::g_protocol_major};
    std::uint32_t protocol_minor_{evidence_transport_limits::g_protocol_minor};
    std::string request_id_;
    std::uint64_t event_revision_{0};
    evidence_receipt_state state_{evidence_receipt_state::failed};
    std::string media_id_;
    std::uint64_t actual_begin_ns_{0};
    std::uint64_t actual_end_ns_{0};
    std::string reason_;
};

[[nodiscard]] status vqec_vision_ai_core_evtrn_make_command(
    const feature_event& _event, evidence_command& _command);
[[nodiscard]] status vqec_vision_ai_core_evtrn_validate_command(
    const evidence_command& _command);
[[nodiscard]] status vqec_vision_ai_core_evtrn_validate_receipt(
    const evidence_receipt& _receipt, const evidence_command& _command);
[[nodiscard]] status vqec_vision_ai_core_evtrn_encode_command(
    const evidence_command& _command, std::vector<std::uint8_t>& _wire);
[[nodiscard]] status vqec_vision_ai_core_evtrn_decode_command(
    const std::uint8_t* _wire, std::size_t _wire_bytes, evidence_command& _command);
[[nodiscard]] status vqec_vision_ai_core_evtrn_encode_receipt(
    const evidence_receipt& _receipt, std::vector<std::uint8_t>& _wire);
[[nodiscard]] status vqec_vision_ai_core_evtrn_decode_receipt(
    const std::uint8_t* _wire, std::size_t _wire_bytes, evidence_receipt& _receipt);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_EVIDENCE_TRANSPORT_HPP
