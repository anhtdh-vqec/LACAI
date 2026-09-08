#include "vqec/vision/ai/contracts/vqec_vision_submission_window.hpp"

namespace vqec::vision::ai {

status submission_window::vqec_vision_ai_core_subwn_configure(const submission_config& _config) {
    if (configured_) {
        return {status_code::invalid_state, "submission window already configured"};
    }
    if (_config.cycle_id_ == 0 || _config.source_epoch_ == 0 || _config.capacity_ == 0 ||
        _config.capacity_ > slots_.size() || _config.pipeline_anchor_ns_ == UINT64_MAX ||
        _config.job_timeout_ns_ == 0 || _config.job_timeout_ns_ == UINT64_MAX) {
        return {status_code::invalid_argument, "invalid submission identity, capacity or timing"};
    }
    config_ = _config;
    configured_ = true;
    return {};
}

status submission_window::vqec_vision_ai_core_subwn_reserve(std::uint64_t _source_epoch,
                                                            std::uint64_t _source_pts_ns,
                                                            std::uint64_t _steady_now_ns,
                                                            submission_ticket& _ticket) {
    if (!vqec_vision_ai_core_subwn_is_accepting()) {
        return {status_code::invalid_state, "submission window is not accepting"};
    }
    if (_source_epoch != config_.source_epoch_ || _source_pts_ns == UINT64_MAX ||
        (has_timestamp_ && _source_pts_ns <= last_source_pts_ns_)) {
        return {status_code::invalid_argument, "stale epoch or unavailable/non-increasing PTS"};
    }
    const auto delta = has_timestamp_ ? _source_pts_ns - first_source_pts_ns_ : 0;
    if (delta >= UINT64_MAX - config_.pipeline_anchor_ns_ ||
        _steady_now_ns >= UINT64_MAX - config_.job_timeout_ns_ || next_job_id_ == UINT64_MAX) {
        return {status_code::invalid_argument, "submission timestamp or identity overflow"};
    }
    slot* available = nullptr;
    for (unsigned index = 0; index < config_.capacity_; ++index) {
        if (slots_[index].job_id_ == 0) {
            available = &slots_[index];
            break;
        }
    }
    if (available == nullptr) {
        return {status_code::resource_exhausted, "submission window full"};
    }
    if (!has_timestamp_) {
        first_source_pts_ns_ = _source_pts_ns;
        has_timestamp_ = true;
    }
    last_source_pts_ns_ = _source_pts_ns;
    available->job_id_ = next_job_id_++;
    available->deadline_ns_ = _steady_now_ns + config_.job_timeout_ns_;
    _ticket = {{config_.cycle_id_, available->job_id_},
               _source_pts_ns,
               config_.pipeline_anchor_ns_ + delta};
    return {};
}

submission_window::slot*
submission_window::vqec_vision_ai_core_subwn_find_slot(submission_token _token) noexcept {
    if (!configured_ || _token.cycle_id_ != config_.cycle_id_ || _token.job_id_ == 0) {
        return nullptr;
    }
    for (auto& entry : slots_) {
        if (entry.job_id_ == _token.job_id_) {
            return &entry;
        }
    }
    return nullptr;
}

status submission_window::vqec_vision_ai_core_subwn_commit(submission_token _token) {
    auto* entry = vqec_vision_ai_core_subwn_find_slot(_token);
    if (entry == nullptr || entry->committed_ || !vqec_vision_ai_core_subwn_is_accepting()) {
        return {status_code::invalid_state, "job cannot be committed"};
    }
    entry->committed_ = true;
    return {};
}

status submission_window::vqec_vision_ai_core_subwn_cancel_reserved(submission_token _token) {
    auto* entry = vqec_vision_ai_core_subwn_find_slot(_token);
    if (entry == nullptr || entry->committed_) {
        return {status_code::invalid_state, "only an unsubmitted reservation can be cancelled"};
    }
    *entry = {};
    return {};
}

status submission_window::vqec_vision_ai_core_subwn_complete(submission_token _token, bool _input) {
    auto* entry = vqec_vision_ai_core_subwn_find_slot(_token);
    if (entry == nullptr || !entry->committed_) {
        return {status_code::invalid_state, "completion does not match a committed job"};
    }
    auto& completed = _input ? entry->input_done_ : entry->result_done_;
    if (completed) {
        return {status_code::invalid_state, "duplicate completion"};
    }
    completed = true;
    if (entry->input_done_ && entry->result_done_) {
        *entry = {};
    }
    return {};
}

status submission_window::vqec_vision_ai_core_subwn_complete_input(submission_token _token) {
    return vqec_vision_ai_core_subwn_complete(_token, true);
}

status submission_window::vqec_vision_ai_core_subwn_complete_result(submission_token _token) {
    return vqec_vision_ai_core_subwn_complete(_token, false);
}

status submission_window::vqec_vision_ai_core_subwn_check_deadlines(std::uint64_t _steady_now_ns) {
    if (!configured_) {
        return {status_code::invalid_state, "submission window is not configured"};
    }
    for (const auto& entry : slots_) {
        if (entry.job_id_ != 0 && _steady_now_ns >= entry.deadline_ns_) {
            faulted_ = true;
            return {status_code::timeout, "job deadline elapsed; resources must remain owned"};
        }
    }
    return faulted_ ? status{status_code::invalid_state, "submission window remains faulted"}
                    : status{};
}

void submission_window::vqec_vision_ai_core_subwn_begin_drain() noexcept {
    draining_ = true;
}

void submission_window::vqec_vision_ai_core_subwn_mark_fault() noexcept {
    faulted_ = true;
}

unsigned submission_window::vqec_vision_ai_core_subwn_get_outstanding() const noexcept {
    unsigned count = 0;
    for (const auto& entry : slots_) {
        count += entry.job_id_ != 0 ? 1U : 0U;
    }
    return count;
}

bool submission_window::vqec_vision_ai_core_subwn_is_accepting() const noexcept {
    return configured_ && !draining_ && !faulted_;
}

} // namespace vqec::vision::ai
