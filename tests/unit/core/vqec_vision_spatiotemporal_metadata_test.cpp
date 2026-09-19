#include "vqec/vision/ai/contracts/output/vqec_vision_spatiotemporal_metadata.hpp"

#include <cassert>

namespace {

using namespace vqec::vision::ai;

constexpr const char* g_fixture_device_id = "device.fixture";
constexpr const char* g_fixture_source_id = "camera.front_gate";
constexpr const char* g_fixture_boot_id = "boot.fixture";

spatiotemporal_frame_locator vqec_vision_ai_unit_smtst_make_locator(
    std::uint64_t _frame_id, std::uint64_t _source_pts_ns) {
    spatiotemporal_frame_locator locator;
    locator.device_id_ = g_fixture_device_id;
    locator.source_id_ = g_fixture_source_id;
    locator.boot_id_ = g_fixture_boot_id;
    locator.source_epoch_ = 1U;
    locator.frame_id_ = _frame_id;
    locator.source_pts_ns_ = _source_pts_ns;
    locator.has_capture_utc_ = true;
    locator.capture_utc_ns_ = static_cast<std::int64_t>(_source_pts_ns);
    locator.clock_uncertainty_ns_ = 100U;
    locator.clock_mapping_revision_ = "clock.v1";
    locator.scene_revision_ = "scene.v1";
    locator.coordinate_revision_ = "coordinates.v1";
    locator.model_revision_ = "model.v1";
    locator.tracker_revision_ = "tracker.v1";
    return locator;
}

trajectory_chunk vqec_vision_ai_unit_smtst_make_chunk() {
    trajectory_chunk chunk;
    chunk.chunk_id_ = "chunk.1";
    chunk.track_.device_id_ = g_fixture_device_id;
    chunk.track_.source_id_ = g_fixture_source_id;
    chunk.track_.boot_id_ = g_fixture_boot_id;
    chunk.track_.source_epoch_ = 1U;
    chunk.track_.local_track_id_ = 7U;
    chunk.subject_ref_ = "person.7";
    chunk.entity_category_ = "person";
    chunk.chunk_sequence_ = 1U;
    chunk.first_frame_ = vqec_vision_ai_unit_smtst_make_locator(10U, 1000U);
    chunk.last_frame_ = vqec_vision_ai_unit_smtst_make_locator(12U, 3000U);
    chunk.bounds_left_ = 0;
    chunk.bounds_top_ = 0;
    chunk.bounds_right_ = 1920;
    chunk.bounds_bottom_ = 1080;
    chunk.required_access_domain_mask_ =
        vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::trajectory);
    trajectory_point first;
    first.frame_id_ = 10U;
    first.source_pts_ns_ = 1000U;
    first.has_capture_utc_ = true;
    first.capture_utc_ns_ = 1000;
    first.anchor_x_ = 100;
    first.anchor_y_ = 200;
    first.box_left_ = 80;
    first.box_top_ = 100;
    first.box_right_ = 120;
    first.box_bottom_ = 200;
    first.flags_ = static_cast<std::uint32_t>(trajectory_point_flag::observed) |
        static_cast<std::uint32_t>(trajectory_point_flag::mandatory) |
        static_cast<std::uint32_t>(trajectory_point_flag::has_box);
    auto second = first;
    second.frame_id_ = 11U;
    second.source_pts_ns_ = 2000U;
    second.capture_utc_ns_ = 2000;
    second.anchor_x_ = 105;
    second.anchor_y_ = 205;
    second.flags_ = static_cast<std::uint32_t>(trajectory_point_flag::observed);
    auto third = second;
    third.frame_id_ = 12U;
    third.source_pts_ns_ = 3000U;
    third.capture_utc_ns_ = 3000;
    third.anchor_x_ = 110;
    third.anchor_y_ = 210;
    third.flags_ = static_cast<std::uint32_t>(trajectory_point_flag::observed) |
        static_cast<std::uint32_t>(trajectory_point_flag::mandatory) |
        static_cast<std::uint32_t>(trajectory_point_flag::lifecycle_boundary);
    chunk.points_ = {first, second, third};
    return chunk;
}

}  // namespace

