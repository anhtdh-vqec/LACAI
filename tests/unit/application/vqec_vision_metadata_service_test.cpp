#include "vqec_vision_metadata_service.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <thread>
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
constexpr std::size_t g_fixture_queue_capacity = 8U;
constexpr std::size_t g_fixture_live_tracks = 1U;
constexpr std::size_t g_fixture_live_deltas = 2U;
constexpr std::size_t g_fixture_batch_records = 4U;

std::uint64_t vqec_vision_ai_unit_mdsvt_get_deadline_ns() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count()) + 1000000000U;
}

trajectory_chunk vqec_vision_ai_unit_mdsvt_make_chunk(
    const std::string& _chunk_id, std::uint64_t _track_id,
    std::uint64_t _first_pts_ns) {
    trajectory_chunk chunk;
    chunk.chunk_id_ = _chunk_id;
    chunk.track_.device_id_ = "device.fixture";
    chunk.track_.source_id_ = "camera.a";
    chunk.track_.boot_id_ = "boot.fixture";
    chunk.track_.source_epoch_ = 1U;
    chunk.track_.local_track_id_ = _track_id;
    chunk.subject_ref_ = "person." + std::to_string(_track_id);
    chunk.entity_category_ = "person";
    chunk.chunk_sequence_ = 1U;
    chunk.first_frame_.device_id_ = chunk.track_.device_id_;
    chunk.first_frame_.source_id_ = chunk.track_.source_id_;
    chunk.first_frame_.boot_id_ = chunk.track_.boot_id_;
    chunk.first_frame_.source_epoch_ = chunk.track_.source_epoch_;
    chunk.first_frame_.frame_id_ = 10U;
    chunk.first_frame_.source_pts_ns_ = _first_pts_ns;
    chunk.first_frame_.clock_mapping_revision_ = "clock.v1";
    chunk.first_frame_.scene_revision_ = "scene.v1";
    chunk.first_frame_.coordinate_revision_ = "coordinate.v1";
    chunk.first_frame_.model_revision_ = "model.v1";
    chunk.first_frame_.tracker_revision_ = "tracker.v1";
    chunk.last_frame_ = chunk.first_frame_;
    chunk.last_frame_.frame_id_ = 11U;
    chunk.last_frame_.source_pts_ns_ = _first_pts_ns + 1000U;
    chunk.required_access_domain_mask_ =
        vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::trajectory);
    chunk.bounds_left_ = 0;
    chunk.bounds_top_ = 0;
    chunk.bounds_right_ = 100;
    chunk.bounds_bottom_ = 100;
    trajectory_point first;
    first.frame_id_ = 10U;
    first.source_pts_ns_ = _first_pts_ns;
    first.anchor_x_ = 10;
    first.anchor_y_ = 10;
    first.flags_ = static_cast<std::uint32_t>(trajectory_point_flag::observed);
    auto last = first;
    last.frame_id_ = 11U;
    last.source_pts_ns_ += 1000U;
    last.anchor_x_ = 20;
    chunk.points_ = {first, last};
    return chunk;
}

metadata_service_config vqec_vision_ai_unit_mdsvt_make_config(
    const std::filesystem::path& _root) {
    metadata_service_config config;
    config.store_.root_directory_ = _root.string();
    config.store_.shard_duration_ns_ = g_fixture_shard_duration_ns;
    config.store_.maximum_shard_bytes_ = g_fixture_maximum_shard_bytes;
    config.store_.maximum_store_bytes_ = g_fixture_maximum_store_bytes;
    config.store_.reserve_free_bytes_ = g_fixture_reserve_bytes;
    config.store_.maximum_encoded_chunk_bytes_ = g_fixture_maximum_encoded_bytes;
    config.store_.maximum_query_results_ = g_fixture_maximum_results;
    config.store_.busy_timeout_ms_ = g_fixture_busy_timeout_ms;
    config.store_.wal_autocheckpoint_pages_ = g_fixture_checkpoint_pages;
    config.queue_capacity_ = g_fixture_queue_capacity;
    config.maximum_live_tracks_ = g_fixture_live_tracks;
    config.maximum_live_deltas_ = g_fixture_live_deltas;
    config.maximum_batch_records_ = g_fixture_batch_records;
    config.outbox_sinks_ = {"kafka.metadata"};
    return config;
}

}  // namespace

