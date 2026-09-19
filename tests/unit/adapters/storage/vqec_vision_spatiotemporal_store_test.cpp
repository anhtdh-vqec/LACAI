#include "vqec_vision_spatiotemporal_store.hpp"

#include <sqlite3.h>

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace {

using namespace vqec::vision::ai;

constexpr std::uint64_t g_fixture_shard_duration_ns = 1000000U;
constexpr std::uint64_t g_fixture_maximum_shard_bytes = 1048576U;
constexpr std::uint64_t g_fixture_maximum_store_bytes = 16777216U;
constexpr std::uint64_t g_fixture_reserve_bytes = 4096U;
constexpr std::size_t g_fixture_maximum_encoded_bytes = 65536U;
constexpr std::size_t g_fixture_maximum_results = 32U;
constexpr std::uint32_t g_fixture_busy_timeout_ms = 1000U;
constexpr std::uint32_t g_fixture_checkpoint_pages = 32U;

std::uint64_t vqec_vision_ai_unit_ststst_get_deadline_ns() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count()) + 1000000000U;
}

spatiotemporal_frame_locator vqec_vision_ai_unit_ststst_make_locator(
    const std::string& _source_id, std::uint64_t _frame_id, std::uint64_t _pts_ns) {
    spatiotemporal_frame_locator locator;
    locator.device_id_ = "device.fixture";
    locator.source_id_ = _source_id;
    locator.boot_id_ = "boot.fixture";
    locator.source_epoch_ = 1U;
    locator.frame_id_ = _frame_id;
    locator.source_pts_ns_ = _pts_ns;
    locator.has_capture_utc_ = true;
    locator.capture_utc_ns_ = static_cast<std::int64_t>(_pts_ns);
    locator.clock_uncertainty_ns_ = 100U;
    locator.clock_mapping_revision_ = "clock.v1";
    locator.scene_revision_ = "scene.v1";
    locator.coordinate_revision_ = "coordinate.v1";
    locator.model_revision_ = "model.v1";
    locator.tracker_revision_ = "tracker.v1";
    return locator;
}

trajectory_chunk vqec_vision_ai_unit_ststst_make_chunk(
    const std::string& _chunk_id, const std::string& _source_id,
    const std::string& _subject_ref, std::uint64_t _track_id,
    std::uint64_t _first_pts_ns, std::int32_t _first_x, std::int32_t _last_x,
    bool _has_gap, trajectory_resolution _resolution) {
    trajectory_chunk chunk;
    chunk.chunk_id_ = _chunk_id;
    chunk.track_.device_id_ = "device.fixture";
    chunk.track_.source_id_ = _source_id;
    chunk.track_.boot_id_ = "boot.fixture";
    chunk.track_.source_epoch_ = 1U;
    chunk.track_.local_track_id_ = _track_id;
    chunk.subject_ref_ = _subject_ref;
    chunk.entity_category_ = "person";
    chunk.chunk_sequence_ = 1U;
    chunk.first_frame_ =
        vqec_vision_ai_unit_ststst_make_locator(_source_id, 10U, _first_pts_ns);
    chunk.last_frame_ =
        vqec_vision_ai_unit_ststst_make_locator(_source_id, 11U, _first_pts_ns + 1000U);
    chunk.resolution_ = _resolution;
    chunk.sample_mode_ = _resolution == trajectory_resolution::observation_exact
        ? trajectory_sample_mode::exact
        : trajectory_sample_mode::error_bounded;
    chunk.max_spatial_error_units_ =
        _resolution == trajectory_resolution::observation_exact ? 0U : 2U;
    chunk.max_time_error_ns_ =
        _resolution == trajectory_resolution::observation_exact ? 0U : 1000U;
    chunk.required_access_domain_mask_ =
        vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::trajectory);
    chunk.bounds_left_ = std::min(_first_x, _last_x);
    chunk.bounds_top_ = 0;
    chunk.bounds_right_ = std::max(_first_x, _last_x);
    chunk.bounds_bottom_ = 100;
    trajectory_point first;
    first.frame_id_ = 10U;
    first.source_pts_ns_ = _first_pts_ns;
    first.has_capture_utc_ = true;
    first.capture_utc_ns_ = static_cast<std::int64_t>(_first_pts_ns);
    first.anchor_x_ = _first_x;
    first.anchor_y_ = 50;
    first.flags_ = static_cast<std::uint32_t>(trajectory_point_flag::observed) |
        static_cast<std::uint32_t>(trajectory_point_flag::mandatory);
    auto last = first;
    last.frame_id_ = 11U;
    last.source_pts_ns_ = _first_pts_ns + 1000U;
    last.capture_utc_ns_ = static_cast<std::int64_t>(last.source_pts_ns_);
    last.anchor_x_ = _last_x;
    last.flags_ = static_cast<std::uint32_t>(trajectory_point_flag::observed) |
        (_has_gap ? static_cast<std::uint32_t>(trajectory_point_flag::gap_before) : 0U);
    chunk.points_ = {first, last};
    return chunk;
}

