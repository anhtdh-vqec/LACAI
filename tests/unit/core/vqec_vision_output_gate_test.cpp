#include <iostream>

#include "vqec/vision/ai/contracts/vqec_vision_output_gate.hpp"

int main() {
    using namespace vqec::vision::ai;
    static_assert(output_policy_limits::g_max_identifier_bytes == 128);
    static_assert(output_policy_limits::g_max_attributes_per_scope == 64);
    static_assert(output_policy_limits::g_max_policy_rules == 64);
    static_assert(output_policy_limits::g_max_rendered_scopes == 16);
    unsigned failures = 0;
    const auto check = [&](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    output_gate gate;
    output_authorization request{1, "camera:0", "person_tracking", {"human.clothing"}};
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 0).code_ == status_code::unauthorized);
    output_policy policy{1, 10, 100, {
        {"camera:0", "person_tracking", {"human.clothing"}},
        {"camera:1", "traffic", {"traffic.plate_text"}}}};
    check(gate.vqec_vision_ai_core_otgat_apply_policy(policy, 0).code_ == status_code::ok);
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 9).code_ == status_code::unauthorized);
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 10).code_ == status_code::ok);
    request.attributes_.push_back("human.face_embedding");
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 11).code_ == status_code::unauthorized);
    request = {1, "camera:0", "traffic", {"traffic.plate_text"}};
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 12).code_ == status_code::unauthorized);
    request.source_id_ = "camera:1";
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 13).code_ == status_code::ok);
    request.attributes_ = {"traffic.plate_text", "traffic.plate_text"};
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 14).code_ ==
          status_code::invalid_argument);
    request.attributes_ = {"traffic.plate_text"};
    auto invalid = policy;
    invalid.revision_ = 2;
    invalid.rules_.push_back(invalid.rules_[0]);
    check(gate.vqec_vision_ai_core_otgat_apply_policy(invalid, 1).code_ ==
          status_code::invalid_argument);
    check(gate.vqec_vision_ai_core_otgat_get_revision() == 1);
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 15).code_ == status_code::ok);
    policy.revision_ = 2;
    check(gate.vqec_vision_ai_core_otgat_apply_policy(policy, 0).code_ ==
          status_code::invalid_state);
    check(gate.vqec_vision_ai_core_otgat_apply_policy(policy, 1).code_ == status_code::ok);
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 16).code_ == status_code::unauthorized);
    request.policy_revision_ = 2;
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 99).code_ == status_code::ok);
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 100).code_ ==
          status_code::unauthorized);
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 99).code_ == status_code::unauthorized);
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 100).code_ ==
          status_code::unauthorized);
    check(gate.vqec_vision_ai_core_otgat_apply_policy(policy, 2).code_ ==
          status_code::invalid_state);
    policy.revision_ = 3;
    policy.expires_ns_ = 200;
    check(gate.vqec_vision_ai_core_otgat_apply_policy(policy, 2).code_ == status_code::ok);
    request.policy_revision_ = 3;
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 101).code_ == status_code::ok);
    gate.vqec_vision_ai_core_otgat_invalidate();
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 102).code_ ==
          status_code::unauthorized);
    check(gate.vqec_vision_ai_core_otgat_get_revision() == 3);
    policy.revision_ = 4;
    policy.rules_.clear();
    check(gate.vqec_vision_ai_core_otgat_apply_policy(policy, 3).code_ == status_code::ok);
    request.policy_revision_ = 4;
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 103).code_ ==
          status_code::unauthorized);
    policy.revision_ = 5;
    policy.rules_ = {{"camera:0", "counting", {}}};
    check(gate.vqec_vision_ai_core_otgat_apply_policy(policy, 4).code_ == status_code::ok);
    request = {5, "camera:0", "counting", {}};
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 104).code_ == status_code::ok);
    request.attributes_ = {"human.estimated_age"};
    check(gate.vqec_vision_ai_core_otgat_authorize(request, 105).code_ ==
          status_code::unauthorized);
    policy.revision_ = 6;
    policy.rules_[0].source_id_ = "*";
    check(gate.vqec_vision_ai_core_otgat_apply_policy(policy, 5).code_ ==
          status_code::invalid_argument);
    policy.rules_[0].source_id_ = std::string(129, 'a');
    check(gate.vqec_vision_ai_core_otgat_apply_policy(policy, 5).code_ ==
          status_code::invalid_argument);
    policy.rules_.resize(65);
    check(gate.vqec_vision_ai_core_otgat_apply_policy(policy, 5).code_ ==
          status_code::invalid_argument);
    check(gate.vqec_vision_ai_core_otgat_get_revision() == 5);
    // Independent boundary values catch accidental changes to the shared ceilings.
    output_gate boundary_gate;
    output_policy boundary_policy{1, 0, 100, {}};
    for (unsigned index = 0; index < 64; ++index) {
        boundary_policy.rules_.push_back(
            {std::string(128, 's'), "feature_" + std::to_string(index), {}});
    }
    for (unsigned index = 0; index < 64; ++index) {
        boundary_policy.rules_[0].attributes_.push_back("attribute_" + std::to_string(index));
    }
    check(boundary_gate.vqec_vision_ai_core_otgat_apply_policy(boundary_policy, 0).code_ ==
          status_code::ok);
    output_authorization boundary_request{
        1, std::string(128, 's'), "feature_0", boundary_policy.rules_[0].attributes_};
    check(boundary_gate.vqec_vision_ai_core_otgat_authorize(boundary_request, 1).code_ ==
          status_code::ok);
    boundary_request.attributes_.push_back("attribute_over_limit");
    check(boundary_gate.vqec_vision_ai_core_otgat_authorize(boundary_request, 2).code_ ==
          status_code::invalid_argument);
    boundary_policy.revision_ = 2;
    boundary_policy.rules_[0].attributes_.push_back("attribute_over_limit");
    check(boundary_gate.vqec_vision_ai_core_otgat_apply_policy(boundary_policy, 1).code_ ==
          status_code::invalid_argument);
    check(boundary_gate.vqec_vision_ai_core_otgat_get_revision() == 1);
    boundary_request.attributes_.pop_back();
    check(boundary_gate.vqec_vision_ai_core_otgat_authorize(boundary_request, 3).code_ ==
          status_code::ok);
    std::cout << "output gate failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
