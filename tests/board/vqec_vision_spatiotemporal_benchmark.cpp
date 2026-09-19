#include "vqec_vision_metadata_service.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <sys/resource.h>
#include <thread>
#include <time.h>
#include <vector>

namespace {

using namespace vqec::vision::ai;

constexpr std::array<const char*, 27U> g_fixture_scenarios{{
    "security.restricted_area_smoking", "security.suspicious_weapon",
    "security.ppe_compliance", "security.fire_smoke_detection",
    "security.blacklist_person_alert", "security.attendance_recognition",
    "security.demographic_estimation", "security.people_density_heatmap",
    "security.unauthorized_intrusion", "security.people_entry_exit_count",
    "security.person_tracking", "security.vlm_context_alert",
    "security.abandoned_or_removed_object", "security.lost_item_trace",
    "security.luggage_cart_tracking", "security.vehicle_plate_recognition",
    "security.crowd_gathering", "security.abnormal_fight_conflict",
    "traffic.vehicle_flow", "traffic.lane_movement", "traffic.anpr_parking_access",
    "traffic.speed_acceleration", "traffic.queue_congestion",
    "traffic.movement_violation", "traffic.red_light", "traffic.od_travel_time",
    "traffic.near_miss_incident",
}};

constexpr std::uint64_t g_nanoseconds_per_second = 1000000000U;
constexpr std::uint64_t g_default_duration_seconds = 1U;
constexpr std::uint64_t g_default_sets_per_second = 25U;
constexpr std::size_t g_default_source_count = 4U;
constexpr std::size_t g_maximum_source_count = 16U;
constexpr std::uint64_t g_query_budget_bytes = 64U * 1024U * 1024U;

struct benchmark_options {
    std::filesystem::path root_;
    std::uint64_t duration_seconds_{g_default_duration_seconds};
    std::uint64_t sets_per_second_{g_default_sets_per_second};
    std::size_t source_count_{g_default_source_count};
    bool is_full_sync_{true};
};

struct benchmark_result {
    std::vector<std::uint64_t> query_latency_ns_;
    std::uint64_t attempted_sets_{0};
    std::uint64_t rejected_records_{0};
    std::uint64_t query_failures_{0};
};

bool vqec_vision_ai_board_stben_parse_u64(
    const char* _text, std::uint64_t& _value) {
    char* end = nullptr;
    const auto value = std::strtoull(_text, &end, 10);
    if (end == _text || end == nullptr || *end != '\0' || value == 0U) {
        return false;
    }
    _value = value;
    return true;
}

bool vqec_vision_ai_board_stben_parse(
    int _argc, char** _argv, benchmark_options& _options) {
    for (int index = 1; index < _argc; ++index) {
        if (index + 1 >= _argc) {
            return false;
        }
        const std::string option = _argv[index];
        const char* value = _argv[++index];
        std::uint64_t parsed = 0U;
        if (option == "--root") {
            _options.root_ = value;
        } else if (option == "--sync") {
            const std::string sync = value;
            if (sync == "full") {
                _options.is_full_sync_ = true;
            } else if (sync == "normal") {
                _options.is_full_sync_ = false;
            } else {
                return false;
            }
        } else if (option == "--duration-seconds" &&
                   vqec_vision_ai_board_stben_parse_u64(value, parsed)) {
            _options.duration_seconds_ = parsed;
        } else if (option == "--sets-per-second" &&
                   vqec_vision_ai_board_stben_parse_u64(value, parsed)) {
            _options.sets_per_second_ = parsed;
        } else if (option == "--sources" &&
                   vqec_vision_ai_board_stben_parse_u64(value, parsed) &&
                   parsed <= g_maximum_source_count) {
            _options.source_count_ = static_cast<std::size_t>(parsed);
        } else {
            return false;
        }
    }
    return !_options.root_.empty();
}

std::uint64_t vqec_vision_ai_board_stben_steady_ns() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

std::uint64_t vqec_vision_ai_board_stben_cpu_ns() {
    timespec value{};
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &value) != 0) {
        return 0U;
    }
    return static_cast<std::uint64_t>(value.tv_sec) * g_nanoseconds_per_second +
        static_cast<std::uint64_t>(value.tv_nsec);
}

