#include <cassert>

#include "vqec_vision_overlay_preparation.hpp"

using namespace vqec::vision::ai;

int main() {
    const preview_frame_key frame{0, 0, 1, 2, 3};
    const preview_geometry geometry{640, 360};
    observation_batch observations{
        frame, geometry,
        {{frame, 1, "person", {10, 20, 30, 40, 0xffffffffU, ""}, 0.8F,
          observation_quality::high, {}}}};
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
    return 0;
}
