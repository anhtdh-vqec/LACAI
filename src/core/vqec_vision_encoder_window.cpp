#include "vqec/vision/ai/contracts/vqec_vision_encoder_window.hpp"

#include "vqec/vision/ai/contracts/vqec_vision_preview_limits.hpp"

namespace vqec::vision::ai {

status encoder_window::vqec_vision_ai_core_encwn_validate_event(const encoder_event& _event) const {
    const auto valid = vqec_vision_ai_core_encct_validate_event(_event);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    if (_event.kind_ == encoder_event_kind::fault && _event.token_.job_id_ == 0) {
        return configured_ ? status{} : status{status_code::invalid_state, "encoder not configured"};
    }
    for (const auto& item : entries_) {
        if (item.token_.cycle_id_ != _event.token_.cycle_id_ ||
            item.token_.job_id_ != _event.token_.job_id_) {
            continue;
        }
        if (!item.committed_) {
            return {status_code::invalid_state, "encoder event precedes submission commit"};
        }
        if ((_event.kind_ == encoder_event_kind::input_complete && item.input_done_) ||
            ((_event.kind_ == encoder_event_kind::output_ready ||
              _event.kind_ == encoder_event_kind::output_dropped) && item.result_done_)) {
            return {status_code::invalid_state, "duplicate encoder completion event"};
        }
        if (_event.kind_ == encoder_event_kind::output_ready) {
            return vqec_vision_ai_core_pvctr_validate_access_unit(
                _event.output_->vqec_vision_ai_core_encot_borrow_view(), item.frame_, config_.geometry_);
        }
        return {};
    }
    return {status_code::invalid_state, "encoder event does not match an outstanding job"};
}

status encoder_window::vqec_vision_ai_core_encwn_apply_event(const encoder_event& _event) {
    const auto valid = vqec_vision_ai_core_encwn_validate_event(_event);
    if (valid.code_ != status_code::ok) {
        ledger_.vqec_vision_ai_core_subwn_mark_fault();
        return valid;
    }
    status result;
    switch (_event.kind_) {
        case encoder_event_kind::input_complete:
            result = vqec_vision_ai_core_encwn_complete_input(_event.token_);
            break;
        case encoder_event_kind::output_ready:
            result = vqec_vision_ai_core_encwn_complete_result(
                _event.token_, _event.output_->vqec_vision_ai_core_encot_borrow_view());
            break;
        case encoder_event_kind::output_dropped:
            result = vqec_vision_ai_core_encwn_complete_dropped_result(_event.token_);
            break;
        case encoder_event_kind::fault:
            ledger_.vqec_vision_ai_core_subwn_mark_fault();
            return _event.detail_; // Fault is neither input nor result completion.
        default:
            ledger_.vqec_vision_ai_core_subwn_mark_fault();
            return {status_code::protocol_error, "unknown encoder completion kind"};
    }
    if (result.code_ != status_code::ok) {
        ledger_.vqec_vision_ai_core_subwn_mark_fault();
    }
    return result;
}

status encoder_window::vqec_vision_ai_core_encwn_configure(const encoder_window_config& _config) {
    if (configured_) {
        return {status_code::invalid_state, "encoder window already configured"};
    }
    const auto geometry = _config.geometry_;
    if (geometry.width_ == 0 || geometry.height_ == 0 || geometry.width_ > preview_limits::g_max_dimension_pixels ||
        geometry.height_ > preview_limits::g_max_dimension_pixels || geometry.width_ % 2 != 0 || geometry.height_ % 2 != 0 ||
        _config.max_input_bytes_ == 0 || _config.max_input_bytes_ > preview_limits::g_max_pool_bytes) {
        return {status_code::invalid_argument, "invalid encoder geometry or memory budget"};
    }
    const auto pixels = static_cast<std::uint64_t>(geometry.width_) * geometry.height_;
    const auto bytes = pixels + pixels / 2;
    if (bytes > preview_limits::g_max_surface_bytes || bytes > _config.max_input_bytes_) {
        return {status_code::resource_exhausted, "encoder surface exceeds budget"};
    }
    const auto result = ledger_.vqec_vision_ai_core_subwn_configure(_config.submission_);
    if (result.code_ != status_code::ok) {
        return result;
    }
    config_ = _config;
    surface_bytes_ = bytes;
    configured_ = true;
    return {};
}

bool encoder_window::vqec_vision_ai_core_encwn_accept_clock(std::uint64_t _now_ns) noexcept {
    if (_now_ns == UINT64_MAX || _now_ns < last_now_ns_) {
        return false;
    }
    last_now_ns_ = _now_ns;
    return true;
}

status encoder_window::vqec_vision_ai_core_encwn_reserve(
    const preview_frame_key& _frame, bool _has_demand, std::uint64_t _now_ns,
    submission_ticket& _ticket) {
    if (!configured_ || !ledger_.vqec_vision_ai_core_subwn_is_accepting()) {
        return {status_code::invalid_state, "encoder is not accepting"};
    }
    if (!vqec_vision_ai_core_encwn_accept_clock(_now_ns)) {
        return {status_code::invalid_argument, "encoder monotonic clock moved backwards"};
    }
    if (!_has_demand) {
        return {status_code::pending, "no preview consumer demand"};
    }
    if (_frame.camera_id_ != config_.camera_id_ || _frame.channel_id_ != config_.channel_id_) {
        return {status_code::invalid_argument, "encoder source mismatch"};
    }
    if (surface_bytes_ > config_.max_input_bytes_ - reserved_bytes_) {
        return {status_code::resource_exhausted, "encoder input budget full"};
    }
    entry* available = nullptr;
    for (auto& item : entries_) {
        if (item.token_.job_id_ == 0) {
            available = &item;
            break;
        }
    }
    if (available == nullptr) {
        return {status_code::resource_exhausted, "encoder window full"};
    }
    submission_ticket ticket;
    const auto result = ledger_.vqec_vision_ai_core_subwn_reserve(
        _frame.source_epoch_, _frame.frame_id_, _frame.source_pts_ns_, _now_ns, ticket);
    if (result.code_ != status_code::ok) {
        return result;
    }
    *available = {ticket.token_, _frame, false, false};
    available->pipeline_pts_ns_ = ticket.pipeline_pts_ns_;
    reserved_bytes_ += surface_bytes_;
    _ticket = ticket;
    return {};
}

encoder_window::entry* encoder_window::vqec_vision_ai_core_encwn_find(
    submission_token _token) noexcept {
    for (auto& item : entries_) {
        if (_token.job_id_ != 0 && item.token_.job_id_ == _token.job_id_ &&
            item.token_.cycle_id_ == _token.cycle_id_) {
            return &item;
        }
    }
    return nullptr;
}

status encoder_window::vqec_vision_ai_core_encwn_commit(submission_token _token) {
    auto* item = vqec_vision_ai_core_encwn_find(_token);
    if (item == nullptr) {
        return {status_code::invalid_state, "unknown encoder commit token"};
    }
    const auto result = ledger_.vqec_vision_ai_core_subwn_commit(_token);
    if (result.code_ == status_code::ok) {
        item->committed_ = true;
    }
    return result;
}

status encoder_window::vqec_vision_ai_core_encwn_cancel_reserved(submission_token _token) {
    auto* item = vqec_vision_ai_core_encwn_find(_token);
    if (item == nullptr) {
        return {status_code::invalid_state, "unknown encoder reservation"};
    }
    const auto result = ledger_.vqec_vision_ai_core_subwn_cancel_reserved(_token);
    if (result.code_ == status_code::ok) {
        *item = {};
        reserved_bytes_ -= surface_bytes_;
    }
    return result;
}

status encoder_window::vqec_vision_ai_core_encwn_begin_submission(submission_token _token) {
    auto* item = vqec_vision_ai_core_encwn_find(_token);
    if (item == nullptr || !item->committed_ || item->submission_attempted_ ||
        item->input_done_ || item->result_done_ ||
        !ledger_.vqec_vision_ai_core_subwn_is_accepting()) {
        return {status_code::invalid_state, "encoder submission is stale, repeated or stopped"};
    }
    item->submission_attempted_ = true;
    return {};
}

status encoder_window::vqec_vision_ai_core_encwn_begin_input(
    const encoder_input& _input, std::uint64_t _expected_generation) {
    const auto* item = vqec_vision_ai_core_encwn_find(_input.ticket_.token_);
    if (item == nullptr) {
        return {status_code::invalid_state, "encoder input has no reservation"};
    }
    const submission_ticket expected_ticket{
        item->token_, item->frame_.source_epoch_, item->frame_.frame_id_,
        item->frame_.source_pts_ns_, item->pipeline_pts_ns_};
    const auto valid = vqec_vision_ai_core_encct_validate_input(
        _input, item->frame_, config_.geometry_, expected_ticket, _expected_generation);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    return vqec_vision_ai_core_encwn_begin_submission(_input.ticket_.token_);
}

status encoder_window::vqec_vision_ai_core_encwn_complete(submission_token _token, bool _input) {
    auto* item = vqec_vision_ai_core_encwn_find(_token);
    if (item == nullptr) {
        return {status_code::invalid_state, "unknown encoder completion"};
    }
    const auto result = _input ? ledger_.vqec_vision_ai_core_subwn_complete_input(_token)
                              : ledger_.vqec_vision_ai_core_subwn_complete_result(_token);
    if (result.code_ != status_code::ok) {
        return result;
    }
    if (_input) {
        item->input_done_ = true;
    } else {
        item->result_done_ = true;
    }
    if (item->input_done_ && item->result_done_) {
        *item = {};
        reserved_bytes_ -= surface_bytes_;
    }
    return {};
}

status encoder_window::vqec_vision_ai_core_encwn_complete_input(submission_token _token) {
    return vqec_vision_ai_core_encwn_complete(_token, true);
}

status encoder_window::vqec_vision_ai_core_encwn_complete_result(
    submission_token _token, const h264_access_unit_view& _unit) {
    const auto* item = vqec_vision_ai_core_encwn_find(_token);
    if (item == nullptr) {
        return {status_code::invalid_state, "uncorrelated encoder result"};
    }
    const auto result = vqec_vision_ai_core_pvctr_validate_access_unit(
        _unit, item->frame_, config_.geometry_);
    if (result.code_ != status_code::ok) {
        return result;
    }
    return vqec_vision_ai_core_encwn_complete(_token, false);
}

status encoder_window::vqec_vision_ai_core_encwn_complete_dropped_result(submission_token _token) {
    return vqec_vision_ai_core_encwn_complete(_token, false);
}

status encoder_window::vqec_vision_ai_core_encwn_check_deadlines(std::uint64_t _now_ns) {
    if (!configured_) {
        return {status_code::invalid_state, "encoder window not configured"};
    }
    if (!vqec_vision_ai_core_encwn_accept_clock(_now_ns)) {
        return {status_code::invalid_argument, "encoder monotonic clock moved backwards"};
    }
    return ledger_.vqec_vision_ai_core_subwn_check_deadlines(_now_ns);
}

void encoder_window::vqec_vision_ai_core_encwn_begin_drain() noexcept {
    ledger_.vqec_vision_ai_core_subwn_begin_drain();
}

void encoder_window::vqec_vision_ai_core_encwn_mark_fault() noexcept {
    ledger_.vqec_vision_ai_core_subwn_mark_fault();
}

unsigned encoder_window::vqec_vision_ai_core_encwn_outstanding() const noexcept {
    return ledger_.vqec_vision_ai_core_subwn_get_outstanding();
}

std::uint64_t encoder_window::vqec_vision_ai_core_encwn_reserved_bytes() const noexcept {
    return reserved_bytes_;
}

preview_geometry encoder_window::vqec_vision_ai_core_encwn_geometry() const noexcept {
    return config_.geometry_;
}

}  // namespace vqec::vision::ai
