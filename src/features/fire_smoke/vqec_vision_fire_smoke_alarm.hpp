#ifndef VQEC_VISION_AI_FIRES_FIRE_SMOKE_ALARM_HPP
#define VQEC_VISION_AI_FIRES_FIRE_SMOKE_ALARM_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "vqec/vision/ai/ports/vqec_vision_feature_processor.hpp"

namespace vqec::vision::ai {

namespace fire_smoke_limits {
inline constexpr std::size_t g_max_incidents = 64;
inline constexpr std::size_t g_max_zone_ids = 32;
inline constexpr std::size_t g_max_profile_bytes = 128;
inline constexpr char g_fire_class_id[] = "fire";
inline constexpr char g_smoke_class_id[] = "smoke";
inline constexpr char g_event_schema_id[] = "security.fire_smoke.event";
inline constexpr char g_event_schema_version[] = "1";
}  // namespace fire_smoke_limits

enum class fire_smoke_gap_policy { retain, interrupt };

struct fire_smoke_alarm_config {
    std::string model_version_id_;
    bool fire_enabled_{true};
    bool smoke_enabled_{true};
    float fire_alarm_confidence_{0.0F};
    float smoke_alarm_confidence_{0.0F};
    float minimum_region_area_ratio_{0.0F};
    std::uint32_t confirmation_count_{0};
    std::uint64_t confirmation_duration_ns_{0};
    std::uint32_t clear_count_{0};
    std::uint64_t clear_duration_ns_{0};
    std::uint64_t update_interval_ns_{0};
    float association_iou_{0.0F};
    std::uint64_t scene_revision_{0};
    std::size_t max_active_incidents_{0};
    fire_smoke_gap_policy source_gap_policy_{fire_smoke_gap_policy::interrupt};
    float high_confidence_{0.0F};
    std::uint64_t critical_duration_ns_{0};
    bool evidence_enabled_{false};
    std::string evidence_profile_ref_;
    std::uint64_t evidence_pre_duration_ms_{0};
    std::uint64_t evidence_post_duration_ms_{0};
    std::uint64_t evidence_retry_deadline_ms_{0};
    bool evidence_snapshot_{false};
    bool evidence_clip_{false};
};

class fire_smoke_alarm final : public feature_processor_port {
public:
    explicit fire_smoke_alarm(fire_smoke_alarm_config _config);

    [[nodiscard]] status vqec_vision_ai_ports_ftpro_validate_activation(
        const feature_processor_config& _config) const override;
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_reset_epoch(
        std::uint64_t _source_epoch) override;
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_process_observations(
        const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, feature_event_batch& _events) override;

private:
    struct incident_state {
        bool occupied_{false};
        bool active_{false};
        bool seen_{false};
        std::string class_id_;
        overlay_box region_;
        float max_confidence_{0.0F};
        std::uint64_t track_id_{0};
        std::uint64_t first_seen_pts_ns_{0};
        std::uint64_t episode_begin_pts_ns_{0};
        std::uint64_t last_seen_pts_ns_{0};
        std::uint64_t last_update_pts_ns_{0};
        std::uint64_t last_observed_frame_id_{0};
        std::uint32_t confirmation_count_{0};
        std::uint32_t clear_count_{0};
        std::uint64_t episode_revision_{0};
        std::string event_id_;
    };

    [[nodiscard]] bool vqec_vision_ai_fires_fsalm_matches(
        const observation& _item, const preview_geometry& _geometry) const noexcept;
    [[nodiscard]] incident_state* vqec_vision_ai_fires_fsalm_find_or_allocate(
        const observation& _item) noexcept;
    void vqec_vision_ai_fires_fsalm_emit(
        incident_state& _incident, const preview_frame_key& _frame,
        feature_event_kind _kind, const char* _reason, feature_event& _event);
    void vqec_vision_ai_fires_fsalm_clear_incident(incident_state& _incident) noexcept;

    fire_smoke_alarm_config alarm_config_;
    mutable feature_processor_config processor_config_;
    mutable bool configured_{false};
    std::array<incident_state, fire_smoke_limits::g_max_incidents> incidents_{};
    std::uint64_t source_epoch_{0};
    std::uint64_t event_sequence_{0};
};

[[nodiscard]] status vqec_vision_ai_fires_fsalm_validate_config(
    const fire_smoke_alarm_config& _config) noexcept;

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_FIRES_FIRE_SMOKE_ALARM_HPP
