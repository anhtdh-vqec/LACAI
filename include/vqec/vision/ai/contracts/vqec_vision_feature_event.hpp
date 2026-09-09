#ifndef VQEC_VISION_AI_CONTRACTS_FEATURE_EVENT_HPP
#define VQEC_VISION_AI_CONTRACTS_FEATURE_EVENT_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_observation.hpp"

namespace vqec::vision::ai {

namespace feature_event_limits {
inline constexpr std::size_t g_max_events_per_batch = 64;
inline constexpr std::size_t g_max_track_references_per_event = 32;
inline constexpr std::size_t g_max_fields_per_event = 32;
inline constexpr std::size_t g_max_model_versions_per_event = 16;
inline constexpr std::size_t g_max_identifier_bytes = 128;
inline constexpr std::size_t g_max_field_value_bytes = 512;
}  // namespace feature_event_limits

enum class feature_event_kind { episode_opened, episode_updated, episode_closed, snapshot };

struct feature_event_field {
    std::string schema_id_;
    std::string schema_version_;
    std::string value_;
    float confidence_{0.0F};
    observation_quality quality_{observation_quality::unknown};
};

struct feature_event {
    preview_frame_key frame_;
    std::string source_id_;
    std::string feature_id_;
    std::string event_id_;
    std::string event_schema_id_;
    std::string event_schema_version_;
    feature_event_kind kind_{feature_event_kind::snapshot};
    std::uint64_t occurred_at_ns_{UINT64_MAX};
    std::uint64_t config_revision_{0};
    std::vector<std::string> model_version_ids_;
    std::vector<std::uint64_t> track_ids_;
    std::vector<feature_event_field> fields_;
    std::string evidence_request_id_;
};

struct feature_event_batch {
    preview_frame_key frame_;
    preview_geometry geometry_;
    std::vector<feature_event> events_;
};

struct feature_processor_config {
    std::string source_id_;
    std::string feature_id_;
    std::uint64_t config_revision_{0};
    std::size_t max_events_per_update_{0};
    std::size_t max_track_references_per_event_{0};
    std::size_t max_fields_per_event_{0};
};

[[nodiscard]] status vqec_vision_ai_core_ftevt_validate_processor_config(
    const feature_processor_config& _config);

[[nodiscard]] status vqec_vision_ai_core_ftevt_validate_batch(
    const feature_event_batch& _batch, const preview_frame_key& _expected_frame,
    const preview_geometry& _expected_geometry, const feature_processor_config& _config);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_FEATURE_EVENT_HPP
