#ifndef VQEC_VISION_AI_ADAPTERS_STORAGE_SQLITE_METADATA_STORE_HPP
#define VQEC_VISION_AI_ADAPTERS_STORAGE_SQLITE_METADATA_STORE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/output/vqec_vision_metadata_query.hpp"

struct sqlite3;

namespace vqec::vision::ai {

struct sqlite_metadata_store_config {
    std::string database_path_;
    std::size_t max_payload_bytes_{0};
    std::size_t max_page_size_{0};
    std::uint32_t busy_timeout_ms_{0};
    std::uint32_t wal_autocheckpoint_pages_{0};
    bool is_full_sync_{true};
};

class sqlite_metadata_store final {
public:
    explicit sqlite_metadata_store(sqlite_metadata_store_config _config);
    ~sqlite_metadata_store() noexcept;

    sqlite_metadata_store(const sqlite_metadata_store&) = delete;
    sqlite_metadata_store& operator=(const sqlite_metadata_store&) = delete;

    [[nodiscard]] status vqec_vision_ai_stor_mdsql_open();
    [[nodiscard]] status vqec_vision_ai_stor_mdsql_close() noexcept;

    // Single-writer cold path. A successful return is a SQLite commit receipt, not a
    // Kafka/archive receipt. Duplicate identical record revisions are idempotent.
    [[nodiscard]] status vqec_vision_ai_stor_mdsql_ingest_record(
        const metadata_record& _record, const std::vector<std::string>& _outbox_sinks);

    // Snapshot/keyset query. This method is blocking and must run outside frame workers.
    [[nodiscard]] status vqec_vision_ai_stor_mdsql_query_records(
        const metadata_query_request& _request, metadata_query_page& _page);

    [[nodiscard]] status vqec_vision_ai_stor_mdsql_get_outbox_size(
        const std::string& _sink_id, std::uint64_t& _count) const;

private:
    sqlite_metadata_store_config config_;
    sqlite3* database_{nullptr};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_ADAPTERS_STORAGE_SQLITE_METADATA_STORE_HPP