spatiotemporal_query vqec_vision_ai_unit_ststst_make_query() {
    spatiotemporal_query query;
    query.request_id_ = "query.fixture";
    query.source_ids_ = {"camera.a", "camera.b"};
    query.begin_ns_ = 1;
    query.end_ns_ = 5000000;
    query.allowed_access_domain_mask_ =
        vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::trajectory);
    query.authorization_revision_ = 1U;
    query.budget_.maximum_scan_bytes_ = 1048576U;
    query.budget_.maximum_result_bytes_ = 1048576U;
    query.budget_.deadline_ns_ = vqec_vision_ai_unit_ststst_get_deadline_ns();
    query.budget_.maximum_results_ = 16U;
    return query;
}

event_episode_revision vqec_vision_ai_unit_ststst_make_fire_episode(
    const std::string& _episode_id, std::uint64_t _revision,
    std::uint64_t _supersedes_revision, std::int64_t _begin_ns,
    const std::string& _hotspot_cell) {
    event_episode_revision episode;
    episode.episode_id_ = _episode_id;
    episode.revision_ = _revision;
    episode.supersedes_revision_ = _supersedes_revision;
    episode.source_id_ = "camera.a";
    episode.semantic_type_ = "fire_smoke";
    episode.scene_revision_ = "scene.v1";
    episode.rule_revision_ = "fire_rule.v1";
    episode.begin_ns_ = _begin_ns;
    episode.end_ns_ = _begin_ns + 10000;
    episode.recorded_ns_ = episode.end_ns_ + static_cast<std::int64_t>(_revision);
    episode.lifecycle_ = _revision == 1U
        ? episode_lifecycle::closed
        : episode_lifecycle::corrected;
    episode.severity_ppm_ = _revision == 1U ? 700000U : 800000U;
    episode.required_access_domain_mask_ =
        vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::object);
    episode.claims_ = {{"hotspot_cell", _hotspot_cell}, {"event_kind", "fire"}};
    episode.evidence_references_ = {"evidence." + _episode_id};
    return episode;
}

aggregate_contribution_revision vqec_vision_ai_unit_ststst_make_fire_contribution(
    const std::string& _contribution_id, const std::string& _episode_id,
    std::uint64_t _revision, std::uint64_t _supersedes_revision,
    const std::string& _hotspot_cell) {
    aggregate_contribution_revision contribution;
    contribution.contribution_id_ = _contribution_id;
    contribution.revision_ = _revision;
    contribution.supersedes_revision_ = _supersedes_revision;
    contribution.episode_id_ = _episode_id;
    contribution.source_id_ = "camera.a";
    contribution.aggregate_definition_id_ = "fire.incident_count";
    contribution.scene_revision_ = "scene.v1";
    contribution.definition_revision_ = "fire_rollup.v1";
    contribution.bucket_begin_ns_ = 0;
    contribution.bucket_end_ns_ = 5000000;
    contribution.recorded_ns_ = 4000000 + static_cast<std::int64_t>(_revision);
    contribution.numerator_microunits_ = _revision == 1U ? 1000000 : 2000000;
    contribution.denominator_microunits_ = 1000000;
    contribution.observed_duration_ns_ = 4000000U;
    contribution.expected_duration_ns_ = 5000000U;
    contribution.required_access_domain_mask_ =
        vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::aggregate);
    contribution.dimensions_ = {{"hotspot_cell", _hotspot_cell}, {"event_kind", "fire"}};
    return contribution;
}

void vqec_vision_ai_unit_ststst_require_ok(const status& _status) {
    if (_status.code_ != status_code::ok) {
        std::fprintf(stderr, "spatiotemporal store failure: %s\n", _status.message_.c_str());
        assert(false);
    }
}

}  // namespace

