#include <cassert>

#include "vqec_vision_overlay_preparation.hpp"

using namespace vqec::vision::ai;

int main() {
    const preview_frame_key frame{0, 0, 1, 2, 3};
    const preview_geometry geometry{640, 360};
    observation_batch observations{
        frame, geometry,
        {{frame, 1, "person", {10, 20, 30, 40, 0xffffffffU, ""}, 0.8F,
          observation_quality::high, {}, {}}}};
    output_gate gate;
    output_policy policy;
    policy.revision_ = 1;
    policy.not_before_ns_ = 10;
    policy.expires_ns_ = 100;
    policy.rules_.push_back({"source0", "detect", {"bbox"}});
    assert(gate.vqec_vision_ai_core_otgat_apply_policy(policy, 0).code_ == status_code::ok);
    overlay_preparation_context context{"source0", "detect", 1, 20, 100, {"bbox"}};
    overlay_batch overlay;
    assert(vqec_vision_ai_outpt_ovrpr_prepare(observations, context, gate, overlay).code_ ==
           status_code::ok);
    assert(overlay.boxes_.size() == 1U);
    assert(overlay.boxes_[0].label_ == "person");
    prepared_overlay prepared;
    assert(vqec_vision_ai_outpt_ovrpr_prepare_authorized(
               observations, context, gate, prepared).code_ == status_code::ok);
    assert(prepared.overlay_.boxes_.size() == 1U);
    assert(prepared.rendered_scopes_.size() == 1U);
    assert(prepared.rendered_scopes_[0].source_id_ == "source0");
    assert(prepared.rendered_scopes_[0].attributes_.size() == 1U);
    const auto prepared_label = prepared.overlay_.boxes_[0].label_;
    policy.rules_.push_back({"source0", "count", {"total"}});
    policy.revision_ = 2;
    assert(gate.vqec_vision_ai_core_otgat_apply_policy(policy, 1).code_ == status_code::ok);
    const std::vector<output_authorization> scopes{
        {2, "source0", "detect", {"bbox"}},
        {2, "source0", "count", {"total"}}};
    assert(vqec_vision_ai_outpt_ovrpr_prepare_authorized_scopes(
               observations, scopes, 20, 100, gate, prepared).code_ == status_code::ok);
    assert(prepared.rendered_scopes_.size() == 2U);

    const auto previous = overlay;
    context.attributes_ = {"identity"};
    assert(vqec_vision_ai_outpt_ovrpr_prepare(observations, context, gate, overlay).code_ ==
           status_code::unauthorized);
    assert(overlay.boxes_.size() == previous.boxes_.size());
    assert(overlay.boxes_[0].label_ == previous.boxes_[0].label_);
    assert(prepared.overlay_.boxes_[0].label_ == prepared_label);
    context.max_age_ns_ = 0;
    assert(vqec_vision_ai_outpt_ovrpr_prepare(observations, context, gate, overlay).code_ ==
           status_code::invalid_argument);

    // Regression: TTL is recorded and enforced
    assert(prepared.overlay_.ttl_ns_ == 100);
    assert(vqec_vision_ai_core_pvctr_validate_overlay(
               prepared.overlay_, frame, geometry, 2, 20 + 101, 100).code_ ==
           status_code::timeout);

    // Regression: Frame PTS correlation
    preview_frame_key mismatched_frame = frame;
    mismatched_frame.source_pts_ns_ = 1000;
    assert(vqec_vision_ai_core_pvctr_validate_overlay(
               prepared.overlay_, mismatched_frame, geometry, 2, 25, 100).code_ ==
           status_code::invalid_state);

    // Regression: Session epoch correlation
    preview_frame_key mismatched_epoch = frame;
    mismatched_epoch.source_epoch_ = 99;
    assert(vqec_vision_ai_core_pvctr_validate_overlay(
               prepared.overlay_, mismatched_epoch, geometry, 2, 25, 100).code_ ==
           status_code::invalid_state);

    // Regression: Out-of-bounds bounding boxes rejected
    context.policy_revision_ = 2;
    context.max_age_ns_ = 100;
    context.attributes_ = {"bbox"};
    observation_batch oob_observations = observations;
    oob_observations.observations_[0].box_.x_ = 1000.0F;
    assert(vqec_vision_ai_outpt_ovrpr_prepare(oob_observations, context, gate, overlay).code_ ==
           status_code::invalid_argument);

    // Regression: Demand revocation / empty scopes rejected fail-closed
    prepared_overlay empty_demand_prep;
    assert(vqec_vision_ai_outpt_ovrpr_prepare_authorized_scopes(
               observations, {}, 20, 100, gate, empty_demand_prep).code_ ==
           status_code::invalid_argument);

    // Regression: Mixed policy revisions in scopes rejected
    const std::vector<output_authorization> mixed_rev_scopes{
        {2, "source0", "detect", {"bbox"}},
        {1, "source0", "count", {"total"}}};
    assert(vqec_vision_ai_outpt_ovrpr_prepare_authorized_scopes(
               observations, mixed_rev_scopes, 20, 100, gate, empty_demand_prep).code_ ==
           status_code::unauthorized);

    // Regression: Complete policy revocation denying subsequent output
    output_policy rev3_policy;
    rev3_policy.revision_ = 3;
    rev3_policy.not_before_ns_ = 10;
    rev3_policy.expires_ns_ = 200;
    // rev3 explicitly revokes source0
    rev3_policy.rules_.push_back({"source1", "detect", {"bbox"}});
    assert(gate.vqec_vision_ai_core_otgat_apply_policy(rev3_policy, 2).code_ == status_code::ok);

    // Calling with stale revision 2 returns unauthorized
    context.policy_revision_ = 2;
    assert(vqec_vision_ai_outpt_ovrpr_prepare(observations, context, gate, overlay).code_ ==
           status_code::unauthorized);

    // Calling with revision 3 for revoked source0 returns unauthorized
    context.policy_revision_ = 3;
    context.source_id_ = "source0";
    assert(vqec_vision_ai_outpt_ovrpr_prepare(observations, context, gate, overlay).code_ ==
           status_code::unauthorized);

    // Regression: Identity field mismatches (camera, channel, frame_id, geometry)
    preview_frame_key mismatched_cam = frame;
    mismatched_cam.camera_id_ = 42;
    assert(vqec_vision_ai_core_pvctr_validate_overlay(
               prepared.overlay_, mismatched_cam, geometry, 2, 25, 100).code_ ==
           status_code::invalid_state);

    preview_frame_key mismatched_chan = frame;
    mismatched_chan.channel_id_ = 7;
    assert(vqec_vision_ai_core_pvctr_validate_overlay(
               prepared.overlay_, mismatched_chan, geometry, 2, 25, 100).code_ ==
           status_code::invalid_state);

    preview_frame_key mismatched_fid = frame;
    mismatched_fid.frame_id_ = 999;
    assert(vqec_vision_ai_core_pvctr_validate_overlay(
               prepared.overlay_, mismatched_fid, geometry, 2, 25, 100).code_ ==
           status_code::invalid_state);

    preview_geometry mismatched_geom{1920, 1080};
    assert(vqec_vision_ai_core_pvctr_validate_overlay(
               prepared.overlay_, frame, mismatched_geom, 2, 25, 100).code_ ==
           status_code::invalid_state);

    // Odd geometry in overlay batch violates NV12 requirement
    auto odd_overlay = prepared.overlay_;
    odd_overlay.geometry_ = {641, 360};
    preview_geometry odd_geom{641, 360};
    assert(vqec_vision_ai_core_pvctr_validate_overlay(
               odd_overlay, frame, odd_geom, 2, 25, 100).code_ ==
           status_code::invalid_argument);

    // Time travelling monotonic clock (now < prepared)
    assert(vqec_vision_ai_core_pvctr_validate_overlay(
               prepared.overlay_, frame, geometry, 2, 10, 100).code_ ==
           status_code::invalid_argument);

    return 0;
}
