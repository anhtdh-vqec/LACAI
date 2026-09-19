// Device-free tests for the image-alignment contract: template/request validation and
// fail-closed capability gating. No backend or pixel work is involved.

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

#include "vqec/vision/ai/contracts/media/vqec_vision_image_alignment.hpp"

using namespace vqec::vision::ai;

namespace {

alignment_template make_template() {
    alignment_template value;
    value.schema_id_ = "face.5pt";
    value.schema_version_ = "1";
    value.destination_width_ = 112;
    value.destination_height_ = 112;
    value.reference_points_ = {{38.3F, 51.7F}, {73.5F, 51.5F}, {56.0F, 71.7F},
        {41.5F, 92.4F}, {70.7F, 92.2F}};
    return value;
}

alignment_request make_request(const alignment_template& _template) {
    alignment_request value;
    value.frame_.source_epoch_ = 3;
    value.frame_.frame_id_ = 42;
    value.frame_.source_pts_ns_ = 1000;
    value.landmarks_.schema_id_ = _template.schema_id_;
    value.landmarks_.schema_version_ = _template.schema_version_;
    value.landmarks_.points_ = _template.reference_points_;
    return value;
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    const auto good_template = make_template();
    check(vqec_vision_ai_core_imaln_validate_template(good_template).code_ == status_code::ok);

    auto bad = good_template;
    bad.schema_id_.clear();
    check(vqec_vision_ai_core_imaln_validate_template(bad).code_ == status_code::invalid_argument);
    bad = good_template;
    bad.destination_width_ = 4;
    check(vqec_vision_ai_core_imaln_validate_template(bad).code_ == status_code::invalid_argument);
    bad = good_template;
    bad.destination_height_ = 8192;
    check(vqec_vision_ai_core_imaln_validate_template(bad).code_ == status_code::invalid_argument);
    bad = good_template;
    bad.reference_points_.clear();
    check(vqec_vision_ai_core_imaln_validate_template(bad).code_ == status_code::invalid_argument);
    bad = good_template;
    bad.reference_points_[0].x_ = std::numeric_limits<float>::infinity();
    check(vqec_vision_ai_core_imaln_validate_template(bad).code_ == status_code::invalid_argument);

    const auto good_request = make_request(good_template);
    check(vqec_vision_ai_core_imaln_validate_request(good_request, good_template).code_ ==
        status_code::ok);
    auto bad_request = good_request;
    bad_request.landmarks_.schema_version_ = "2";
    check(vqec_vision_ai_core_imaln_validate_request(bad_request, good_template).code_ ==
        status_code::invalid_argument);
    bad_request = good_request;
    bad_request.landmarks_.points_.pop_back();
    check(vqec_vision_ai_core_imaln_validate_request(bad_request, good_template).code_ ==
        status_code::invalid_argument);
    bad_request = good_request;
    bad_request.frame_.source_epoch_ = 0;
    check(vqec_vision_ai_core_imaln_validate_request(bad_request, good_template).code_ ==
        status_code::invalid_argument);
    bad_request = good_request;
    bad_request.deadline_ns_ = std::numeric_limits<std::uint64_t>::max();
    check(vqec_vision_ai_core_imaln_validate_request(bad_request, good_template).code_ ==
        status_code::invalid_argument);

    alignment_capabilities capabilities;
    capabilities.supports_similarity_ = true;
    capabilities.max_points_ = 5;
    capabilities.max_destination_dimension_ = 112;
    check(vqec_vision_ai_core_imaln_require_capability(capabilities, good_template).code_ ==
        status_code::ok);
    auto no_similarity = capabilities;
    no_similarity.supports_similarity_ = false;
    check(vqec_vision_ai_core_imaln_require_capability(no_similarity, good_template).code_ ==
        status_code::unsupported);
    auto few_points = capabilities;
    few_points.max_points_ = 4;
    check(vqec_vision_ai_core_imaln_require_capability(few_points, good_template).code_ ==
        status_code::unsupported);
    auto small_destination = capabilities;
    small_destination.max_destination_dimension_ = 64;
    check(vqec_vision_ai_core_imaln_require_capability(small_destination, good_template).code_ ==
        status_code::unsupported);

    // Similarity transform: identity, translation, and a known scale+rotation recovered
    // from corresponding point sets.
    {
        const std::vector<landmark_point> reference = {
            {0.0F, 0.0F}, {10.0F, 0.0F}, {10.0F, 10.0F}, {0.0F, 10.0F}};
        alignment_transform identity;
        check(vqec_vision_ai_core_imaln_compute_similarity(
                  reference, reference, identity).code_ == status_code::ok);
        check(std::fabs(identity.m00_ - 1.0F) < 1e-5F &&
            std::fabs(identity.m11_ - 1.0F) < 1e-5F &&
            std::fabs(identity.m01_) < 1e-5F && std::fabs(identity.m02_) < 1e-5F);

        std::vector<landmark_point> translated = reference;
        for (auto& point : translated) {
            point.x_ += 7.0F;
            point.y_ -= 3.0F;
        }
        alignment_transform translation;
        check(vqec_vision_ai_core_imaln_compute_similarity(
                  reference, translated, translation).code_ == status_code::ok);
        check(std::fabs(translation.m02_ - 7.0F) < 1e-4F &&
            std::fabs(translation.m12_ + 3.0F) < 1e-4F);

        const float scale = 2.0F;
        const float angle = 0.5235988F;  // 30 degrees
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        std::vector<landmark_point> destination;
        for (const auto& point : reference) {
            destination.push_back({
                scale * (cosine * point.x_ - sine * point.y_) + 5.0F,
                scale * (sine * point.x_ + cosine * point.y_) - 3.0F});
        }
        alignment_transform recovered;
        check(vqec_vision_ai_core_imaln_compute_similarity(
                  reference, destination, recovered).code_ == status_code::ok);
        check(std::fabs(recovered.m00_ - scale * cosine) < 1e-3F &&
            std::fabs(recovered.m01_ + scale * sine) < 1e-3F &&
            std::fabs(recovered.m10_ - scale * sine) < 1e-3F &&
            std::fabs(recovered.m02_ - 5.0F) < 1e-2F &&
            std::fabs(recovered.m12_ + 3.0F) < 1e-2F);

        const std::vector<landmark_point> degenerate = {
            {1.0F, 1.0F}, {1.0F, 1.0F}};
        alignment_transform unused;
        check(vqec_vision_ai_core_imaln_compute_similarity(
                  degenerate, reference, unused).code_ == status_code::invalid_argument);
        check(vqec_vision_ai_core_imaln_compute_similarity(
                  std::vector<landmark_point>{{0.0F, 0.0F}}, reference, unused).code_ ==
              status_code::invalid_argument);
    }

    std::cout << "image alignment failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
