#ifndef VQEC_VISION_AI_CONTRACTS_SPATIOTEMPORAL_METADATA_HPP
#define VQEC_VISION_AI_CONTRACTS_SPATIOTEMPORAL_METADATA_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_version_registry.h"

namespace vqec::vision::ai {

inline constexpr std::uint32_t g_spatiotemporal_metadata_schema_version =
    VQEC_VISION_AI_BASELINE_SCHEMA_VERSION;
inline constexpr std::size_t g_spatiotemporal_max_identifier_bytes = 128U;
inline constexpr std::size_t g_spatiotemporal_max_points_per_chunk = 4096U;
inline constexpr std::size_t g_spatiotemporal_max_query_sources = 16U;
inline constexpr std::size_t g_spatiotemporal_max_query_dimensions = 16U;
inline constexpr std::size_t g_spatiotemporal_max_outbox_sinks = 8U;
inline constexpr std::size_t g_spatiotemporal_max_episode_claims = 32U;
inline constexpr std::size_t g_spatiotemporal_max_episode_references = 32U;
inline constexpr std::uint32_t g_spatiotemporal_score_scale_ppm = 1000000U;

enum class spatiotemporal_coordinate_space : std::uint8_t {
    source_pixel = 1,
    normalized_ppm,
    ground_plane_mm
};

enum class spatiotemporal_anchor : std::uint8_t {
    box_center = 1,
    box_footpoint,
    object_centroid,
    calibrated_ground_point
};

enum class trajectory_resolution : std::uint8_t {
    observation_exact = 1,
    trajectory_bounded,
    episode_fact,
    aggregate
};

enum class trajectory_sample_mode : std::uint8_t {
    exact = 1,
    fixed_gap,
    error_bounded,
    mandatory_only
};

enum class trajectory_point_flag : std::uint32_t {
    observed = 1U << 0U,
    predicted = 1U << 1U,
    mandatory = 1U << 2U,
    gap_before = 1U << 3U,
    lifecycle_boundary = 1U << 4U,
    scene_boundary = 1U << 5U,
    passage_boundary = 1U << 6U,
    attribute_boundary = 1U << 7U,
    episode_boundary = 1U << 8U,
    split_merge_boundary = 1U << 9U,
    has_box = 1U << 10U
};

enum class spatiotemporal_access_domain : std::uint32_t {
    aggregate = 1U << 0U,
    object = 1U << 1U,
    visual_attribute = 1U << 2U,
    trajectory = 1U << 3U,
    plate = 1U << 4U,
    identity = 1U << 5U,
    vector = 1U << 6U,
    media = 1U << 7U,
    operational = 1U << 8U,
    audit = 1U << 9U
};

inline constexpr std::uint32_t g_spatiotemporal_all_access_domains = (1U << 10U) - 1U;

inline constexpr std::uint32_t g_trajectory_point_known_flags =
    (1U << 11U) - 1U;

struct spatiotemporal_frame_locator {
    std::uint32_t schema_version_{g_spatiotemporal_metadata_schema_version};
    std::string device_id_;
    std::string source_id_;
    std::string boot_id_;
    std::uint64_t source_epoch_{0};
    std::uint64_t frame_id_{0};
    std::uint64_t source_pts_ns_{0};
    bool has_capture_utc_{false};
    std::int64_t capture_utc_ns_{0};
    std::uint64_t clock_uncertainty_ns_{0};
    std::string clock_mapping_revision_;
    std::string scene_revision_;
    std::string coordinate_revision_;
    std::string model_revision_;
    std::string tracker_revision_;
    std::string media_reference_;
};

struct spatiotemporal_track_key {
    std::string device_id_;
    std::string source_id_;
    std::string boot_id_;
    std::uint64_t source_epoch_{0};
    std::uint64_t local_track_id_{0};
};

struct trajectory_point {
    std::uint64_t frame_id_{0};
    std::uint64_t source_pts_ns_{0};
    bool has_capture_utc_{false};
    std::int64_t capture_utc_ns_{0};
    std::int32_t anchor_x_{0};
    std::int32_t anchor_y_{0};
    std::int32_t box_left_{0};
    std::int32_t box_top_{0};
    std::int32_t box_right_{0};
    std::int32_t box_bottom_{0};
    std::uint32_t flags_{0};
};

struct trajectory_chunk {
    std::uint32_t schema_version_{g_spatiotemporal_metadata_schema_version};
    std::string chunk_id_;
    spatiotemporal_track_key track_;
    std::string subject_ref_;
    std::string entity_category_;
    std::uint64_t chunk_sequence_{0};
    spatiotemporal_frame_locator first_frame_;
    spatiotemporal_frame_locator last_frame_;
    spatiotemporal_coordinate_space coordinate_space_{
        spatiotemporal_coordinate_space::source_pixel};
    spatiotemporal_anchor anchor_{spatiotemporal_anchor::box_footpoint};
    trajectory_resolution resolution_{trajectory_resolution::observation_exact};
    trajectory_sample_mode sample_mode_{trajectory_sample_mode::exact};
    std::uint32_t max_spatial_error_units_{0};
    std::uint64_t max_time_error_ns_{0};
    std::uint32_t required_access_domain_mask_{0};
    std::int32_t bounds_left_{0};
    std::int32_t bounds_top_{0};
    std::int32_t bounds_right_{0};
    std::int32_t bounds_bottom_{0};
    std::vector<trajectory_point> points_;
};

enum class association_review_state : std::uint8_t {
    candidate = 1,
    accepted,
    rejected,
    superseded
};

struct track_association_revision {
    std::uint32_t schema_version_{g_spatiotemporal_metadata_schema_version};
    std::string association_id_;
    std::uint64_t revision_{0};
    std::uint64_t supersedes_revision_{0};
    std::string entity_id_;
    std::string left_chunk_id_;
    std::string right_chunk_id_;
    std::string method_revision_;
    std::string topology_path_;
    std::uint32_t score_ppm_{0};
    std::uint64_t minimum_travel_ns_{0};
    std::uint64_t maximum_travel_ns_{0};
    std::uint64_t clock_uncertainty_ns_{0};
    association_review_state review_state_{association_review_state::candidate};
    std::int64_t recorded_ns_{0};
};

struct spatiotemporal_dimension {
    std::string key_;
    std::string value_;
};

enum class episode_lifecycle : std::uint8_t {
    opened = 1,
    updated,
    closed,
    corrected,
    tombstoned
};

// An episode is a versioned interval fact, not one detection per frame. Producers retain
// their stable episode_id across updates and corrections so retries cannot increase counts.
struct event_episode_revision {
    std::uint32_t schema_version_{g_spatiotemporal_metadata_schema_version};
    std::string episode_id_;
    std::uint64_t revision_{0};
    std::uint64_t supersedes_revision_{0};
    std::string source_id_;
    std::string semantic_type_;
    std::string subject_ref_;
    std::string scene_revision_;
    std::string rule_revision_;
    std::int64_t begin_ns_{0};
    std::int64_t end_ns_{0};
    std::int64_t recorded_ns_{0};
    episode_lifecycle lifecycle_{episode_lifecycle::opened};
    std::uint32_t severity_ppm_{0};
    std::uint32_t required_access_domain_mask_{0};
    std::vector<spatiotemporal_dimension> claims_;
    std::vector<std::string> evidence_references_;
};

enum class aggregate_contribution_operation : std::uint8_t {
    add = 1,
    retract
};

// Contributions are the durable idempotency boundary for rollups. Corrections append a new
// revision that supersedes the prior revision; rollup readers apply only the selected revision.
struct aggregate_contribution_revision {
    std::uint32_t schema_version_{g_spatiotemporal_metadata_schema_version};
    std::string contribution_id_;
    std::uint64_t revision_{0};
    std::uint64_t supersedes_revision_{0};
    std::string episode_id_;
    std::string source_id_;
    std::string aggregate_definition_id_;
    std::string scene_revision_;
    std::string definition_revision_;
    std::int64_t bucket_begin_ns_{0};
    std::int64_t bucket_end_ns_{0};
    std::int64_t recorded_ns_{0};
    aggregate_contribution_operation operation_{aggregate_contribution_operation::add};
    std::int64_t numerator_microunits_{0};
    std::int64_t denominator_microunits_{0};
    std::uint64_t observed_duration_ns_{0};
    std::uint64_t expected_duration_ns_{0};
    std::uint32_t required_access_domain_mask_{0};
    std::vector<spatiotemporal_dimension> dimensions_;
};

struct aggregate_bucket {
    std::string aggregate_definition_id_;
    std::string source_id_;
    std::string scene_revision_;
    std::string definition_revision_;
    std::int64_t bucket_begin_ns_{0};
    std::int64_t bucket_end_ns_{0};
    std::int64_t numerator_microunits_{0};
    std::int64_t denominator_microunits_{0};
    std::uint64_t observed_duration_ns_{0};
    std::uint64_t expected_duration_ns_{0};
    std::uint64_t contribution_count_{0};
    std::vector<spatiotemporal_dimension> dimensions_;
};

enum class live_trajectory_operation : std::uint8_t {
    upsert = 1,
    remove,
    reset
};

struct live_trajectory_delta {
    std::uint64_t sequence_{0};
    live_trajectory_operation operation_{live_trajectory_operation::upsert};
    spatiotemporal_track_key track_;
    trajectory_chunk chunk_;
};

struct live_trajectory_snapshot {
    std::uint64_t sequence_{0};
    std::vector<trajectory_chunk> chunks_;
};

enum class spatiotemporal_collection : std::uint8_t {
    observations = 1,
    tracklets,
    entities,
    episodes,
    passages,
    aggregates,
    health
};

enum class spatiotemporal_temporal_relation : std::uint8_t {
    overlaps = 1,
    during,
    before,
    after,
    ordered_sequence,
    dwell,
    recurrence
};

enum class spatiotemporal_spatial_relation : std::uint8_t {
    none = 1,
    bounds_intersect,
    path_intersects,
    inside,
    nearest
};

enum class spatiotemporal_revision_view : std::uint8_t {
    as_observed = 1,
    as_known_at,
    latest_corrected
};

struct spatiotemporal_query_budget {
    std::uint64_t maximum_scan_bytes_{0};
    std::uint64_t maximum_result_bytes_{0};
    std::uint64_t deadline_ns_{0};
    std::size_t maximum_results_{0};
};

struct spatiotemporal_query {
    std::uint32_t schema_version_{g_spatiotemporal_metadata_schema_version};
    std::string request_id_;
    spatiotemporal_collection collection_{spatiotemporal_collection::tracklets};
    std::vector<std::string> source_ids_;
    std::int64_t begin_ns_{0};
    std::int64_t end_ns_{0};
    std::string subject_ref_;
    std::string entity_id_;
    std::string semantic_type_;
    spatiotemporal_temporal_relation temporal_relation_{
        spatiotemporal_temporal_relation::overlaps};
    spatiotemporal_spatial_relation spatial_relation_{
        spatiotemporal_spatial_relation::none};
    bool has_spatial_bounds_{false};
    std::int32_t bounds_left_{0};
    std::int32_t bounds_top_{0};
    std::int32_t bounds_right_{0};
    std::int32_t bounds_bottom_{0};
    trajectory_resolution minimum_resolution_{trajectory_resolution::trajectory_bounded};
    spatiotemporal_revision_view revision_view_{
        spatiotemporal_revision_view::latest_corrected};
    std::int64_t known_at_ns_{0};
    std::uint32_t allowed_access_domain_mask_{0};
    std::uint64_t authorization_revision_{0};
    std::uint64_t snapshot_sequence_{0};
    std::uint64_t cursor_sequence_{0};
    spatiotemporal_query_budget budget_;
};

enum class spatiotemporal_result_completeness : std::uint8_t {
    complete = 1,
    partial,
    approximate,
    unsupported,
    budget_exceeded
};

struct spatiotemporal_query_page {
    spatiotemporal_result_completeness completeness_{
        spatiotemporal_result_completeness::partial};
    trajectory_resolution delivered_resolution_{trajectory_resolution::trajectory_bounded};
    std::uint64_t snapshot_sequence_{0};
    std::uint64_t next_cursor_sequence_{0};
    std::uint64_t scanned_bytes_{0};
    std::uint64_t result_bytes_{0};
    std::vector<trajectory_chunk> trajectory_chunks_;
    std::vector<track_association_revision> associations_;
    std::vector<event_episode_revision> episodes_;
    std::vector<aggregate_bucket> aggregate_buckets_;
    bool has_more_{false};
    bool has_coverage_gap_{false};
};

[[nodiscard]] constexpr std::uint32_t vqec_vision_ai_cntr_stmet_get_access_domain_mask(
    spatiotemporal_access_domain _domain) noexcept {
    return static_cast<std::uint32_t>(_domain);
}

[[nodiscard]] status vqec_vision_ai_cntr_stmet_validate_frame_locator(
    const spatiotemporal_frame_locator& _locator);

[[nodiscard]] status vqec_vision_ai_cntr_stmet_validate_track_key(
    const spatiotemporal_track_key& _track);

[[nodiscard]] status vqec_vision_ai_cntr_stmet_validate_trajectory_chunk(
    const trajectory_chunk& _chunk);

[[nodiscard]] status vqec_vision_ai_cntr_stmet_validate_association_revision(
    const track_association_revision& _association);

[[nodiscard]] status vqec_vision_ai_cntr_stmet_validate_episode_revision(
    const event_episode_revision& _episode);

[[nodiscard]] status vqec_vision_ai_cntr_stmet_validate_aggregate_contribution(
    const aggregate_contribution_revision& _contribution);

[[nodiscard]] status vqec_vision_ai_cntr_stmet_validate_query(
    const spatiotemporal_query& _query);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_SPATIOTEMPORAL_METADATA_HPP