std::string vqec_vision_ai_board_stben_source(std::size_t _index) {
    return "camera." + std::to_string(_index + 1U);
}

spatiotemporal_frame_locator vqec_vision_ai_board_stben_locator(
    const std::string& _source, std::uint64_t _frame, std::uint64_t _pts) {
    spatiotemporal_frame_locator locator;
    locator.device_id_ = "device.benchmark";
    locator.source_id_ = _source;
    locator.boot_id_ = "boot.benchmark";
    locator.source_epoch_ = 1U;
    locator.frame_id_ = _frame;
    locator.source_pts_ns_ = _pts;
    locator.has_capture_utc_ = true;
    locator.capture_utc_ns_ = static_cast<std::int64_t>(_pts);
    locator.clock_uncertainty_ns_ = 1000U;
    locator.clock_mapping_revision_ = "clock.v1";
    locator.scene_revision_ = "scene.v1";
    locator.coordinate_revision_ = "coordinate.v1";
    locator.model_revision_ = "model.v1";
    locator.tracker_revision_ = "tracker.v1";
    return locator;
}

trajectory_chunk vqec_vision_ai_board_stben_chunk(
    std::uint64_t _sequence, const std::string& _source) {
    trajectory_chunk chunk;
    chunk.chunk_id_ = "chunk." + std::to_string(_sequence);
    chunk.track_.device_id_ = "device.benchmark";
    chunk.track_.source_id_ = _source;
    chunk.track_.boot_id_ = "boot.benchmark";
    chunk.track_.source_epoch_ = 1U;
    chunk.track_.local_track_id_ = _sequence + 1U;
    chunk.subject_ref_ = "subject." + std::to_string(_sequence);
    chunk.entity_category_ = (_sequence % 3U) == 0U ? "vehicle" : "person";
    chunk.chunk_sequence_ = 1U;
    const auto pts = (_sequence + 1U) * 1000000U;
    chunk.first_frame_ = vqec_vision_ai_board_stben_locator(
        _source, _sequence * 2U + 1U, pts);
    chunk.last_frame_ = vqec_vision_ai_board_stben_locator(
        _source, _sequence * 2U + 2U, pts + 100000U);
    chunk.required_access_domain_mask_ =
        vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::trajectory);
    chunk.bounds_left_ = static_cast<std::int32_t>(_sequence % 1000U);
    chunk.bounds_top_ = 0;
    chunk.bounds_right_ = chunk.bounds_left_ + 100;
    chunk.bounds_bottom_ = 100;
    trajectory_point first;
    first.frame_id_ = chunk.first_frame_.frame_id_;
    first.source_pts_ns_ = chunk.first_frame_.source_pts_ns_;
    first.has_capture_utc_ = true;
    first.capture_utc_ns_ = static_cast<std::int64_t>(first.source_pts_ns_);
    first.anchor_x_ = chunk.bounds_left_;
    first.anchor_y_ = 50;
    first.flags_ = static_cast<std::uint32_t>(trajectory_point_flag::observed) |
        static_cast<std::uint32_t>(trajectory_point_flag::mandatory);
    auto last = first;
    last.frame_id_ = chunk.last_frame_.frame_id_;
    last.source_pts_ns_ = chunk.last_frame_.source_pts_ns_;
    last.capture_utc_ns_ = static_cast<std::int64_t>(last.source_pts_ns_);
    last.anchor_x_ = chunk.bounds_right_;
    chunk.points_ = {first, last};
    return chunk;
}

event_episode_revision vqec_vision_ai_board_stben_episode(
    std::uint64_t _sequence, const std::string& _source, const std::string& _scenario) {
    event_episode_revision episode;
    episode.episode_id_ = "episode." + std::to_string(_sequence);
    episode.revision_ = 1U;
    episode.source_id_ = _source;
    episode.semantic_type_ = _scenario;
    episode.subject_ref_ = "subject." + std::to_string(_sequence);
    episode.scene_revision_ = "scene.v1";
    episode.rule_revision_ = "rule.v1";
    episode.begin_ns_ = static_cast<std::int64_t>((_sequence + 1U) * 1000000U);
    episode.end_ns_ = episode.begin_ns_ + 500000;
    episode.recorded_ns_ = episode.end_ns_ + 1;
    episode.lifecycle_ = episode_lifecycle::closed;
    episode.severity_ppm_ = 700000U;
    episode.required_access_domain_mask_ =
        vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::object);
    episode.claims_ = {{"hotspot_cell", "grid_" + std::to_string(_sequence % 64U)},
        {"outcome", "positive"}};
    return episode;
}

