#include "vqec/vision/ai/contracts/vqec_vision_output_gate.hpp"

#include <algorithm>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_core_otgat_is_identifier(const std::string& _value) noexcept {
    return vqec_vision_ai_cntr_ident_is_valid(
        _value, output_policy_limits::g_max_identifier_bytes);
}

bool vqec_vision_ai_core_otgat_are_attributes_valid(const std::vector<std::string>& _attributes) {
    if (_attributes.size() > output_policy_limits::g_max_attributes_per_scope) {
        return false;
    }
    for (std::size_t index = 0; index < _attributes.size(); ++index) {
        if (!vqec_vision_ai_core_otgat_is_identifier(_attributes[index])) {
            return false;
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (_attributes[index] == _attributes[previous]) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

status output_gate::vqec_vision_ai_core_otgat_apply_policy(
    const output_policy& _policy, std::uint64_t _expected_revision) {
    if (_expected_revision != policy_.revision_ || _policy.revision_ <= policy_.revision_) {
        return {status_code::invalid_state, "stale entitlement update or revision conflict"};
    }
    if (_policy.revision_ == UINT64_MAX || _policy.not_before_ns_ >= _policy.expires_ns_ ||
        _policy.expires_ns_ == UINT64_MAX ||
        _policy.rules_.size() > output_policy_limits::g_max_policy_rules) {
        return {status_code::invalid_argument, "invalid output policy limits or validity interval"};
    }
    for (std::size_t index = 0; index < _policy.rules_.size(); ++index) {
        const auto& rule = _policy.rules_[index];
        if (!vqec_vision_ai_core_otgat_is_identifier(rule.source_id_) ||
            !vqec_vision_ai_core_otgat_is_identifier(rule.feature_id_) ||
            !vqec_vision_ai_core_otgat_are_attributes_valid(rule.attributes_)) {
            return {status_code::invalid_argument, "invalid source/feature/attribute scope"};
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            const auto& other = _policy.rules_[previous];
            if (rule.source_id_ == other.source_id_ && rule.feature_id_ == other.feature_id_) {
                return {status_code::invalid_argument, "duplicate source/feature policy pair"};
            }
        }
    }
    auto candidate = _policy;
    policy_ = std::move(candidate);
    is_active_ = true;
    return {};
}

status output_gate::vqec_vision_ai_core_otgat_authorize(
    const output_authorization& _request, std::uint64_t _steady_now_ns) {
    if (_steady_now_ns == UINT64_MAX || _steady_now_ns < last_now_ns_) {
        is_active_ = false;
        return {status_code::unauthorized, "invalid/backward clock invalidated output policy"};
    }
    last_now_ns_ = _steady_now_ns;
    if (!is_active_ || _request.policy_revision_ != policy_.revision_ ||
        _steady_now_ns < policy_.not_before_ns_ || _steady_now_ns >= policy_.expires_ns_) {
        return {status_code::unauthorized, "output policy missing, stale or outside validity"};
    }
    if (!vqec_vision_ai_core_otgat_is_identifier(_request.source_id_) ||
        !vqec_vision_ai_core_otgat_is_identifier(_request.feature_id_) ||
        !vqec_vision_ai_core_otgat_are_attributes_valid(_request.attributes_)) {
        return {status_code::invalid_argument, "invalid output authorization scope"};
    }
    for (const auto& rule : policy_.rules_) {
        if (rule.source_id_ != _request.source_id_ || rule.feature_id_ != _request.feature_id_) {
            continue;
        }
        for (const auto& attribute : _request.attributes_) {
            if (std::find(rule.attributes_.begin(), rule.attributes_.end(), attribute) ==
                rule.attributes_.end()) {
                return {status_code::unauthorized, "output contains an unlicensed attribute"};
            }
        }
        return {};
    }
    return {status_code::unauthorized, "source/feature pair is not entitled"};
}

void output_gate::vqec_vision_ai_core_otgat_invalidate() noexcept {
    is_active_ = false;
}

std::uint64_t output_gate::vqec_vision_ai_core_otgat_get_revision() const noexcept {
    return policy_.revision_;
}

}  // namespace vqec::vision::ai