int main() {
    char directory_template[] = "/tmp/vqec_vision_metadata_service_XXXXXX";
    const auto* created = mkdtemp(directory_template);
    assert(created != nullptr);
    const std::filesystem::path root(created);

    metadata_service service(vqec_vision_ai_unit_mdsvt_make_config(root));
    assert(service.vqec_vision_ai_appl_mdsvc_start().code_ == status_code::ok);
    const auto first = vqec_vision_ai_unit_mdsvt_make_chunk("chunk.1", 1U, 100000U);
    const auto second = vqec_vision_ai_unit_mdsvt_make_chunk("chunk.2", 2U, 200000U);
    const auto third = vqec_vision_ai_unit_mdsvt_make_chunk("chunk.3", 3U, 300000U);
    assert(service.vqec_vision_ai_appl_mdsvc_submit_trajectory(first).code_ ==
           status_code::ok);
    assert(service.vqec_vision_ai_appl_mdsvc_submit_trajectory(second).code_ ==
           status_code::ok);
    assert(service.vqec_vision_ai_appl_mdsvc_submit_trajectory(third).code_ ==
           status_code::ok);

    live_trajectory_snapshot snapshot;
    const auto trajectory_mask = vqec_vision_ai_cntr_stmet_get_access_domain_mask(
        spatiotemporal_access_domain::trajectory);
    assert(service.vqec_vision_ai_appl_mdsvc_get_live_snapshot(
               trajectory_mask, snapshot).code_ == status_code::ok);
    assert(snapshot.chunks_.size() == 1U);
    assert(snapshot.chunks_.front().chunk_id_ == "chunk.3");

    std::vector<live_trajectory_delta> deltas;
    bool has_gap = false;
    assert(service.vqec_vision_ai_appl_mdsvc_get_live_deltas(
               1U, 2U, trajectory_mask, deltas, has_gap).code_ == status_code::ok);
    assert(has_gap);
    assert(deltas.empty());

    spatiotemporal_query query;
    query.request_id_ = "query.service";
    query.source_ids_ = {"camera.a"};
    query.begin_ns_ = 1;
    query.end_ns_ = 1000000;
    query.allowed_access_domain_mask_ = trajectory_mask;
    query.authorization_revision_ = 1U;
    query.budget_.maximum_scan_bytes_ = 1048576U;
    query.budget_.maximum_result_bytes_ = 1048576U;
    query.budget_.deadline_ns_ = vqec_vision_ai_unit_mdsvt_get_deadline_ns();
    query.budget_.maximum_results_ = 8U;
    spatiotemporal_query_page page;
    assert(service.vqec_vision_ai_appl_mdsvc_query(query, page).code_ == status_code::ok);
    assert(page.trajectory_chunks_.size() == 3U);
    auto cancelled_query = query;
    cancelled_query.request_id_ = "query.cancelled";
    cancelled_query.budget_.deadline_ns_ =
        vqec_vision_ai_unit_mdsvt_get_deadline_ns() + 1000000000U;
    status cancelled_result{status_code::pending, "not started"};
    std::thread query_thread([&service, &cancelled_query, &cancelled_result]() {
        spatiotemporal_query_page cancelled_page;
        cancelled_result = service.vqec_vision_ai_appl_mdsvc_query(
            cancelled_query, cancelled_page);
    });
    status cancel_result{status_code::invalid_argument, "not found"};
    for (unsigned attempt = 0U; attempt < 10000U &&
         cancel_result.code_ != status_code::ok; ++attempt) {
        cancel_result = service.vqec_vision_ai_appl_mdsvc_cancel_query(
            cancelled_query.request_id_);
        std::this_thread::yield();
    }
    query_thread.join();
    assert(cancel_result.code_ == status_code::ok);
    assert(cancelled_result.code_ == status_code::timeout);
    const auto stats = service.vqec_vision_ai_appl_mdsvc_get_stats();
    assert(stats.accepted_records_ == 5U);
    assert(stats.committed_records_ == 3U);
    assert(stats.completed_queries_ == 1U);
    assert(stats.live_track_count_ == 1U);
    assert(service.vqec_vision_ai_appl_mdsvc_stop(true).code_ == status_code::ok);
    assert(service.vqec_vision_ai_appl_mdsvc_submit_trajectory(first).code_ ==
           status_code::invalid_state);

    const auto conflict_root = root / "conflict";
    metadata_service conflict_service(vqec_vision_ai_unit_mdsvt_make_config(conflict_root));
    assert(conflict_service.vqec_vision_ai_appl_mdsvc_start().code_ == status_code::ok);
    auto conflicting = first;
    conflicting.points_.back().anchor_x_ += 1;
    assert(conflict_service.vqec_vision_ai_appl_mdsvc_submit_trajectory(first).code_ ==
           status_code::ok);
    assert(conflict_service.vqec_vision_ai_appl_mdsvc_submit_trajectory(conflicting).code_ ==
           status_code::ok);
    assert(conflict_service.vqec_vision_ai_appl_mdsvc_stop(true).code_ ==
           status_code::invalid_argument);
    assert(conflict_service.vqec_vision_ai_appl_mdsvc_get_health().code_ ==
           status_code::invalid_argument);

    std::filesystem::remove_all(root);
    return 0;
}
