#ifndef VQEC_VISION_AI_APP_METADATA_RUNTIME_HPP
#define VQEC_VISION_AI_APP_METADATA_RUNTIME_HPP

#include <cstddef>
#include <cstdint>
#include <istream>
#include <memory>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_deployment_config.hpp"
#include "vqec/vision/ai/ports/vqec_vision_feature_event_sink.hpp"
#include "vqec_vision_metadata_service.hpp"

namespace vqec::vision::ai {

namespace metadata_runtime_limits {
inline constexpr std::size_t g_maximum_sources = deployment_limits::g_max_sources;
inline constexpr std::size_t g_maximum_track_builders = 4096U;
inline constexpr std::size_t g_maximum_points_per_chunk = 4096U;
inline constexpr std::size_t g_maximum_event_access_rules = 64U;
}  // namespace metadata_runtime_limits

struct metadata_event_access_rule {
    std::string feature_id_;
    std::uint32_t access_domain_mask_{0};
};

struct metadata_source_profile {
    std::string source_id_;
    std::string scene_revision_;
    std::string coordinate_revision_;
    std::string clock_mapping_revision_;
    std::string trajectory_model_id_;
    std::string trajectory_authorization_feature_id_;
    std::string trajectory_authorization_attribute_id_;
    std::uint32_t source_width_{0};
    std::uint32_t source_height_{0};
    std::vector<metadata_event_access_rule> event_access_rules_;
};

struct metadata_runtime_config {
    std::uint32_t schema_version_{0};
    std::string device_id_;
    metadata_service_config service_;
    std::vector<metadata_source_profile> sources_;
    std::size_t maximum_points_per_chunk_{0};
    std::uint64_t maximum_chunk_duration_ns_{0};
    std::uint64_t minimum_sample_interval_ns_{0};
    std::uint64_t stale_track_ns_{0};
    std::int64_t aggregate_bucket_ns_{0};
    std::uint32_t hotspot_grid_columns_{0};
    std::uint32_t hotspot_grid_rows_{0};
    std::uint64_t maintenance_interval_ns_{0};
    std::uint64_t trajectory_retention_ns_{0};
    std::uint64_t episode_retention_ns_{0};
    std::uint64_t contribution_retention_ns_{0};
    std::uint64_t rollup_retention_ns_{0};
    bool required_{true};
};

[[nodiscard]] status vqec_vision_ai_appl_mdrun_load_config(
    std::istream& _stream, const deployment_config& _deployment,
    metadata_runtime_config& _config);

// Production lifecycle owner and authorized event sink. The event dispatcher must authorize an
// event before calling this sink. Trajectory authorization is evaluated by the composition root
// and passed explicitly to submit_observations.
class metadata_runtime final : public feature_event_sink_port {
public:
    metadata_runtime(metadata_runtime_config _config,
        feature_event_sink_port& _downstream_sink);
    ~metadata_runtime() noexcept override;

    metadata_runtime(const metadata_runtime&) = delete;
    metadata_runtime& operator=(const metadata_runtime&) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_mdrun_start();
    [[nodiscard]] status vqec_vision_ai_appl_mdrun_stop(bool _drain) noexcept;

    [[nodiscard]] status vqec_vision_ai_appl_mdrun_submit_observations(
        const std::string& _source_id, const std::string& _model_revision,
        const std::string& _tracker_revision, const observation_batch& _batch,
        bool _is_authorized);

    [[nodiscard]] status vqec_vision_ai_appl_mdrun_maintain(
        std::uint64_t _source_time_ns);

    [[nodiscard]] const metadata_source_profile*
    vqec_vision_ai_appl_mdrun_find_source(const std::string& _source_id) const noexcept;

    [[nodiscard]] status vqec_vision_ai_ports_fesnk_deliver_event(
        const feature_event& _event) override;

    [[nodiscard]] metadata_service_stats
    vqec_vision_ai_appl_mdrun_get_stats() const noexcept;

private:
    class implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_METADATA_RUNTIME_HPP
