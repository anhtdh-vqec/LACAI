#include "vqec_vision_overlay_preparation.hpp"

#include <utility>

namespace vqec::vision::ai {

status vqec_vision_ai_outpt_ovrpr_prepare_authorized_scopes(
    const observation_batch& _observations,
    const std::vector<output_authorization>& _scopes,
    std::uint64_t _prepared_monotonic_ns, std::uint64_t _max_age_ns,
    output_gate& _gate, prepared_overlay& _prepared) {
    if (_scopes.empty() || _scopes.size() > output_policy_limits::g_max_rendered_scopes ||
        _max_age_ns == 0 || _max_age_ns == UINT64_MAX ||
        _prepared_monotonic_ns == UINT64_MAX) {
        return {status_code::invalid_argument, "invalid overlay scope or freshness budget"};
    }
    const auto revision = _scopes.front().policy_revision_;
    if (revision == 0) {
        return {status_code::invalid_argument, "overlay policy revision is missing"};
    }
    for (const auto& scope : _scopes) {
        if (scope.policy_revision_ != revision) {
            return {status_code::unauthorized, "overlay scopes use different policy revisions"};
        }
        const auto authorized = _gate.vqec_vision_ai_core_otgat_authorize(
            scope, _prepared_monotonic_ns);
        if (authorized.code_ != status_code::ok) {
            return authorized;
        }
    }
    const auto valid_observations = vqec_vision_ai_core_obval_validate_batch(
        _observations, _observations.frame_, _observations.geometry_);
    if (valid_observations.code_ != status_code::ok) {
        return valid_observations;
    }
    overlay_batch candidate;
    candidate.frame_ = _observations.frame_;
    candidate.geometry_ = _observations.geometry_;
    candidate.policy_revision_ = revision;
    candidate.prepared_monotonic_ns_ = _prepared_monotonic_ns;
    candidate.boxes_.reserve(_observations.observations_.size());
    for (const auto& observation : _observations.observations_) {
        auto box = observation.box_;
        if (box.label_.empty()) {
            box.label_ = observation.class_id_;
        }
        candidate.boxes_.push_back(std::move(box));
    }
    const auto valid_overlay = vqec_vision_ai_core_pvctr_validate_overlay(
        candidate, _observations.frame_, _observations.geometry_, revision,
        _prepared_monotonic_ns, _max_age_ns);
    if (valid_overlay.code_ != status_code::ok) {
        return valid_overlay;
    }
    prepared_overlay candidate_prepared;
    candidate_prepared.overlay_ = std::move(candidate);
    candidate_prepared.rendered_scopes_ = _scopes;
    std::swap(_prepared, candidate_prepared);
    return {};
}

status vqec_vision_ai_outpt_ovrpr_prepare_authorized(
    const observation_batch& _observations, const overlay_preparation_context& _context,
    output_gate& _gate, prepared_overlay& _prepared) {
    const output_authorization authorization{
        _context.policy_revision_, _context.source_id_, _context.feature_id_, _context.attributes_};
    return vqec_vision_ai_outpt_ovrpr_prepare_authorized_scopes(
        _observations, {authorization}, _context.prepared_monotonic_ns_, _context.max_age_ns_,
        _gate, _prepared);
}

status vqec_vision_ai_outpt_ovrpr_prepare(
    const observation_batch& _observations, const overlay_preparation_context& _context,
    output_gate& _gate, overlay_batch& _overlay) {
    prepared_overlay prepared;
    const auto result = vqec_vision_ai_outpt_ovrpr_prepare_authorized(
        _observations, _context, _gate, prepared);
    if (result.code_ == status_code::ok) {
        _overlay = std::move(prepared.overlay_);
    }
    return result;
}

}  // namespace vqec::vision::ai
