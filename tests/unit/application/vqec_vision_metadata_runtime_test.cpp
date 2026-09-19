#include "vqec_vision_metadata_runtime.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <string>
#include <unistd.h>

namespace {

using namespace vqec::vision::ai;

class metadata_runtime_test_sink final : public feature_event_sink_port {
public:
    status vqec_vision_ai_ports_fesnk_deliver_event(
        const feature_event& _event) override {
        ++deliveries_;
        last_event_id_ = _event.event_id_;
        return {};
    }

    std::uint64_t deliveries_{0};
    std::string last_event_id_;
};

std::uint64_t vqec_vision_ai_unit_mdrut_deadline_ns() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count()) + 1000000000U;
}

std::string vqec_vision_ai_unit_mdrut_profile(const std::string& _root,
    const std::string& _model_id) {
    std::ostringstream profile;
    profile << R"json({
  "schema_version": 1,
  "device_id": "device.fixture",
  "required": true,
  "aggregate_bucket_ns": 1000000,
  "service": {
    "queue_capacity": 32,
    "maximum_live_tracks": 16,
    "maximum_live_deltas": 32,
    "maximum_batch_records": 16,
    "outbox_sinks": [],
    "store": {
      "root_directory": ")json" << _root << R"json(",
      "shard_duration_ns": 1000000,
      "maximum_shard_bytes": 1048576,
      "maximum_store_bytes": 16777216,
      "reserve_free_bytes": 4096,
      "maximum_encoded_chunk_bytes": 65536,
      "maximum_query_results": 32,
      "busy_timeout_ms": 1000,
      "wal_autocheckpoint_pages": 32,
      "full_sync": true
    }
  },
  "trajectory": {
    "maximum_points_per_chunk": 8,
    "maximum_chunk_duration_ns": 1000000,
    "minimum_sample_interval_ns": 0,
    "stale_track_ns": 1000000
  },
  "retention": {
    "maintenance_interval_ns": 1000000,
    "trajectory_ns": 10000000,
    "episode_ns": 10000000,
    "contribution_ns": 10000000,
    "rollup_ns": 10000000
  },
  "sources": [{
    "source_id": "camera.a",
    "scene_revision": "scene.v1",
    "coordinate_revision": "coordinate.v1",
    "clock_mapping_revision": "clock.v1",
    "trajectory_model_id": ")json" << _model_id << R"json(",
    "trajectory_authorization_feature_id": "person_tracking",
    "trajectory_authorization_attribute_id": "trajectory",
    "event_access_rules": [{
      "feature_id": "fire_smoke",
      "access_domains": ["object", "visual_attribute"]
    }]
  }]
})json";
    return profile.str();
}

observation_batch vqec_vision_ai_unit_mdrut_batch(
    std::uint64_t _frame_id, std::uint64_t _pts_ns, float _x) {
    const preview_frame_key frame{1U, 0U, 1U, _frame_id, _pts_ns};
    observation item;
    item.frame_ = frame;
    item.track_id_ = 7U;
    item.class_id_ = "person";
    item.box_ = {_x, 10.0F, 20.0F, 40.0F, 0xffffffffU, "person"};
    item.confidence_ = 0.9F;
    item.quality_ = observation_quality::high;
    return {frame, {640U, 360U}, {item}};
}

spatiotemporal_query vqec_vision_ai_unit_mdrut_query(
    spatiotemporal_collection _collection, std::uint32_t _mask) {
    spatiotemporal_query query;
    query.request_id_ = "query.runtime";
    query.collection_ = _collection;
    query.source_ids_ = {"camera.a"};
    query.begin_ns_ = 1;
    query.end_ns_ = 2000000;
    query.allowed_access_domain_mask_ = _mask;
    query.authorization_revision_ = 1U;
    query.budget_.maximum_scan_bytes_ = 1048576U;
    query.budget_.maximum_result_bytes_ = 1048576U;
    query.budget_.deadline_ns_ = vqec_vision_ai_unit_mdrut_deadline_ns();
    query.budget_.maximum_results_ = 16U;
    return query;
}

}  // namespace

