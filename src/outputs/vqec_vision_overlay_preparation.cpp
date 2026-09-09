#include "vqec_vision_overlay_preparation.hpp"

#include <utility>

namespace vqec::vision::ai {

status vqec_vision_ai_outpt_ovrpr_prepare_authorized(
    const observation_batch& _observations, const overlay_preparation_context& _context,
    output_gate& _gate, prepared_overlay& _prepared) {
    if (_context.max_age_ns_ == 0 || _context.max_age_ns_ == UINT64_MAX) {
        return {status_code::invalid_argument, "overlay freshness budget is invalid"};
    }
    const auto valid_observations = vqec_vision_ai_core_obval_validate_batch(
        _observations, _observations.frame_, _observations.geometry_);
    if (valid_observations.code_ != status_code::ok) {
        return valid_observations;
    }
    const output_authorization authorization{
        _context.policy_revision_, _context.source_id_, _context.feature_id_, _context.attributes_};
    const auto authorized = _gate.vqec_vision_ai_core_otgat_authorize(
        authorization, _context.prepared_monotonic_ns_);
    if (authorized.code_ != status_code::ok) {
        return authorized;
    }
    overlay_batch candidate;
    candidate.frame_ = _observations.frame_;
    candidate.geometry_ = _observations.geometry_;
    candidate.policy_revision_ = _context.policy_revision_;
    candidate.prepared_monotonic_ns_ = _context.prepared_monotonic_ns_;
    candidate.boxes_.reserve(_observations.observations_.size());
    for (const auto& observation : _observations.observations_) {
        auto box = observation.box_;
        if (box.label_.empty()) {
            box.label_ = observation.class_id_;
        }
        candidate.boxes_.push_back(std::move(box));
    }
    const auto valid_overlay = vqec_vision_ai_core_pvctr_validate_overlay(
        candidate, _observations.frame_, _observations.geometry_, _context.policy_revision_,
        _context.prepared_monotonic_ns_, _context.max_age_ns_);
    if (valid_overlay.code_ != status_code::ok) {
        return valid_overlay;
    }
    prepared_overlay candidate_prepared;
    candidate_prepared.overlay_ = std::move(candidate);
    candidate_prepared.rendered_scopes_.push_back(authorization);
    std::swap(_prepared, candidate_prepared);
    return {};
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
