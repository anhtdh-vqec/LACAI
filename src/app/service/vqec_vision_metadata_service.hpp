#ifndef VQEC_VISION_AI_APP_METADATA_SERVICE_HPP
#define VQEC_VISION_AI_APP_METADATA_SERVICE_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "vqec_vision_spatiotemporal_store.hpp"

namespace vqec::vision::ai {

namespace metadata_service_limits {
inline constexpr std::size_t g_maximum_queue_capacity = 4096U;
inline constexpr std::size_t g_maximum_live_tracks = 4096U;
inline constexpr std::size_t g_maximum_live_deltas = 16384U;
}  // namespace metadata_service_limits

struct metadata_service_config {
    spatiotemporal_store_config store_;
    std::size_t queue_capacity_{0};
    std::size_t maximum_live_tracks_{0};
    std::size_t maximum_live_deltas_{0};
    std::vector<std::string> outbox_sinks_;
};

struct metadata_service_stats {
    std::uint64_t accepted_records_{0};
    std::uint64_t committed_records_{0};
    std::uint64_t rejected_records_{0};
    std::uint64_t failed_records_{0};
    std::uint64_t completed_queries_{0};
    std::uint64_t failed_queries_{0};
    std::uint64_t live_sequence_{0};
    std::size_t queue_depth_{0};
    std::size_t queue_high_watermark_{0};
    std::size_t live_track_count_{0};
};

// One worker owns all blocking SQLite calls. Producer methods only validate/copy into a
// bounded queue and update bounded live state; resource_exhausted means no durable acceptance.
class metadata_service final {
public:
    explicit metadata_service(metadata_service_config _config);
    ~metadata_service() noexcept;

    metadata_service(const metadata_service&) = delete;
    metadata_service& operator=(const metadata_service&) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_mdsvc_start();
    [[nodiscard]] status vqec_vision_ai_appl_mdsvc_stop(bool _drain) noexcept;

    [[nodiscard]] status vqec_vision_ai_appl_mdsvc_submit_trajectory(
        const trajectory_chunk& _chunk);
    [[nodiscard]] status vqec_vision_ai_appl_mdsvc_submit_association(
        const track_association_revision& _association);
    [[nodiscard]] status vqec_vision_ai_appl_mdsvc_submit_episode(
        const event_episode_revision& _episode);
    [[nodiscard]] status vqec_vision_ai_appl_mdsvc_submit_aggregate_contribution(
        const aggregate_contribution_revision& _contribution);

    [[nodiscard]] status vqec_vision_ai_appl_mdsvc_remove_live_track(
        const spatiotemporal_track_key& _track);
    [[nodiscard]] status vqec_vision_ai_appl_mdsvc_get_live_snapshot(
        std::uint32_t _allowed_access_domain_mask,
        live_trajectory_snapshot& _snapshot) const;
    [[nodiscard]] status vqec_vision_ai_appl_mdsvc_get_live_deltas(
        std::uint64_t _after_sequence, std::size_t _maximum_results,
        std::uint32_t _allowed_access_domain_mask,
        std::vector<live_trajectory_delta>& _deltas, bool& _has_sequence_gap) const;

    // Intended for API/job threads, never frame/DSP threads. The request is serialized behind
    // prior writes and retains its own deadline/scan/output bounds.
    [[nodiscard]] status vqec_vision_ai_appl_mdsvc_query(
        const spatiotemporal_query& _query, spatiotemporal_query_page& _page);

    [[nodiscard]] metadata_service_stats
    vqec_vision_ai_appl_mdsvc_get_stats() const noexcept;
    [[nodiscard]] status vqec_vision_ai_appl_mdsvc_get_store_stats(
        spatiotemporal_store_stats& _stats) const;

private:
    class implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_METADATA_SERVICE_HPP
