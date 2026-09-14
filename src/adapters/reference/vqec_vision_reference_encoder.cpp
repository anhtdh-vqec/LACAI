#include "vqec_vision_reference_encoder.hpp"

#include <utility>
#include <vector>

namespace vqec::vision::ai {
namespace {

void vqec_vision_ai_refer_renc_append_u64(std::vector<std::uint8_t>& _bytes,
    std::uint64_t _value) {
    for (unsigned shift = 0; shift < 8; ++shift) {
        _bytes.push_back(
            static_cast<std::uint8_t>((_value >> (shift * 8U)) & 0xFFU));
    }
}

}  // namespace

reference_encoder::reference_encoder(reference_encoder_config _config)
    : config_(_config) {}

status reference_encoder::vqec_vision_ai_cntr_encbk_submit(
    const encoder_input& _input) {
    if (is_draining_) {
        return {status_code::invalid_state, "reference encoder is draining"};
    }
    if (config_.max_pending_ == 0 || pending_.size() >= config_.max_pending_) {
        return {status_code::resource_exhausted, "reference encoder input queue is full"};
    }
    pending_input pending;
    pending.ticket_ = _input.ticket_;
    pending.frame_ = _input.frame_;
    pending.geometry_ = _input.geometry_;
    pending.dispatch_generation_ = _input.dispatch_generation_;
    pending_.push_back(std::move(pending));
    ++submitted_;
    return {};
}

status reference_encoder::vqec_vision_ai_cntr_encbk_poll(encoder_event& _event) {
    if (pending_.empty()) {
        return {status_code::pending, "no encoded access unit is ready"};
    }
    const pending_input input = pending_.front();
    pending_.pop_front();
    // Synthetic access unit: start code + frame id + PTS + dispatch generation.
    std::vector<std::uint8_t> payload{0x00, 0x00, 0x00, 0x01};
    vqec_vision_ai_refer_renc_append_u64(payload, input.frame_.frame_id_);
    vqec_vision_ai_refer_renc_append_u64(payload, input.frame_.source_pts_ns_);
    vqec_vision_ai_refer_renc_append_u64(payload, input.dispatch_generation_);
    h264_access_unit_view view;
    view.frame_ = input.frame_;
    view.geometry_ = input.geometry_;
    view.is_keyframe_ = true;
    view.payload_ = {payload.data(), payload.size()};
    std::shared_ptr<const owned_h264_output> output;
    const auto copied = owned_h264_output::vqec_vision_ai_core_encot_copy_output(
        view, input.frame_, input.geometry_, output);
    if (copied.code_ != status_code::ok) {
        return copied;
    }
    encoder_event event;
    event.kind_ = encoder_event_kind::output_ready;
    event.token_ = input.ticket_.token_;
    event.output_ = std::move(output);
    _event = std::move(event);
    ++delivered_;
    return {};
}

status reference_encoder::vqec_vision_ai_cntr_encbk_begin_drain() {
    is_draining_ = true;
    return pending_.empty() ?
        status{} :
        status{status_code::pending, "reference encoder still holds undelivered input"};
}

std::size_t reference_encoder::vqec_vision_ai_refer_renc_get_pending() const noexcept {
    return pending_.size();
}

std::uint64_t reference_encoder::vqec_vision_ai_refer_renc_get_submitted() const noexcept {
    return submitted_;
}

std::uint64_t reference_encoder::vqec_vision_ai_refer_renc_get_delivered() const noexcept {
    return delivered_;
}

}  // namespace vqec::vision::ai