aggregate_contribution_revision vqec_vision_ai_board_stben_contribution(
    std::uint64_t _sequence, const event_episode_revision& _episode) {
    aggregate_contribution_revision contribution;
    contribution.contribution_id_ = "contribution." + std::to_string(_sequence);
    contribution.revision_ = 1U;
    contribution.episode_id_ = _episode.episode_id_;
    contribution.source_id_ = _episode.source_id_;
    contribution.aggregate_definition_id_ = _episode.semantic_type_ + ".count";
    contribution.scene_revision_ = _episode.scene_revision_;
    contribution.definition_revision_ = "count.v1";
    contribution.bucket_begin_ns_ = 0;
    contribution.bucket_end_ns_ = static_cast<std::int64_t>(86400U * g_nanoseconds_per_second);
    contribution.recorded_ns_ = _episode.recorded_ns_;
    contribution.numerator_microunits_ = 1000000;
    contribution.denominator_microunits_ = 1000000;
    contribution.observed_duration_ns_ = 500000U;
    contribution.expected_duration_ns_ = 500000U;
    contribution.required_access_domain_mask_ =
        vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::aggregate);
    contribution.dimensions_ = {{"scenario", _episode.semantic_type_}};
    return contribution;
}

metadata_service_config vqec_vision_ai_board_stben_config(
    const benchmark_options& _options) {
    metadata_service_config config;
    config.store_.root_directory_ = _options.root_.string();
    config.store_.shard_duration_ns_ = 60000000000U;
    config.store_.maximum_shard_bytes_ = 64U * 1024U * 1024U;
    config.store_.maximum_store_bytes_ = 2U * 1024U * 1024U * 1024U;
    config.store_.reserve_free_bytes_ = 64U * 1024U * 1024U;
    config.store_.maximum_encoded_chunk_bytes_ = 256U * 1024U;
    config.store_.maximum_query_results_ = 4096U;
    config.store_.busy_timeout_ms_ = 1000U;
    config.store_.wal_autocheckpoint_pages_ = 1000U;
    config.store_.is_full_sync_ = _options.is_full_sync_;
    config.queue_capacity_ = 4096U;
    config.maximum_live_tracks_ = 4096U;
    config.maximum_live_deltas_ = 16384U;
    config.maximum_batch_records_ = 128U;
    config.outbox_sinks_ = {"kafka.offline"};
    return config;
}

spatiotemporal_query vqec_vision_ai_board_stben_query(
    const benchmark_options& _options, const std::string& _semantic_type) {
    spatiotemporal_query query;
    query.request_id_ = "benchmark.query";
    query.collection_ = spatiotemporal_collection::aggregates;
    for (std::size_t index = 0U; index < _options.source_count_; ++index) {
        query.source_ids_.push_back(vqec_vision_ai_board_stben_source(index));
    }
    query.begin_ns_ = 0;
    query.end_ns_ = static_cast<std::int64_t>(86400U * g_nanoseconds_per_second);
    query.semantic_type_ = _semantic_type;
    query.allowed_access_domain_mask_ = g_spatiotemporal_all_access_domains;
    query.authorization_revision_ = 1U;
    query.budget_.maximum_scan_bytes_ = g_query_budget_bytes;
    query.budget_.maximum_result_bytes_ = g_query_budget_bytes;
    query.budget_.deadline_ns_ =
        vqec_vision_ai_board_stben_steady_ns() + g_nanoseconds_per_second;
    query.budget_.maximum_results_ = 4096U;
    return query;
}

std::uint64_t vqec_vision_ai_board_stben_quantile(
    const std::vector<std::uint64_t>& _sorted, std::size_t _percent) {
    if (_sorted.empty()) {
        return 0U;
    }
    const auto index = std::min(_sorted.size() - 1U,
        (_sorted.size() * _percent + 99U) / 100U - 1U);
    return _sorted[index];
}

