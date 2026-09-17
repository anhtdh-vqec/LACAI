#include "vqec_vision_event_delivery_seam.hpp"

#include <algorithm>
#include <new>
#include <utility>

namespace vqec::vision::ai {

event_delivery_seam::event_delivery_seam(
    const event_delivery_seam_config& _config) noexcept
    : config_(_config) {
    if (config_.max_queued_events_ == 0) {
        config_.max_queued_events_ =
            event_delivery_seam_limits::g_default_queue_capacity;
    } else if (config_.max_queued_events_ >
               event_delivery_seam_limits::g_max_queue_capacity) {
        config_.max_queued_events_ =
            event_delivery_seam_limits::g_max_queue_capacity;
    }
}

status event_delivery_seam::vqec_vision_ai_ports_fesnk_deliver_event(
    const feature_event& _event) {
    if (is_stopping_) {
        ++metrics_.events_rejected_;
        return {status_code::invalid_state, "event delivery seam is stopping"};
    }
    if (pending_count_ >= config_.max_queued_events_) {
        ++metrics_.events_dropped_;
        return {status_code::resource_exhausted,
            "event delivery seam queue capacity reached"};
    }
    try {
        auto& slot = queue_[tail_index_];
        slot.event_ = _event;
        slot.enqueued_time_ns_ = _event.occurred_at_ns_;
        slot.disposition_ = event_disposition::accepted_pending;
        tail_index_ =
            (tail_index_ + 1) % event_delivery_seam_limits::g_max_queue_capacity;
        ++pending_count_;
        ++metrics_.events_accepted_;
        metrics_.events_pending_ = pending_count_;
        if (pending_count_ == 1) {
            metrics_.oldest_pending_ns_ = slot.enqueued_time_ns_;
        }
    } catch (const std::bad_alloc&) {
        ++metrics_.events_dropped_;
        return {status_code::resource_exhausted,
            "event delivery seam allocation failed"};
    }
    return {};
}

status event_delivery_seam::vqec_vision_ai_outpt_evdsm_take_next(
    feature_event& _event) {
    if (pending_count_ == 0) {
        return {status_code::pending, "event delivery seam has no pending event"};
    }
    auto& slot = queue_[head_index_];
    _event = std::move(slot.event_);
    slot.enqueued_time_ns_ = 0;
    slot.disposition_ = event_disposition::handed_off;
    head_index_ =
        (head_index_ + 1) % event_delivery_seam_limits::g_max_queue_capacity;
    --pending_count_;
    ++metrics_.events_handed_off_;
    metrics_.events_pending_ = pending_count_;
    if (pending_count_ > 0) {
        metrics_.oldest_pending_ns_ = queue_[head_index_].enqueued_time_ns_;
    } else {
        metrics_.oldest_pending_ns_ = 0;
    }
    return {};
}

void event_delivery_seam::vqec_vision_ai_outpt_evdsm_discard_pending() noexcept {
    metrics_.events_discarded_ += pending_count_;
    while (pending_count_ > 0) {
        auto& slot = queue_[head_index_];
        slot.event_ = {};
        slot.enqueued_time_ns_ = 0;
        slot.disposition_ = event_disposition::rejected;
        head_index_ =
            (head_index_ + 1) % event_delivery_seam_limits::g_max_queue_capacity;
        --pending_count_;
    }
    metrics_.events_pending_ = 0;
    metrics_.oldest_pending_ns_ = 0;
}

void event_delivery_seam::vqec_vision_ai_outpt_evdsm_request_stop() noexcept {
    is_stopping_ = true;
}

bool event_delivery_seam::vqec_vision_ai_outpt_evdsm_is_stopping() const noexcept {
    return is_stopping_;
}

const event_delivery_seam_metrics&
event_delivery_seam::vqec_vision_ai_outpt_evdsm_get_metrics() const noexcept {
    return metrics_;
}

std::size_t
event_delivery_seam::vqec_vision_ai_outpt_evdsm_get_pending_count() const noexcept {
    return pending_count_;
}

const feature_event*
event_delivery_seam::vqec_vision_ai_outpt_evdsm_peek(std::size_t _index) const noexcept {
    if (_index >= pending_count_) {
        return nullptr;
    }
    const std::size_t slot_idx =
        (head_index_ + _index) % event_delivery_seam_limits::g_max_queue_capacity;
    return &queue_[slot_idx].event_;
}

}  // namespace vqec::vision::ai
