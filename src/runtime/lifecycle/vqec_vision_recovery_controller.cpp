#include "vqec_vision_recovery_controller.hpp"

#include <limits>

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_life_rcvr_checked_mul(
    std::uint64_t _left, std::uint64_t _right, std::uint64_t& _product) noexcept {
    if (_right != 0 && _left > std::numeric_limits<std::uint64_t>::max() / _right) {
        return false;
    }
    _product = _left * _right;
    return true;
}

}  // namespace

status source_recovery_controller::vqec_vision_ai_life_rcvr_configure(
    std::uint16_t _source_count, const recovery_policy& _policy) {
    if (source_count_ != 0) {
        return {status_code::invalid_state, "recovery controller is already configured"};
    }
    if (_source_count == 0 || _source_count > deployment_limits::g_max_sources) {
        return {status_code::invalid_argument, "invalid recovery source count"};
    }
    if (_policy.max_retries_ == 0 ||
        _policy.max_retries_ > recovery_policy_limits::g_max_retries ||
        _policy.initial_backoff_ns_ == 0 ||
        _policy.max_backoff_ns_ < _policy.initial_backoff_ns_ ||
        _policy.max_backoff_ns_ > recovery_policy_limits::g_max_backoff_ns ||
        _policy.multiplier_numerator_ == 0 || _policy.multiplier_denominator_ == 0) {
        return {status_code::invalid_argument, "invalid recovery policy"};
    }
    policy_ = _policy;
    source_count_ = _source_count;
    return {};
}

bool source_recovery_controller::vqec_vision_ai_life_rcvr_is_configured() const noexcept {
    return source_count_ != 0;
}

status source_recovery_controller::vqec_vision_ai_life_rcvr_on_fault(
    std::uint16_t _source_slot, std::uint64_t _now_ns,
    recovery_decision& _decision) {
    if (source_count_ == 0) {
        return {status_code::invalid_state, "recovery controller is not configured"};
    }
    if (_source_slot >= source_count_ ||
        _now_ns == std::numeric_limits<std::uint64_t>::max()) {
        return {status_code::invalid_argument, "invalid recovery fault input"};
    }
    if (exhausted_[_source_slot]) {
        _decision = recovery_decision::exhausted;
        return {};
    }
    if (attempts_[_source_slot] >= policy_.max_retries_) {
        exhausted_[_source_slot] = true;
        _decision = recovery_decision::exhausted;
        return {};
    }
    // Backoff for attempt index n = attempts_ (0-based) is
    // initial * (num/den)^n, capped at max_backoff_ns.
    std::uint64_t backoff = policy_.initial_backoff_ns_;
    for (std::uint32_t step = 0; step < attempts_[_source_slot]; ++step) {
        std::uint64_t scaled = 0;
        if (!vqec_vision_ai_life_rcvr_checked_mul(
                backoff, policy_.multiplier_numerator_, scaled) ||
            scaled / policy_.multiplier_denominator_ > policy_.max_backoff_ns_) {
            backoff = policy_.max_backoff_ns_;
            break;
        }
        backoff = scaled / policy_.multiplier_denominator_;
        if (backoff >= policy_.max_backoff_ns_) {
            backoff = policy_.max_backoff_ns_;
            break;
        }
    }
    ++attempts_[_source_slot];
    if (_now_ns > std::numeric_limits<std::uint64_t>::max() - backoff) {
        next_retry_ns_[_source_slot] = std::numeric_limits<std::uint64_t>::max();
        _decision = recovery_decision::wait;
        return {};
    }
    next_retry_ns_[_source_slot] = _now_ns + backoff;
    _decision = backoff == 0 ? recovery_decision::retry : recovery_decision::wait;
    return {};
}

status source_recovery_controller::vqec_vision_ai_life_rcvr_on_success(
    std::uint16_t _source_slot) {
    if (source_count_ == 0) {
        return {status_code::invalid_state, "recovery controller is not configured"};
    }
    if (_source_slot >= source_count_) {
        return {status_code::invalid_argument, "invalid recovery source slot"};
    }
    attempts_[_source_slot] = 0;
    next_retry_ns_[_source_slot] = 0;
    exhausted_[_source_slot] = false;
    return {};
}

status source_recovery_controller::vqec_vision_ai_life_rcvr_get_snapshot(
    std::uint16_t _source_slot, recovery_snapshot& _snapshot) const {
    if (source_count_ == 0) {
        return {status_code::invalid_state, "recovery controller is not configured"};
    }
    if (_source_slot >= source_count_) {
        return {status_code::invalid_argument, "invalid recovery source slot"};
    }
    recovery_snapshot snapshot;
    snapshot.attempts_ = attempts_[_source_slot];
    snapshot.next_retry_ns_ = next_retry_ns_[_source_slot];
    snapshot.exhausted_ = exhausted_[_source_slot];
    _snapshot = snapshot;
    return {};
}

}  // namespace vqec::vision::ai
