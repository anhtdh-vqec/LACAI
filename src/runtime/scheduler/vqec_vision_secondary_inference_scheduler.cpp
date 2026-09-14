#include "vqec_vision_secondary_inference_scheduler.hpp"

#include <limits>
#include <new>
#include <utility>

namespace vqec::vision::ai {

status secondary_inference_scheduler::vqec_vision_ai_sched_secsd_configure(
    std::size_t _capacity, std::uint16_t _source_count,
    secondary_inference_backend& _backend) {
    if (backend_ != nullptr) {
        return {status_code::invalid_state, "secondary scheduler is already configured"};
    }
    if (_capacity == 0 || _capacity > secondary_inference_limits::g_max_queue ||
        _source_count == 0 || _source_count > secondary_inference_limits::g_max_sources) {
        return {status_code::invalid_argument, "invalid secondary scheduler configuration"};
    }
    capacity_ = _capacity;
    source_count_ = _source_count;
    backend_ = &_backend;
    source_epochs_.assign(_source_count, 0);
    return {};
}

bool secondary_inference_scheduler::vqec_vision_ai_sched_secsd_is_configured() const noexcept {
    return backend_ != nullptr;
}

status secondary_inference_scheduler::vqec_vision_ai_sched_secsd_submit(
    const secondary_inference_request& _request) {
    if (backend_ == nullptr) {
        return {status_code::invalid_state, "secondary scheduler is not configured"};
    }
    const auto valid = vqec_vision_ai_core_secin_validate_request(_request);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    if (_request.source_slot_ >= source_count_) {
        return {status_code::invalid_argument, "secondary task source slot is out of range"};
    }
    if (queue_.size() >= capacity_) {
        ++rejected_total_;
        return {status_code::resource_exhausted, "secondary scheduler queue is full"};
    }
    queue_.push_back(_request);
    ++submitted_total_;
    return {};
}

status secondary_inference_scheduler::vqec_vision_ai_sched_secsd_step(
    std::uint64_t _now_ns, secondary_inference_result& _result) {
    if (backend_ == nullptr) {
        return {status_code::invalid_state, "secondary scheduler is not configured"};
    }
    if (_now_ns == std::numeric_limits<std::uint64_t>::max()) {
        return {status_code::invalid_argument, "invalid secondary scheduler time"};
    }
    if (!cancelled_.empty()) {
        _result = std::move(cancelled_.front());
        cancelled_.pop_front();
        return {};
    }
    if (queue_.empty()) {
        return {status_code::pending, "no secondary task is queued"};
    }
    const secondary_inference_request request = queue_.front();
    queue_.pop_front();

    secondary_inference_result result;
    result.request_ = request;
    if (request.deadline_ns_ != 0 && _now_ns > request.deadline_ns_) {
        result.result_ = {status_code::timeout, "secondary task deadline expired"};
        ++expired_total_;
        _result = std::move(result);
        return {};
    }
    status executed;
    try {
        executed = backend_->vqec_vision_ai_cntr_secin_execute(request, result.payload_);
    } catch (const std::bad_alloc&) {
        executed = {status_code::resource_exhausted, "secondary backend allocation failed"};
    } catch (...) {
        executed = {status_code::io_error, "secondary backend raised an exception"};
    }
    result.result_ = std::move(executed);
    if (result.result_.code_ == status_code::ok) {
        ++completed_total_;
    } else {
        ++failed_total_;
    }
    const auto slot = request.source_slot_;
    if (slot < source_epochs_.size() && source_epochs_[slot] != 0 &&
        source_epochs_[slot] != request.source_epoch_) {
        result.stale_epoch_ = true;
        ++stale_total_;
    }
    const auto valid_result = vqec_vision_ai_core_secin_validate_result(result);
    if (valid_result.code_ != status_code::ok) {
        _result = std::move(result);
        return valid_result;
    }
    _result = std::move(result);
    return {};
}

status secondary_inference_scheduler::vqec_vision_ai_sched_secsd_set_source_epoch(
    std::uint16_t _source_slot, std::uint64_t _source_epoch) {
    if (backend_ == nullptr) {
        return {status_code::invalid_state, "secondary scheduler is not configured"};
    }
    if (_source_slot >= source_count_) {
        return {status_code::invalid_argument, "source slot is outside the scheduler range"};
    }
    if (_source_epoch == 0 || _source_epoch < source_epochs_[_source_slot]) {
        return {status_code::invalid_argument, "source epoch must be nonzero and monotonic"};
    }
    source_epochs_[_source_slot] = _source_epoch;
    return {};
}

void secondary_inference_scheduler::vqec_vision_ai_sched_secsd_cancel_all() noexcept {
    while (!queue_.empty()) {
        secondary_inference_result result;
        result.request_ = queue_.front();
        queue_.pop_front();
        result.cancelled_ = true;
        result.result_ = {status_code::pending, "cancelled before execution"};
        if (cancelled_.size() < capacity_) {
            cancelled_.push_back(std::move(result));
        }
        ++cancelled_total_;
    }
}

secondary_inference_snapshot
secondary_inference_scheduler::vqec_vision_ai_sched_secsd_get_snapshot() const {
    secondary_inference_snapshot snapshot;
    snapshot.queue_depth_ = queue_.size();
    snapshot.queue_capacity_ = capacity_;
    snapshot.submitted_total_ = submitted_total_;
    snapshot.completed_total_ = completed_total_;
    snapshot.failed_total_ = failed_total_;
    snapshot.expired_total_ = expired_total_;
    snapshot.rejected_total_ = rejected_total_;
    snapshot.cancelled_total_ = cancelled_total_;
    snapshot.stale_total_ = stale_total_;
    return snapshot;
}

}  // namespace vqec::vision::ai
