#ifndef VQEC_VISION_AI_ADAPTERS_STORAGE_SPATIOTEMPORAL_STORE_HPP
#define VQEC_VISION_AI_ADAPTERS_STORAGE_SPATIOTEMPORAL_STORE_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
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
    std::uint64_t acknowledged_outbox_records_{0};
    std::uint64_t retired_shards_{0};
};

enum class metadata_outbox_record_family : std::uint8_t {
    trajectory = 1,
    episode,
    aggregate_contribution
};

struct metadata_outbox_receipt {
    std::string sink_id_;
    metadata_outbox_record_family record_family_{metadata_outbox_record_family::trajectory};
    std::string record_id_;
    std::uint64_t revision_{0};
};

struct spatiotemporal_retention_policy {
    std::int64_t trajectory_before_ns_{0};
    std::int64_t episode_before_ns_{0};
    std::int64_t contribution_before_ns_{0};
    std::int64_t rollup_before_ns_{0};
};

struct spatiotemporal_retention_report {
    std::uint64_t retired_shards_{0};
    std::uint64_t blocked_shards_{0};
    std::uint64_t purged_episode_revisions_{0};
    std::uint64_t purged_contribution_revisions_{0};
    std::uint64_t purged_rollups_{0};
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

    // Commits one bounded projection batch and all outbox rows atomically. The caller retains
    // record order within each revision chain and must keep trajectory chunks in their detail
    // shard path rather than mixing them into this catalog transaction.
    [[nodiscard]] status vqec_vision_ai_stor_stsql_ingest_projection_batch(
        const std::vector<event_episode_revision>& _episodes,
        const std::vector<aggregate_contribution_revision>& _contributions,
        const std::vector<std::string>& _outbox_sinks);

    [[nodiscard]] status vqec_vision_ai_stor_stsql_query(
        const spatiotemporal_query& _query, spatiotemporal_query_page& _page);

    [[nodiscard]] status vqec_vision_ai_stor_stsql_seal_before(
        std::uint64_t _source_pts_ns);

    // A receipt means the named sink durably accepted the exact immutable record. Retention
    // never treats an attempted delivery as acknowledgement.
    [[nodiscard]] status vqec_vision_ai_stor_stsql_acknowledge_outbox(
        const metadata_outbox_receipt& _receipt);

    [[nodiscard]] status vqec_vision_ai_stor_stsql_apply_retention(
        const spatiotemporal_retention_policy& _policy,
        spatiotemporal_retention_report& _report);

    // Thread-safe cooperative cancellation for the single currently executing history query.
    void vqec_vision_ai_stor_stsql_cancel_query() noexcept;
    void vqec_vision_ai_stor_stsql_clear_query_cancellation() noexcept;

    [[nodiscard]] status vqec_vision_ai_stor_stsql_get_stats(
        spatiotemporal_store_stats& _stats) const;

private:
    [[nodiscard]] status vqec_vision_ai_stor_stsql_recover_index();
    [[nodiscard]] status vqec_vision_ai_stor_stsql_query_projection(
        const spatiotemporal_query& _query, std::uint64_t _snapshot_sequence,
        spatiotemporal_query_page& _page);

    spatiotemporal_store_config config_;
    sqlite3* catalog_{nullptr};
    std::map<std::string, sqlite3*> detail_writers_;
    mutable spatiotemporal_store_stats stats_;
    std::atomic<bool> query_cancel_requested_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_ADAPTERS_STORAGE_SPATIOTEMPORAL_STORE_HPP