int main() {
    auto chunk = vqec_vision_ai_unit_smtst_make_chunk();
    assert(vqec_vision_ai_cntr_stmet_validate_trajectory_chunk(chunk).code_ == status_code::ok);

    auto fabricated_endpoint = chunk;
    fabricated_endpoint.last_frame_.frame_id_ = 13U;
    assert(vqec_vision_ai_cntr_stmet_validate_trajectory_chunk(fabricated_endpoint).code_ ==
           status_code::invalid_argument);

    auto scene_crossing = chunk;
    scene_crossing.last_frame_.scene_revision_ = "scene.v2";
    assert(vqec_vision_ai_cntr_stmet_validate_trajectory_chunk(scene_crossing).code_ ==
           status_code::invalid_argument);

    auto ambiguous_point = chunk;
    ambiguous_point.points_[1].flags_ = 0U;
    assert(vqec_vision_ai_cntr_stmet_validate_trajectory_chunk(ambiguous_point).code_ ==
           status_code::invalid_argument);

    track_association_revision association;
    association.association_id_ = "association.1";
    association.revision_ = 1U;
    association.entity_id_ = "entity.1";
    association.left_chunk_id_ = "chunk.1";
    association.right_chunk_id_ = "chunk.2";
    association.method_revision_ = "reid.v1";
    association.topology_path_ = "gate_a.gate_b";
    association.score_ppm_ = 800000U;
    association.minimum_travel_ns_ = 1000U;
    association.maximum_travel_ns_ = 10000U;
    association.review_state_ = association_review_state::candidate;
    association.recorded_ns_ = 20000;
    assert(vqec_vision_ai_cntr_stmet_validate_association_revision(association).code_ ==
           status_code::ok);
    association.right_chunk_id_ = association.left_chunk_id_;
    assert(vqec_vision_ai_cntr_stmet_validate_association_revision(association).code_ ==
           status_code::invalid_argument);

    event_episode_revision episode;
    episode.episode_id_ = "episode.fire.1";
    episode.revision_ = 1U;
    episode.source_id_ = g_fixture_source_id;
    episode.semantic_type_ = "fire_smoke";
    episode.scene_revision_ = "scene.v1";
    episode.rule_revision_ = "fire_rule.v1";
    episode.begin_ns_ = 1000;
    episode.end_ns_ = 10000;
    episode.recorded_ns_ = 11000;
    episode.lifecycle_ = episode_lifecycle::closed;
    episode.severity_ppm_ = 700000U;
    episode.required_access_domain_mask_ =
        vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::object);
    episode.claims_ = {{"hotspot_cell", "grid_3_4"}, {"event_kind", "fire"}};
    episode.evidence_references_ = {"evidence.fire.1"};
    assert(vqec_vision_ai_cntr_stmet_validate_episode_revision(episode).code_ ==
           status_code::ok);
    auto duplicate_claim = episode;
    duplicate_claim.claims_.push_back({"hotspot_cell", "grid_4_4"});
    assert(vqec_vision_ai_cntr_stmet_validate_episode_revision(duplicate_claim).code_ ==
           status_code::invalid_argument);

    aggregate_contribution_revision contribution;
    contribution.contribution_id_ = "contribution.fire.1";
    contribution.revision_ = 1U;
    contribution.episode_id_ = episode.episode_id_;
    contribution.source_id_ = g_fixture_source_id;
    contribution.aggregate_definition_id_ = "fire.incident_count";
    contribution.scene_revision_ = "scene.v1";
    contribution.definition_revision_ = "fire_rollup.v1";
    contribution.bucket_begin_ns_ = 0;
    contribution.bucket_end_ns_ = 100000;
    contribution.recorded_ns_ = 11000;
    contribution.numerator_microunits_ = 1000000;
    contribution.denominator_microunits_ = 1000000;
    contribution.observed_duration_ns_ = 9000U;
    contribution.expected_duration_ns_ = 10000U;
    contribution.required_access_domain_mask_ =
        vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::aggregate);
    contribution.dimensions_ = {{"hotspot_cell", "grid_3_4"}};
    assert(vqec_vision_ai_cntr_stmet_validate_aggregate_contribution(contribution).code_ ==
           status_code::ok);
    auto invalid_retraction = contribution;
    invalid_retraction.operation_ = aggregate_contribution_operation::retract;
    assert(vqec_vision_ai_cntr_stmet_validate_aggregate_contribution(
               invalid_retraction).code_ == status_code::invalid_argument);

    spatiotemporal_query query;
    query.request_id_ = "request.1";
    query.source_ids_ = {g_fixture_source_id};
    query.begin_ns_ = 1;
    query.end_ns_ = 100000;
    query.allowed_access_domain_mask_ = 1U;
    query.authorization_revision_ = 1U;
    query.budget_.maximum_scan_bytes_ = 1024U;
    query.budget_.maximum_result_bytes_ = 1024U;
    query.budget_.deadline_ns_ = 1000000U;
    query.budget_.maximum_results_ = 10U;
    assert(vqec_vision_ai_cntr_stmet_validate_query(query).code_ == status_code::ok);
    query.spatial_relation_ = spatiotemporal_spatial_relation::path_intersects;
    assert(vqec_vision_ai_cntr_stmet_validate_query(query).code_ ==
           status_code::invalid_argument);
    return 0;
}