int main() {
    char directory_template[] = "/tmp/vqec_vision_spatiotemporal_store_XXXXXX";
    const auto* created = mkdtemp(directory_template);
    assert(created != nullptr);
    const std::filesystem::path root(created);

    spatiotemporal_store_config config;
    config.root_directory_ = root.string();
    config.shard_duration_ns_ = g_fixture_shard_duration_ns;
    config.maximum_shard_bytes_ = g_fixture_maximum_shard_bytes;
    config.maximum_store_bytes_ = g_fixture_maximum_store_bytes;
    config.reserve_free_bytes_ = g_fixture_reserve_bytes;
    config.maximum_encoded_chunk_bytes_ = g_fixture_maximum_encoded_bytes;
    config.maximum_query_results_ = g_fixture_maximum_results;
    config.busy_timeout_ms_ = g_fixture_busy_timeout_ms;
    config.wal_autocheckpoint_pages_ = g_fixture_checkpoint_pages;
    config.is_full_sync_ = true;

    sqlite_spatiotemporal_store store(config);
    assert(store.vqec_vision_ai_stor_stsql_open().code_ == status_code::ok);
    const auto exact = vqec_vision_ai_unit_ststst_make_chunk(
        "chunk.a", "camera.a", "person.a", 1U, 100000U, 0, 100, false,
        trajectory_resolution::observation_exact);
    const auto gap = vqec_vision_ai_unit_ststst_make_chunk(
        "chunk.gap", "camera.a", "person.gap", 2U, 200000U, 0, 100, true,
        trajectory_resolution::trajectory_bounded);
    const auto second_camera = vqec_vision_ai_unit_ststst_make_chunk(
        "chunk.b", "camera.b", "person.a", 3U, 1200000U, 100, 200, false,
        trajectory_resolution::trajectory_bounded);
    vqec_vision_ai_unit_ststst_require_ok(
        store.vqec_vision_ai_stor_stsql_ingest_trajectory(exact, {"kafka.metadata"}));
    assert(store.vqec_vision_ai_stor_stsql_ingest_trajectory(gap, {}).code_ == status_code::ok);
    assert(store.vqec_vision_ai_stor_stsql_ingest_trajectory(
               second_camera, {}).code_ == status_code::ok);
    assert(store.vqec_vision_ai_stor_stsql_ingest_trajectory(
               exact, {"kafka.metadata"}).code_ == status_code::ok);

    auto query = vqec_vision_ai_unit_ststst_make_query();
    query.subject_ref_ = "person.a";
    spatiotemporal_query_page page;
    assert(store.vqec_vision_ai_stor_stsql_query(query, page).code_ == status_code::ok);
    assert(page.trajectory_chunks_.size() == 2U);
    assert(page.completeness_ == spatiotemporal_result_completeness::approximate);
    assert(page.trajectory_chunks_[0].points_.front().frame_id_ == 10U);

    auto exact_only = query;
    exact_only.minimum_resolution_ = trajectory_resolution::observation_exact;
    exact_only.budget_.deadline_ns_ = vqec_vision_ai_unit_ststst_get_deadline_ns();
    assert(store.vqec_vision_ai_stor_stsql_query(exact_only, page).code_ == status_code::ok);
    assert(page.trajectory_chunks_.size() == 1U);
    assert(page.completeness_ == spatiotemporal_result_completeness::complete);

    auto spatial = vqec_vision_ai_unit_ststst_make_query();
    spatial.source_ids_ = {"camera.a"};
    spatial.subject_ref_ = "person.gap";
    spatial.has_spatial_bounds_ = true;
    spatial.spatial_relation_ = spatiotemporal_spatial_relation::path_intersects;
    spatial.bounds_left_ = 40;
    spatial.bounds_top_ = 40;
    spatial.bounds_right_ = 60;
    spatial.bounds_bottom_ = 60;
    assert(store.vqec_vision_ai_stor_stsql_query(spatial, page).code_ == status_code::ok);
    assert(page.trajectory_chunks_.empty());

    auto unauthorized = query;
    unauthorized.allowed_access_domain_mask_ =
        vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::object);
    unauthorized.budget_.deadline_ns_ = vqec_vision_ai_unit_ststst_get_deadline_ns();
    assert(store.vqec_vision_ai_stor_stsql_query(unauthorized, page).code_ ==
           status_code::unauthorized);

    track_association_revision association;
    association.association_id_ = "association.a.b";
    association.revision_ = 1U;
    association.entity_id_ = "entity.person.a";
    association.left_chunk_id_ = "chunk.a";
    association.right_chunk_id_ = "chunk.b";
    association.method_revision_ = "reid.v1";
    association.topology_path_ = "camera_a.camera_b";
    association.score_ppm_ = 900000U;
    association.minimum_travel_ns_ = 1000U;
    association.maximum_travel_ns_ = 2000000U;
    association.clock_uncertainty_ns_ = 100U;
    association.recorded_ns_ = 1300000;
    assert(store.vqec_vision_ai_stor_stsql_ingest_association(association).code_ ==
           status_code::ok);
    assert(store.vqec_vision_ai_stor_stsql_ingest_association(association).code_ ==
           status_code::ok);
    auto conflicting_association = association;
    conflicting_association.score_ppm_ = 800000U;
    assert(store.vqec_vision_ai_stor_stsql_ingest_association(
               conflicting_association).code_ == status_code::invalid_argument);
    auto accepted_association = association;
    accepted_association.revision_ = 2U;
    accepted_association.supersedes_revision_ = 1U;
    accepted_association.review_state_ = association_review_state::accepted;
    accepted_association.recorded_ns_ = 1400000;
    assert(store.vqec_vision_ai_stor_stsql_ingest_association(
               accepted_association).code_ == status_code::ok);

    auto entity_query = vqec_vision_ai_unit_ststst_make_query();
    entity_query.collection_ = spatiotemporal_collection::entities;
    entity_query.entity_id_ = "entity.person.a";
    entity_query.allowed_access_domain_mask_ =
        vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::identity);
    assert(store.vqec_vision_ai_stor_stsql_query(entity_query, page).code_ == status_code::ok);
    assert(page.associations_.size() == 1U);
    assert(page.associations_.front().revision_ == 2U);

    auto observed_query = entity_query;
    observed_query.revision_view_ = spatiotemporal_revision_view::as_observed;
    observed_query.budget_.deadline_ns_ = vqec_vision_ai_unit_ststst_get_deadline_ns();
    assert(store.vqec_vision_ai_stor_stsql_query(observed_query, page).code_ == status_code::ok);
    assert(page.associations_.size() == 1U);
    assert(page.associations_.front().revision_ == 1U);

    const auto fire_a_v1 = vqec_vision_ai_unit_ststst_make_fire_episode(
        "fire.a", 1U, 0U, 300000, "grid_3_4");
    auto fire_a_v2 = vqec_vision_ai_unit_ststst_make_fire_episode(
        "fire.a", 2U, 1U, 300000, "grid_3_4");
    const auto fire_b_v1 = vqec_vision_ai_unit_ststst_make_fire_episode(
        "fire.b", 1U, 0U, 600000, "grid_3_4");
    assert(store.vqec_vision_ai_stor_stsql_ingest_episode(
               fire_a_v1, {"kafka.metadata"}).code_ == status_code::ok);
    assert(store.vqec_vision_ai_stor_stsql_ingest_episode(
               fire_a_v2, {"kafka.metadata"}).code_ == status_code::ok);
    assert(store.vqec_vision_ai_stor_stsql_ingest_episode(
               fire_a_v2, {"kafka.metadata"}).code_ == status_code::ok);
    auto conflicting_episode = fire_a_v2;
    conflicting_episode.severity_ppm_ = 900000U;
    assert(store.vqec_vision_ai_stor_stsql_ingest_episode(
               conflicting_episode, {}).code_ == status_code::invalid_argument);
    assert(store.vqec_vision_ai_stor_stsql_ingest_episode(
               fire_b_v1, {}).code_ == status_code::ok);

    auto episode_query = vqec_vision_ai_unit_ststst_make_query();
    episode_query.collection_ = spatiotemporal_collection::episodes;
    episode_query.semantic_type_ = "fire_smoke";
    episode_query.allowed_access_domain_mask_ =
        vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::object);
    assert(store.vqec_vision_ai_stor_stsql_query(episode_query, page).code_ ==
           status_code::ok);
    assert(page.episodes_.size() == 2U);
    assert(page.episodes_.front().revision_ == 2U);
    auto observed_episode_query = episode_query;
    observed_episode_query.revision_view_ = spatiotemporal_revision_view::as_observed;
    observed_episode_query.budget_.deadline_ns_ =
        vqec_vision_ai_unit_ststst_get_deadline_ns();
    assert(store.vqec_vision_ai_stor_stsql_query(observed_episode_query, page).code_ ==
           status_code::ok);
    assert(page.episodes_.size() == 2U);
    assert(page.episodes_.front().revision_ == 1U);

    auto contribution_a = vqec_vision_ai_unit_ststst_make_fire_contribution(
        "fire.contribution.a", "fire.a", 1U, 0U, "grid_3_4");
    auto contribution_b = vqec_vision_ai_unit_ststst_make_fire_contribution(
        "fire.contribution.b", "fire.b", 1U, 0U, "grid_3_4");
    assert(store.vqec_vision_ai_stor_stsql_ingest_aggregate_contribution(
               contribution_a, {"kafka.metadata"}).code_ == status_code::ok);
    assert(store.vqec_vision_ai_stor_stsql_ingest_aggregate_contribution(
               contribution_b, {}).code_ == status_code::ok);
    assert(store.vqec_vision_ai_stor_stsql_ingest_aggregate_contribution(
               contribution_b, {}).code_ == status_code::ok);
    auto contribution_a_v2 = contribution_a;
    contribution_a_v2.revision_ = 2U;
    contribution_a_v2.supersedes_revision_ = 1U;
    contribution_a_v2.recorded_ns_ += 100U;
    contribution_a_v2.numerator_microunits_ = 2000000;
    assert(store.vqec_vision_ai_stor_stsql_ingest_aggregate_contribution(
               contribution_a_v2, {}).code_ == status_code::ok);

    auto aggregate_query = vqec_vision_ai_unit_ststst_make_query();
    aggregate_query.collection_ = spatiotemporal_collection::aggregates;
    aggregate_query.semantic_type_ = "fire.incident_count";
    aggregate_query.allowed_access_domain_mask_ =
        vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::aggregate);
    assert(store.vqec_vision_ai_stor_stsql_query(aggregate_query, page).code_ ==
           status_code::ok);
    assert(page.aggregate_buckets_.size() == 1U);
    assert(page.aggregate_buckets_.front().contribution_count_ == 2U);
    assert(page.aggregate_buckets_.front().numerator_microunits_ == 3000000);

    auto contribution_b_v2 = contribution_b;
    contribution_b_v2.revision_ = 2U;
    contribution_b_v2.supersedes_revision_ = 1U;
    contribution_b_v2.recorded_ns_ += 100U;
    contribution_b_v2.operation_ = aggregate_contribution_operation::retract;
    assert(store.vqec_vision_ai_stor_stsql_ingest_aggregate_contribution(
               contribution_b_v2, {}).code_ == status_code::ok);
    aggregate_query.budget_.deadline_ns_ = vqec_vision_ai_unit_ststst_get_deadline_ns();
    assert(store.vqec_vision_ai_stor_stsql_query(aggregate_query, page).code_ ==
           status_code::ok);
    assert(page.aggregate_buckets_.size() == 1U);
    assert(page.aggregate_buckets_.front().contribution_count_ == 1U);
    assert(page.aggregate_buckets_.front().numerator_microunits_ == 2000000);

    assert(store.vqec_vision_ai_stor_stsql_seal_before(1000000U).code_ == status_code::ok);
    spatiotemporal_store_stats stats;
    assert(store.vqec_vision_ai_stor_stsql_get_stats(stats).code_ == status_code::ok);
    assert(stats.committed_chunks_ == 3U && stats.committed_associations_ == 2U);
    assert(stats.committed_episode_revisions_ == 3U);
    assert(stats.committed_aggregate_revisions_ == 4U);
    assert(stats.sealed_shards_ == 1U);
    assert(store.vqec_vision_ai_stor_stsql_close().code_ == status_code::ok);

    sqlite3* catalog = nullptr;
    assert(sqlite3_open((root / "catalog.db").c_str(), &catalog) == SQLITE_OK);
    assert(sqlite3_exec(catalog, "DELETE FROM chunk_index WHERE chunk_id='chunk.a';",
               nullptr, nullptr, nullptr) == SQLITE_OK);
    assert(sqlite3_close(catalog) == SQLITE_OK);

    sqlite_spatiotemporal_store recovered(config);
    assert(recovered.vqec_vision_ai_stor_stsql_open().code_ == status_code::ok);
    assert(recovered.vqec_vision_ai_stor_stsql_get_stats(stats).code_ == status_code::ok);
    assert(stats.orphan_chunks_recovered_ == 1U);
    query.budget_.deadline_ns_ = vqec_vision_ai_unit_ststst_get_deadline_ns();
    assert(recovered.vqec_vision_ai_stor_stsql_query(query, page).code_ == status_code::ok);
    assert(page.trajectory_chunks_.size() == 2U);
    assert(recovered.vqec_vision_ai_stor_stsql_close().code_ == status_code::ok);

    std::filesystem::remove_all(root);
    return 0;
}