int main() {
    char directory_template[] = "/tmp/vqec_vision_metadata_runtime_XXXXXX";
    const auto* created = mkdtemp(directory_template);
    assert(created != nullptr);
    const std::filesystem::path root(created);

    deployment_config deployment;
    deployment.schema_version_ = 1U;
    deployment.revision_ = 1U;
    source_deployment_config source;
    source.source_id_ = "camera.a";
    source.model_ids_ = {"person_detector"};
    deployment.sources_.push_back(source);

    metadata_runtime_config config;
    auto profile_text = vqec_vision_ai_unit_mdrut_profile(root.string(), "missing_model");
    std::istringstream missing_model(profile_text);
    assert(vqec_vision_ai_appl_mdrun_load_config(
               missing_model, deployment, config).code_ == status_code::unsupported);
    profile_text = vqec_vision_ai_unit_mdrut_profile(root.string(), "person_detector");
    std::istringstream valid_profile(profile_text);
    assert(vqec_vision_ai_appl_mdrun_load_config(
               valid_profile, deployment, config).code_ == status_code::ok);

    metadata_runtime_test_sink sink;
    metadata_runtime runtime(config, sink);
    assert(runtime.vqec_vision_ai_appl_mdrun_start().code_ == status_code::ok);
    const auto first = vqec_vision_ai_unit_mdrut_batch(1U, 100000U, 10.0F);
    const auto second = vqec_vision_ai_unit_mdrut_batch(2U, 200000U, 20.0F);
    assert(runtime.vqec_vision_ai_appl_mdrun_submit_observations(
               "camera.a", "person_detector.1", "tracker.1", first, false).code_ ==
           status_code::unauthorized);
    assert(runtime.vqec_vision_ai_appl_mdrun_submit_observations(
               "camera.a", "person_detector.1", "tracker.1", first, true).code_ ==
           status_code::ok);
    assert(runtime.vqec_vision_ai_appl_mdrun_submit_observations(
               "camera.a", "person_detector.1", "tracker.1", second, true).code_ ==
           status_code::ok);

    feature_event event;
    event.frame_ = second.frame_;
    event.source_id_ = "camera.a";
    event.feature_id_ = "fire_smoke";
    event.event_id_ = "fire.episode.1";
    event.event_schema_id_ = "fire_smoke";
    event.event_schema_version_ = "1";
    event.kind_ = feature_event_kind::snapshot;
    event.occurred_at_ns_ = second.frame_.source_pts_ns_;
    event.config_revision_ = 1U;
    event.track_ids_ = {7U};
    event.fields_ = {{"hotspot_cell", "1", "grid_1_1", 0.9F,
        observation_quality::high}};
    event.episode_revision_ = 1U;
    event.episode_begin_ns_ = event.occurred_at_ns_;
    auto unsupported_event = event;
    unsupported_event.feature_id_ = "not_installed";
    assert(runtime.vqec_vision_ai_ports_fesnk_deliver_event(unsupported_event).code_ ==
           status_code::unsupported);
    assert(sink.deliveries_ == 0U);
    assert(runtime.vqec_vision_ai_ports_fesnk_deliver_event(event).code_ ==
           status_code::ok);
    assert(sink.deliveries_ == 1U && sink.last_event_id_ == event.event_id_);
    assert(runtime.vqec_vision_ai_appl_mdrun_stop(true).code_ == status_code::ok);

    sqlite_spatiotemporal_store store(config.service_.store_);
    assert(store.vqec_vision_ai_stor_stsql_open().code_ == status_code::ok);
    spatiotemporal_query_page page;
    auto query = vqec_vision_ai_unit_mdrut_query(
        spatiotemporal_collection::tracklets,
        vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::trajectory));
    assert(store.vqec_vision_ai_stor_stsql_query(query, page).code_ == status_code::ok);
    assert(page.trajectory_chunks_.size() == 1U);
    assert(page.trajectory_chunks_.front().points_.size() == 2U);
    query = vqec_vision_ai_unit_mdrut_query(
        spatiotemporal_collection::episodes, g_spatiotemporal_all_access_domains);
    assert(store.vqec_vision_ai_stor_stsql_query(query, page).code_ == status_code::ok);
    assert(page.episodes_.size() == 1U &&
           page.episodes_.front().episode_id_ == event.event_id_);
    query = vqec_vision_ai_unit_mdrut_query(
        spatiotemporal_collection::aggregates, g_spatiotemporal_all_access_domains);
    assert(store.vqec_vision_ai_stor_stsql_query(query, page).code_ == status_code::ok);
    assert(page.aggregate_buckets_.size() == 1U &&
           page.aggregate_buckets_.front().contribution_count_ == 1U);
    assert(store.vqec_vision_ai_stor_stsql_close().code_ == status_code::ok);
    std::filesystem::remove_all(root);
    return 0;
}
