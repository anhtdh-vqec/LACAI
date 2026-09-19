#ifndef VQEC_VISION_AI_ADAPTERS_STORAGE_SPATIOTEMPORAL_STORE_HPP
#define VQEC_VISION_AI_ADAPTERS_STORAGE_SPATIOTEMPORAL_STORE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_spatiotemporal_metadata.hpp"

struct sqlite3;

namespace vqec::vision::ai {

struct spatiotemporal_store_config {
    std::string root_directory_;
    std::uint64_t shard_duration_ns_{0};
    std::uint64_t maximum_shard_bytes_{0};
    std::uint64_t maximum_store_bytes_{0};
    std::uint64_t reserve_free_bytes_{0};
    std::size_t maximum_encoded_chunk_bytes_{0};
    std::size_t maximum_query_results_{0};
    std::uint32_t busy_timeout_ms_{0};
    std::uint32_t wal_autocheckpoint_pages_{0};
    bool is_full_sync_{true};
};

struct spatiotemporal_store_stats {
    std::uint64_t committed_chunks_{0};
    std::uint64_t committed_associations_{0};
    std::uint64_t committed_episode_revisions_{0};
    std::uint64_t committed_aggregate_revisions_{0};
    std::uint64_t sealed_shards_{0};
    std::uint64_t store_bytes_{0};
    std::uint64_t orphan_chunks_recovered_{0};
    std::uint64_t rejected_quota_writes_{0};
};

class sqlite_spatiotemporal_store final {
public:
    explicit sqlite_spatiotemporal_store(spatiotemporal_store_config _config);
    ~sqlite_spatiotemporal_store() noexcept;

    sqlite_spatiotemporal_store(const sqlite_spatiotemporal_store&) = delete;
    sqlite_spatiotemporal_store& operator=(const sqlite_spatiotemporal_store&) = delete;

    [[nodiscard]] status vqec_vision_ai_stor_stsql_open();
    [[nodiscard]] status vqec_vision_ai_stor_stsql_close() noexcept;

    [[nodiscard]] status vqec_vision_ai_stor_stsql_ingest_trajectory(
        const trajectory_chunk& _chunk, const std::vector<std::string>& _outbox_sinks);

    [[nodiscard]] status vqec_vision_ai_stor_stsql_ingest_association(
        const track_association_revision& _association);

    [[nodiscard]] status vqec_vision_ai_stor_stsql_ingest_episode(
        const event_episode_revision& _episode,
        const std::vector<std::string>& _outbox_sinks);

    [[nodiscard]] status vqec_vision_ai_stor_stsql_ingest_aggregate_contribution(
        const aggregate_contribution_revision& _contribution,
        const std::vector<std::string>& _outbox_sinks);

    [[nodiscard]] status vqec_vision_ai_stor_stsql_query(
        const spatiotemporal_query& _query, spatiotemporal_query_page& _page);

    [[nodiscard]] status vqec_vision_ai_stor_stsql_seal_before(
        std::uint64_t _source_pts_ns);

    [[nodiscard]] status vqec_vision_ai_stor_stsql_get_stats(
        spatiotemporal_store_stats& _stats) const;

private:
    [[nodiscard]] status vqec_vision_ai_stor_stsql_recover_index();
    [[nodiscard]] status vqec_vision_ai_stor_stsql_query_projection(
        const spatiotemporal_query& _query, std::uint64_t _snapshot_sequence,
        spatiotemporal_query_page& _page);

    spatiotemporal_store_config config_;
    sqlite3* catalog_{nullptr};
    mutable spatiotemporal_store_stats stats_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_ADAPTERS_STORAGE_SPATIOTEMPORAL_STORE_HPP
