#include <cassert>
#include <cstdint>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_evidence_transport.hpp"

using namespace vqec::vision::ai;

namespace {

evidence_command vqec_vision_ai_unit_evtst_make_command() {
    evidence_command command;
    command.request_id_ = "fs:1:1:e";
    command.event_id_ = "fs:1:1";
    command.event_revision_ = 1U;
    command.source_id_ = "camera.front";
    command.feature_id_ = "fire_smoke_alarm";
    command.schema_id_ = "security.fire_smoke.event";
    command.schema_version_ = "1";
    command.frame_ = {1U, 0U, 2U, 30U, 1000000U};
    command.occurred_at_ns_ = command.frame_.source_pts_ns_;
    command.policy_revision_ = 7U;
    command.config_revision_ = 4U;
    command.fields_.push_back({"security.fire_smoke.evidence", "1",
        "security_alarm_v1,5000,10000,snapshot,clip,30000", 0.9F,
        observation_quality::high});
    return command;
}

}  // namespace

int main() {
    const auto command = vqec_vision_ai_unit_evtst_make_command();
    assert(vqec_vision_ai_core_evtrn_validate_command(command).code_ == status_code::ok);
    std::vector<std::uint8_t> wire;
    assert(vqec_vision_ai_core_evtrn_encode_command(command, wire).code_ == status_code::ok);
    evidence_command decoded;
    assert(vqec_vision_ai_core_evtrn_decode_command(
               wire.data(), wire.size(), decoded).code_ == status_code::ok);
    assert(decoded.request_id_ == command.request_id_);
    assert(decoded.event_id_ == command.event_id_);
    assert(decoded.fields_.size() == 1U);
    assert(decoded.fields_.front().value_ == command.fields_.front().value_);

    auto trailing = wire;
    trailing.push_back(0U);
    assert(vqec_vision_ai_core_evtrn_decode_command(
               trailing.data(), trailing.size(), decoded).code_ ==
           status_code::protocol_error);
    auto wrong_version = wire;
    wrong_version[8] = 2U;
    assert(vqec_vision_ai_core_evtrn_decode_command(
               wrong_version.data(), wrong_version.size(), decoded).code_ ==
           status_code::protocol_error);
    assert(vqec_vision_ai_core_evtrn_decode_command(nullptr, 0U, decoded).code_ ==
           status_code::invalid_argument);

    evidence_receipt receipt;
    receipt.request_id_ = command.request_id_;
    receipt.event_revision_ = command.event_revision_;
    receipt.state_ = evidence_receipt_state::ready;
    receipt.media_id_ = "media.fire.1";
    receipt.actual_begin_ns_ = 900000U;
    receipt.actual_end_ns_ = 1200000U;
    assert(vqec_vision_ai_core_evtrn_validate_receipt(receipt, command).code_ ==
           status_code::ok);
    assert(vqec_vision_ai_core_evtrn_encode_receipt(receipt, wire).code_ ==
           status_code::ok);
    evidence_receipt decoded_receipt;
    assert(vqec_vision_ai_core_evtrn_decode_receipt(
               wire.data(), wire.size(), decoded_receipt).code_ == status_code::ok);
    assert(decoded_receipt.media_id_ == receipt.media_id_);
    assert(vqec_vision_ai_core_evtrn_validate_receipt(
               decoded_receipt, command).code_ == status_code::ok);

    decoded_receipt.request_id_ = "different";
    assert(vqec_vision_ai_core_evtrn_validate_receipt(
               decoded_receipt, command).code_ == status_code::protocol_error);
    receipt.media_id_.clear();
    assert(vqec_vision_ai_core_evtrn_validate_receipt(receipt, command).code_ ==
           status_code::protocol_error);
    assert(vqec_vision_ai_core_evtrn_encode_receipt(receipt, wire).code_ ==
           status_code::invalid_argument);
    receipt.media_id_ = "media.fire.1";
    receipt.state_ = static_cast<evidence_receipt_state>(255U);
    assert(vqec_vision_ai_core_evtrn_encode_receipt(receipt, wire).code_ ==
           status_code::invalid_argument);
    receipt.state_ = evidence_receipt_state::ready;
    receipt.actual_begin_ns_ = 1200000U;
    receipt.actual_end_ns_ = 900000U;
    assert(vqec_vision_ai_core_evtrn_encode_receipt(receipt, wire).code_ ==
           status_code::invalid_argument);
    return 0;
}
