#ifndef VQEC_VISION_AI_CONTRACTS_METADATA_QUERY_HPP
#define VQEC_VISION_AI_CONTRACTS_METADATA_QUERY_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_version_registry.h"

namespace vqec::vision::ai {

inline constexpr std::uint32_t g_metadata_query_schema_version =
    VQEC_VISION_AI_BASELINE_SCHEMA_VERSION;
inline constexpr std::size_t g_metadata_query_max_identifier_bytes = 128U;
inline constexpr std::size_t g_metadata_query_max_sources = 16U;
inline constexpr std::size_t g_metadata_query_max_projection_fields = 32U;
inline constexpr std::size_t g_metadata_query_max_outbox_sinks = 8U;
inline constexpr std::uint32_t g_metadata_query_confidence_scale_ppm = 1000000U;

enum class metadata_record_family : std::uint8_t {
    source_scene_revision = 1,
    observation_sample,
    track_segment,
    trajectory_chunk,
    attribute_assertion,
    relation_interval,
    passage_presence,
    measurement_sample,
    external_state_interval,
    event_episode,
    recognition_attendance,
    plate_read_consensus,
    context_assessment,
    aggregate_bucket,
    evidence_reference,
    association_hypothesis,
    coverage_health_interval,
    delivery_archive_audit
};

enum class metadata_value_state : std::uint8_t {
    known = 1,
    unknown,
    not_observable,
    not_supported,
    expired
};

enum class metadata_scope : std::uint32_t {
    aggregate = 1U << 0U,
    object = 1U << 1U,
    visual_attribute = 1U << 2U,
    trajectory = 1U << 3U,
    plate = 1U << 4U,
    identity = 1U << 5U,
    biometric = 1U << 6U,
    media = 1U << 7U,
    operational = 1U << 8U,
    audit = 1U << 9U
};

enum class metadata_query_kind : std::uint8_t {
    q01_object_attribute = 1,
    q02_attribute_at_event,
    q03_trajectory_detail,
    q04_ordered_passage,
    q05_presence_dwell,
    q06_relation,
    q07_cross_camera,
    q08_event,
    q09_recognition,
    q10_attendance,
    q11_missing_object,
    q12_plate_search,
    q13_plate_fuzzy,
    q14_flow_heatmap,
    q15_trend,
    q16_similarity,
    q17_vlm_alert,
    q18_vehicle_flow,
    q19_lane_movement,
    q20_parking,
    q21_speed,
    q22_congestion,
    q23_violation,
    q24_origin_destination,
    q25_retroactive_geometry,
    q26_explain,
    q27_coverage,
    q28_export,
    q29_correction_purge,
    q30_recompute
};

enum class metadata_query_completeness : std::uint8_t {
    complete = 1,
    partial,
    approximate,
    unsupported,
    budget_exceeded
};

struct metadata_record {
    std::uint32_t schema_version_{g_metadata_query_schema_version};
    std::string record_id_;
    std::uint64_t revision_{0};
    std::uint64_t supersedes_revision_{0};
    metadata_record_family family_{metadata_record_family::source_scene_revision};
    std::string device_id_;
    std::string source_id_;
    std::string boot_id_;
    std::uint64_t source_epoch_{0};
    std::string subject_ref_;
    std::string object_ref_;
    std::string scene_ref_;
    std::string semantic_type_;
    std::string typed_value_;
    metadata_value_state value_state_{metadata_value_state::unknown};
    std::int64_t valid_begin_ns_{0};
    std::int64_t valid_end_ns_{0};
    std::int64_t recorded_ns_{0};
    std::uint64_t clock_uncertainty_ns_{0};
    std::uint32_t confidence_ppm_{0};
    metadata_scope sensitivity_scope_{metadata_scope::object};
    std::string producer_revision_;
    std::string payload_;
    bool is_tombstone_{false};
};

struct metadata_query_request {
    std::uint32_t schema_version_{g_metadata_query_schema_version};
    std::string query_id_;
    metadata_query_kind kind_{metadata_query_kind::q01_object_attribute};
    std::vector<std::string> source_ids_;
    std::int64_t begin_ns_{0};
    std::int64_t end_ns_{0};
    std::string subject_ref_;
    std::string scene_ref_;
    std::string semantic_type_;
    std::string typed_value_;
    std::vector<std::string> projection_;
    std::uint32_t allowed_scope_mask_{0};
    std::uint64_t authorization_revision_{0};
    std::uint64_t snapshot_sequence_{0};
    std::uint64_t cursor_sequence_{0};
    std::size_t page_size_{0};
};

struct metadata_query_page {
    metadata_query_completeness completeness_{metadata_query_completeness::partial};
    std::uint64_t snapshot_sequence_{0};
    std::uint64_t next_cursor_sequence_{0};
    std::uint64_t authorization_revision_{0};
    std::string coverage_state_;
    std::vector<metadata_record> records_;
    bool has_more_{false};
};

[[nodiscard]] status vqec_vision_ai_cntr_mdqry_validate_record(
    const metadata_record& _record, std::size_t _max_payload_bytes);

[[nodiscard]] status vqec_vision_ai_cntr_mdqry_validate_request(
    const metadata_query_request& _request, std::size_t _max_page_size);

[[nodiscard]] std::uint32_t vqec_vision_ai_cntr_mdqry_get_scope_mask(
    metadata_scope _scope) noexcept;

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_METADATA_QUERY_HPP