std::uint64_t vqec_vision_ai_board_stben_store_bytes(
    const std::filesystem::path& _root) {
    std::error_code error;
    std::uint64_t bytes = 0U;
    for (std::filesystem::directory_iterator iterator(_root, error), end;
         !error && iterator != end; iterator.increment(error)) {
        if (iterator->is_regular_file(error) && !error) {
            bytes += iterator->file_size(error);
        }
    }
    return error ? 0U : bytes;
}

int vqec_vision_ai_board_stben_run(const benchmark_options& _options) {
    std::error_code error;
    if (std::filesystem::exists(_options.root_, error) &&
        (!std::filesystem::is_directory(_options.root_, error) ||
            std::filesystem::directory_iterator(_options.root_, error) !=
                std::filesystem::directory_iterator{})) {
        std::fprintf(stderr, "benchmark root must not exist or must be empty\n");
        return 2;
    }
    metadata_service service(vqec_vision_ai_board_stben_config(_options));
    auto status_value = service.vqec_vision_ai_appl_mdsvc_start();
    if (status_value.code_ != status_code::ok) {
        std::fprintf(stderr, "metadata start failed: %s\n", status_value.message_.c_str());
        return 3;
    }
    benchmark_result result;
    std::uint64_t sequence = 0U;
    for (; sequence < g_fixture_scenarios.size(); ++sequence) {
        const auto source = vqec_vision_ai_board_stben_source(
            static_cast<std::size_t>(sequence % _options.source_count_));
        const auto episode = vqec_vision_ai_board_stben_episode(
            sequence, source, g_fixture_scenarios[sequence]);
        const auto contribution = vqec_vision_ai_board_stben_contribution(sequence, episode);
        const auto chunk = vqec_vision_ai_board_stben_chunk(sequence, source);
        ++result.attempted_sets_;
        if (service.vqec_vision_ai_appl_mdsvc_submit_episode(episode).code_ != status_code::ok ||
            service.vqec_vision_ai_appl_mdsvc_submit_aggregate_contribution(
                contribution).code_ != status_code::ok ||
            service.vqec_vision_ai_appl_mdsvc_submit_trajectory(chunk).code_ !=
                status_code::ok) {
            ++result.rejected_records_;
        }
    }
    std::atomic<bool> stop_queries{false};
    std::thread reader([&]() {
        std::size_t index = 0U;
        while (!stop_queries.load()) {
            auto query = vqec_vision_ai_board_stben_query(_options,
                std::string(g_fixture_scenarios[index]) + ".count");
            spatiotemporal_query_page page;
            const auto begin = vqec_vision_ai_board_stben_steady_ns();
            const auto query_status = service.vqec_vision_ai_appl_mdsvc_query(query, page);
            const auto end = vqec_vision_ai_board_stben_steady_ns();
            if (query_status.code_ == status_code::ok) {
                result.query_latency_ns_.push_back(end - begin);
            } else {
                ++result.query_failures_;
            }
            index = (index + 1U) % g_fixture_scenarios.size();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    });
    const auto wall_begin = vqec_vision_ai_board_stben_steady_ns();
    const auto cpu_begin = vqec_vision_ai_board_stben_cpu_ns();
    const auto wall_deadline = wall_begin + _options.duration_seconds_ * g_nanoseconds_per_second;
    const auto interval_ns = g_nanoseconds_per_second / _options.sets_per_second_;
    auto next_ns = wall_begin;
    while (vqec_vision_ai_board_stben_steady_ns() < wall_deadline) {
        const auto source = vqec_vision_ai_board_stben_source(
            static_cast<std::size_t>(sequence % _options.source_count_));
        const auto episode = vqec_vision_ai_board_stben_episode(sequence, source,
            g_fixture_scenarios[sequence % g_fixture_scenarios.size()]);
        const auto contribution = vqec_vision_ai_board_stben_contribution(sequence, episode);
        const auto chunk = vqec_vision_ai_board_stben_chunk(sequence, source);
        ++result.attempted_sets_;
        if (service.vqec_vision_ai_appl_mdsvc_submit_episode(episode).code_ != status_code::ok) {
            ++result.rejected_records_;
        }
        if (service.vqec_vision_ai_appl_mdsvc_submit_aggregate_contribution(
                contribution).code_ != status_code::ok) {
            ++result.rejected_records_;
        }
        if (service.vqec_vision_ai_appl_mdsvc_submit_trajectory(chunk).code_ != status_code::ok) {
            ++result.rejected_records_;
        }
        ++sequence;
        next_ns += interval_ns;
        const auto now = vqec_vision_ai_board_stben_steady_ns();
        if (next_ns > now) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(next_ns - now));
        }
    }
    stop_queries.store(true);
    reader.join();
    std::uint64_t oracle_failures = 0U;
    for (const auto* scenario : g_fixture_scenarios) {
        auto query = vqec_vision_ai_board_stben_query(
            _options, std::string(scenario) + ".count");
        spatiotemporal_query_page page;
        status_value = service.vqec_vision_ai_appl_mdsvc_query(query, page);
        if (status_value.code_ != status_code::ok || page.aggregate_buckets_.empty()) {
            ++oracle_failures;
        }
    }
    const auto wall_finish = vqec_vision_ai_board_stben_steady_ns();
    const auto cpu_finish = vqec_vision_ai_board_stben_cpu_ns();
    const auto service_stats = service.vqec_vision_ai_appl_mdsvc_get_stats();
    status_value = service.vqec_vision_ai_appl_mdsvc_stop(true);
    if (status_value.code_ != status_code::ok) {
        std::fprintf(stderr, "metadata stop failed: %s\n", status_value.message_.c_str());
        return 4;
    }
    std::sort(result.query_latency_ns_.begin(), result.query_latency_ns_.end());
    rusage usage{};
    (void)getrusage(RUSAGE_SELF, &usage);
    const auto wall_ns = wall_finish - wall_begin;
    const auto cpu_ns = cpu_finish >= cpu_begin ? cpu_finish - cpu_begin : 0U;
    const double cpu_percent = wall_ns == 0U
        ? 0.0
        : static_cast<double>(cpu_ns) * 100.0 / static_cast<double>(wall_ns);
    std::printf(
        "{\"schema_version\":1,\"scenario_count\":%zu,\"attempted_sets\":%llu,"
        "\"rejected_records\":%llu,\"committed_records\":%llu,"
        "\"failed_records\":%llu,\"query_count\":%zu,\"query_failures\":%llu,"
        "\"oracle_failures\":%llu,\"query_p50_ms\":%.3f,\"query_p95_ms\":%.3f,"
        "\"query_p99_ms\":%.3f,\"cpu_percent_one_core\":%.3f,"
        "\"max_rss_kib\":%ld,\"store_bytes\":%llu,\"queue_high_watermark\":%zu}\n",
        g_fixture_scenarios.size(),
        static_cast<unsigned long long>(result.attempted_sets_),
        static_cast<unsigned long long>(result.rejected_records_),
        static_cast<unsigned long long>(service_stats.committed_records_),
        static_cast<unsigned long long>(service_stats.failed_records_),
        result.query_latency_ns_.size(),
        static_cast<unsigned long long>(result.query_failures_),
        static_cast<unsigned long long>(oracle_failures),
        static_cast<double>(vqec_vision_ai_board_stben_quantile(
            result.query_latency_ns_, 50U)) / 1000000.0,
        static_cast<double>(vqec_vision_ai_board_stben_quantile(
            result.query_latency_ns_, 95U)) / 1000000.0,
        static_cast<double>(vqec_vision_ai_board_stben_quantile(
            result.query_latency_ns_, 99U)) / 1000000.0,
        cpu_percent, usage.ru_maxrss,
        static_cast<unsigned long long>(vqec_vision_ai_board_stben_store_bytes(_options.root_)),
        service_stats.queue_high_watermark_);
    return oracle_failures == 0U && result.rejected_records_ == 0U &&
            service_stats.failed_records_ == 0U
        ? 0
        : 5;
}

}  // namespace

int main(int argc, char** argv) {
    benchmark_options options;
    if (!vqec_vision_ai_board_stben_parse(argc, argv, options)) {
        std::fprintf(stderr,
            "usage: %s --root PATH [--duration-seconds N] [--sets-per-second N] "
            "[--sources 1..16] [--sync full|normal]\n", argv[0]);
        return 1;
    }
    return vqec_vision_ai_board_stben_run(options);
}
