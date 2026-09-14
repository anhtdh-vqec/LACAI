#ifndef VQEC_VISION_AI_REFER_REFERENCE_FEATURE_HPP
#define VQEC_VISION_AI_REFER_REFERENCE_FEATURE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "vqec/vision/ai/ports/vqec_vision_feature_processor.hpp"

namespace vqec::vision::ai {

namespace reference_feature_limits {
inline constexpr std::size_t g_max_tracks = 256;
inline constexpr std::size_t g_max_identifier_bytes = 128;
}  // namespace reference_feature_limits

enum class reference_feature_mode { roi_presence, line_crossing, count };

struct reference_feature_params {
    reference_feature_mode mode_{reference_feature_mode::roi_presence};
    // ROI rectangle in source pixel coordinates for roi_presence and count.
    overlay_box zone_{};
    // line_crossing: horizontal line uses the box centre y, vertical uses centre x.
    bool line_horizontal_{true};
    float line_position_{0.0F};
    // Emit an episode_updated snapshot once a track has stayed inside for this long.
    std::uint64_t dwell_ns_{0};
    // Minimum spacing between repeated events for one track.
    std::uint64_t cooldown_ns_{0};
    // Empty selects every class.
    std::string class_filter_;
    std::string event_schema_id_{"reference.zone"};
    std::string event_schema_version_{"1"};
    float confidence_{0.5F};
};

// Device-free neutral feature processor. It consumes tracked observations and emits bounded
// feature events; it never calls a backend and owns only numeric per-track zone state. It
// exists to prove the feature API and to feed tests, not as a qualified usecase rule.
class reference_zone_feature final : public feature_processor_port {
public:
    reference_zone_feature() = default;
    explicit reference_zone_feature(reference_feature_params _params);

    [[nodiscard]] status vqec_vision_ai_ports_ftpro_validate_activation(
        const feature_processor_config& _config) const override;
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_reset_epoch(
        std::uint64_t _source_epoch) override;
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_process_observations(
        const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, feature_event_batch& _events) override;

private:
    struct track_state {
        std::uint64_t track_id_{0};
        bool inside_{false};
        bool side_{false};
        bool seen_{false};
        bool dwell_emitted_{false};
        std::uint64_t entered_ns_{0};
        std::uint64_t last_event_ns_{0};
    };

    [[nodiscard]] bool vqec_vision_ai_refer_rfeat_matches(
        const observation& _item) const noexcept;
    [[nodiscard]] bool vqec_vision_ai_refer_rfeat_inside(
        const observation& _item) const noexcept;
    void vqec_vision_ai_refer_rfeat_make_event(
        const observation& _item, feature_event_kind _kind, const std::string& _value,
        feature_event& _event);
    [[nodiscard]] track_state* vqec_vision_ai_refer_rfeat_find_track(
        std::uint64_t _track_id) noexcept;

    reference_feature_params params_;
    // The port's validate_activation is const, so activation stores the bound config here.
    mutable feature_processor_config config_;
    mutable bool is_configured_{false};
    std::array<track_state, reference_feature_limits::g_max_tracks> tracks_{};
    std::uint16_t track_count_{0};
    std::uint64_t event_sequence_{0};
    bool count_initialized_{false};
    int last_count_{-1};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_REFER_REFERENCE_FEATURE_HPP
