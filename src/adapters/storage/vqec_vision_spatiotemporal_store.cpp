#include "vqec_vision_spatiotemporal_store.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <limits>
#include <map>
#include <new>
#include <sstream>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_trajectory_codec.hpp"

namespace vqec::vision::ai {
namespace {

constexpr const char* g_spatiotemporal_catalog_filename = "catalog.db";
constexpr const char* g_spatiotemporal_shard_prefix = "detail_";
constexpr const char* g_spatiotemporal_shard_suffix = ".db";
constexpr const char* g_spatiotemporal_shard_active = "active";
constexpr const char* g_spatiotemporal_shard_sealed = "sealed";
constexpr const char* g_spatiotemporal_shard_missing = "missing";
constexpr const char* g_spatiotemporal_shard_retiring = "retiring";
constexpr const char* g_spatiotemporal_shard_retired = "retired";
constexpr const char* g_spatiotemporal_outbox_pending = "pending";
constexpr const char* g_spatiotemporal_outbox_acknowledged = "acknowledged";
constexpr const char* g_spatiotemporal_sequence_key = "global_sequence";

constexpr const char* g_spatiotemporal_catalog_schema_sql = R"sql(
CREATE TABLE IF NOT EXISTS store_state (
    key TEXT PRIMARY KEY,
    value INTEGER NOT NULL
);
INSERT OR IGNORE INTO store_state(key,value) VALUES('global_sequence',0);
CREATE TABLE IF NOT EXISTS shard_manifests (
    shard_id TEXT PRIMARY KEY,
    relative_path TEXT NOT NULL UNIQUE,
    bucket_begin_ns INTEGER NOT NULL,
    bucket_end_ns INTEGER NOT NULL,
    generation INTEGER NOT NULL,
    state TEXT NOT NULL,
    committed_bytes INTEGER NOT NULL,
    UNIQUE(bucket_begin_ns,generation)
);
CREATE INDEX IF NOT EXISTS shard_manifest_time_idx
    ON shard_manifests(bucket_begin_ns,bucket_end_ns,state);
CREATE TABLE IF NOT EXISTS chunk_index (
    chunk_id TEXT PRIMARY KEY,
    global_sequence INTEGER NOT NULL UNIQUE,
    shard_id TEXT NOT NULL,
    source_id TEXT NOT NULL,
    subject_ref TEXT NOT NULL,
    entity_category TEXT NOT NULL,
    begin_ns INTEGER NOT NULL,
    end_ns INTEGER NOT NULL,
    resolution INTEGER NOT NULL,
    required_access_mask INTEGER NOT NULL,
    bounds_left INTEGER NOT NULL,
    bounds_top INTEGER NOT NULL,
    bounds_right INTEGER NOT NULL,
    bounds_bottom INTEGER NOT NULL,
    encoded_bytes INTEGER NOT NULL,
    checksum_crc32 INTEGER NOT NULL,
    FOREIGN KEY(shard_id) REFERENCES shard_manifests(shard_id)
);
CREATE INDEX IF NOT EXISTS chunk_index_source_time_idx
    ON chunk_index(source_id,begin_ns,end_ns,global_sequence);
CREATE INDEX IF NOT EXISTS chunk_index_subject_time_idx
    ON chunk_index(subject_ref,begin_ns,end_ns,global_sequence);
CREATE TABLE IF NOT EXISTS association_revisions (
    global_sequence INTEGER PRIMARY KEY,
    association_id TEXT NOT NULL,
    revision INTEGER NOT NULL,
    supersedes_revision INTEGER NOT NULL,
    entity_id TEXT NOT NULL,
    left_chunk_id TEXT NOT NULL,
    right_chunk_id TEXT NOT NULL,
    method_revision TEXT NOT NULL,
    topology_path TEXT NOT NULL,
    score_ppm INTEGER NOT NULL,
    minimum_travel_ns INTEGER NOT NULL,
    maximum_travel_ns INTEGER NOT NULL,
    clock_uncertainty_ns INTEGER NOT NULL,
    review_state INTEGER NOT NULL,
    recorded_ns INTEGER NOT NULL,
    UNIQUE(association_id,revision)
);
CREATE INDEX IF NOT EXISTS association_entity_time_idx
    ON association_revisions(entity_id,recorded_ns,global_sequence);
CREATE TABLE IF NOT EXISTS episode_revisions (
    global_sequence INTEGER PRIMARY KEY,
    episode_id TEXT NOT NULL,
    revision INTEGER NOT NULL,
    supersedes_revision INTEGER NOT NULL,
    source_id TEXT NOT NULL,
    semantic_type TEXT NOT NULL,
    subject_ref TEXT NOT NULL,
    scene_revision TEXT NOT NULL,
    rule_revision TEXT NOT NULL,
    begin_ns INTEGER NOT NULL,
    end_ns INTEGER NOT NULL,
    recorded_ns INTEGER NOT NULL,
    lifecycle INTEGER NOT NULL,
    severity_ppm INTEGER NOT NULL,
    required_access_mask INTEGER NOT NULL,
    claims BLOB NOT NULL,
    evidence_references BLOB NOT NULL,
    canonical_payload BLOB NOT NULL,
    UNIQUE(episode_id,revision)
);
CREATE INDEX IF NOT EXISTS episode_source_time_idx
    ON episode_revisions(source_id,begin_ns,end_ns,global_sequence);
CREATE INDEX IF NOT EXISTS episode_semantic_time_idx
    ON episode_revisions(semantic_type,begin_ns,end_ns,global_sequence);
CREATE TABLE IF NOT EXISTS aggregate_contribution_revisions (
    global_sequence INTEGER PRIMARY KEY,
    contribution_id TEXT NOT NULL,
    revision INTEGER NOT NULL,
    supersedes_revision INTEGER NOT NULL,
    episode_id TEXT NOT NULL,
    source_id TEXT NOT NULL,
    aggregate_definition_id TEXT NOT NULL,
    scene_revision TEXT NOT NULL,
    definition_revision TEXT NOT NULL,
    bucket_begin_ns INTEGER NOT NULL,
    bucket_end_ns INTEGER NOT NULL,
    recorded_ns INTEGER NOT NULL,
    operation INTEGER NOT NULL,
    numerator_microunits INTEGER NOT NULL,
    denominator_microunits INTEGER NOT NULL,
    observed_duration_ns INTEGER NOT NULL,
    expected_duration_ns INTEGER NOT NULL,
    required_access_mask INTEGER NOT NULL,
    dimensions BLOB NOT NULL,
    canonical_payload BLOB NOT NULL,
    UNIQUE(contribution_id,revision)
);
CREATE INDEX IF NOT EXISTS aggregate_definition_time_idx
    ON aggregate_contribution_revisions(
        aggregate_definition_id,bucket_begin_ns,bucket_end_ns,global_sequence);
CREATE TABLE IF NOT EXISTS aggregate_rollups (
    aggregate_definition_id TEXT NOT NULL,
    source_id TEXT NOT NULL,
    scene_revision TEXT NOT NULL,
    definition_revision TEXT NOT NULL,
    bucket_begin_ns INTEGER NOT NULL,
    bucket_end_ns INTEGER NOT NULL,
    required_access_mask INTEGER NOT NULL,
    dimensions BLOB NOT NULL,
    numerator_microunits INTEGER NOT NULL,
    denominator_microunits INTEGER NOT NULL,
    observed_duration_ns INTEGER NOT NULL,
    expected_duration_ns INTEGER NOT NULL,
    contribution_count INTEGER NOT NULL,
    updated_sequence INTEGER NOT NULL,
    PRIMARY KEY(aggregate_definition_id,source_id,scene_revision,definition_revision,
        bucket_begin_ns,bucket_end_ns,required_access_mask,dimensions)
);
CREATE INDEX IF NOT EXISTS aggregate_rollup_time_idx
    ON aggregate_rollups(aggregate_definition_id,bucket_begin_ns,bucket_end_ns);
CREATE TABLE IF NOT EXISTS metadata_outbox (
    sink_id TEXT NOT NULL,
    record_family TEXT NOT NULL,
    record_id TEXT NOT NULL,
    revision INTEGER NOT NULL,
    state TEXT NOT NULL,
    attempt_revision INTEGER NOT NULL,
    PRIMARY KEY(sink_id,record_family,record_id,revision)
);
)sql";

constexpr const char* g_spatiotemporal_detail_schema_sql = R"sql(
CREATE TABLE IF NOT EXISTS trajectory_chunks (
    chunk_id TEXT PRIMARY KEY,
    global_sequence INTEGER NOT NULL UNIQUE,
    device_id TEXT NOT NULL,
    source_id TEXT NOT NULL,
    boot_id TEXT NOT NULL,
    source_epoch INTEGER NOT NULL,
    local_track_id INTEGER NOT NULL,
    subject_ref TEXT NOT NULL,
    entity_category TEXT NOT NULL,
    chunk_sequence INTEGER NOT NULL,
    first_frame_id INTEGER NOT NULL,
    first_pts_ns INTEGER NOT NULL,
    last_frame_id INTEGER NOT NULL,
    last_pts_ns INTEGER NOT NULL,
    first_has_utc INTEGER NOT NULL,
    first_utc_ns INTEGER NOT NULL,
    last_has_utc INTEGER NOT NULL,
    last_utc_ns INTEGER NOT NULL,
    clock_uncertainty_ns INTEGER NOT NULL,
    clock_mapping_revision TEXT NOT NULL,
    scene_revision TEXT NOT NULL,
    coordinate_revision TEXT NOT NULL,
    model_revision TEXT NOT NULL,
    tracker_revision TEXT NOT NULL,
    first_media_reference TEXT NOT NULL,
    last_media_reference TEXT NOT NULL,
    coordinate_space INTEGER NOT NULL,
    anchor INTEGER NOT NULL,
    resolution INTEGER NOT NULL,
    sample_mode INTEGER NOT NULL,
    max_spatial_error_units INTEGER NOT NULL,
    max_time_error_ns INTEGER NOT NULL,
    required_access_mask INTEGER NOT NULL,
    bounds_left INTEGER NOT NULL,
    bounds_top INTEGER NOT NULL,
    bounds_right INTEGER NOT NULL,
    bounds_bottom INTEGER NOT NULL,
    checksum_crc32 INTEGER NOT NULL,
    encoded_points BLOB NOT NULL
);
CREATE INDEX IF NOT EXISTS trajectory_source_time_idx
    ON trajectory_chunks(source_id,first_pts_ns,last_pts_ns,global_sequence);
CREATE INDEX IF NOT EXISTS trajectory_track_time_idx
    ON trajectory_chunks(device_id,source_id,boot_id,source_epoch,local_track_id,chunk_sequence);
CREATE TABLE IF NOT EXISTS trajectory_outbox (
    sink_id TEXT NOT NULL,
    chunk_id TEXT NOT NULL,
    state TEXT NOT NULL,
    attempt_revision INTEGER NOT NULL,
    PRIMARY KEY(sink_id,chunk_id)
);
)sql";

class sqlite_statement final {
public:
    sqlite_statement() = default;
    ~sqlite_statement() noexcept {
        if (statement_ != nullptr) {
            sqlite3_finalize(statement_);
        }
    }
    sqlite_statement(const sqlite_statement&) = delete;
    sqlite_statement& operator=(const sqlite_statement&) = delete;

    sqlite3_stmt** vqec_vision_ai_stor_stsql_put() noexcept {
        return &statement_;
    }

    sqlite3_stmt* vqec_vision_ai_stor_stsql_get() const noexcept {
        return statement_;
    }

private:
    sqlite3_stmt* statement_{nullptr};
};

class sqlite_connection_map final {
public:
    ~sqlite_connection_map() noexcept {
        for (auto& entry : connections_) {
            if (entry.second != nullptr) {
                sqlite3_close(entry.second);
            }
        }
    }

    sqlite_connection_map(const sqlite_connection_map&) = delete;
    sqlite_connection_map& operator=(const sqlite_connection_map&) = delete;
    sqlite_connection_map() = default;

    sqlite3* vqec_vision_ai_stor_stsql_get_connection(
        const std::string& _path, std::uint32_t _busy_timeout_ms) {
        const auto existing = connections_.find(_path);
        if (existing != connections_.end()) {
            return existing->second;
        }
        sqlite3* database = nullptr;
        if (sqlite3_open_v2(_path.c_str(), &database,
                SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr) != SQLITE_OK ||
            sqlite3_busy_timeout(database, static_cast<int>(_busy_timeout_ms)) != SQLITE_OK) {
            if (database != nullptr) {
                sqlite3_close(database);
            }
            return nullptr;
        }
        connections_.emplace(_path, database);
        return database;
    }

private:
    std::map<std::string, sqlite3*> connections_;
};

status vqec_vision_ai_stor_stsql_make_error(sqlite3* _database, const char* _operation) {
    std::ostringstream message;
    message << _operation << ": "
            << (_database == nullptr ? "sqlite database unavailable" : sqlite3_errmsg(_database));
    return {status_code::io_error, message.str()};
}

status vqec_vision_ai_stor_stsql_execute(sqlite3* _database, const char* _sql) {
    char* error_message = nullptr;
    const auto result = sqlite3_exec(_database, _sql, nullptr, nullptr, &error_message);
    if (result == SQLITE_OK) {
        return {};
    }
    std::string message = error_message == nullptr ? "sqlite execution failed" : error_message;
    sqlite3_free(error_message);
    return {status_code::io_error, message};
}

status vqec_vision_ai_stor_stsql_prepare(
    sqlite3* _database, const std::string& _sql, sqlite_statement& _statement) {
    if (sqlite3_prepare_v2(_database, _sql.c_str(), -1,
            _statement.vqec_vision_ai_stor_stsql_put(), nullptr) != SQLITE_OK) {
        return vqec_vision_ai_stor_stsql_make_error(_database, "prepare spatiotemporal statement");
    }
    return {};
}

bool vqec_vision_ai_stor_stsql_bind_text(
    sqlite3_stmt* _statement, int _index, const std::string& _value) {
    return sqlite3_bind_text(_statement, _index, _value.c_str(),
               static_cast<int>(_value.size()), SQLITE_TRANSIENT) == SQLITE_OK;
}

std::string vqec_vision_ai_stor_stsql_read_text(sqlite3_stmt* _statement, int _column) {
    const auto* text = sqlite3_column_text(_statement, _column);
    return text == nullptr ? std::string{} :
                             std::string(reinterpret_cast<const char*>(text));
}

status vqec_vision_ai_stor_stsql_configure_database(
    sqlite3* _database, const spatiotemporal_store_config& _config) {
    if (sqlite3_busy_timeout(_database, static_cast<int>(_config.busy_timeout_ms_)) != SQLITE_OK) {
        return vqec_vision_ai_stor_stsql_make_error(_database, "set store busy timeout");
    }
    auto result = vqec_vision_ai_stor_stsql_execute(_database, "PRAGMA journal_mode=WAL;");
    if (result.code_ != status_code::ok) {
        return result;
    }
    result = vqec_vision_ai_stor_stsql_execute(_database,
        _config.is_full_sync_ ? "PRAGMA synchronous=FULL;" : "PRAGMA synchronous=NORMAL;");
    if (result.code_ != status_code::ok) {
        return result;
    }
    const auto checkpoint = std::string("PRAGMA wal_autocheckpoint=") +
        std::to_string(_config.wal_autocheckpoint_pages_) + ";";
    return vqec_vision_ai_stor_stsql_execute(_database, checkpoint.c_str());
}

std::uint64_t vqec_vision_ai_stor_stsql_get_regular_file_bytes(
    const std::filesystem::path& _root) {
    std::error_code error;
    std::uint64_t total = 0U;
    for (std::filesystem::directory_iterator iterator(_root, error), end;
         !error && iterator != end; iterator.increment(error)) {
        if (!iterator->is_regular_file(error) || error) {
            continue;
        }
        const auto bytes = iterator->file_size(error);
        if (!error && bytes <= std::numeric_limits<std::uint64_t>::max() - total) {
            total += bytes;
        }
    }
    return error ? std::numeric_limits<std::uint64_t>::max() : total;
}

std::uint64_t vqec_vision_ai_stor_stsql_get_deadline_now_ns() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

bool vqec_vision_ai_stor_stsql_is_config_valid(const spatiotemporal_store_config& _config) {
    return !_config.root_directory_.empty() && _config.shard_duration_ns_ != 0U &&
        _config.maximum_shard_bytes_ != 0U && _config.maximum_store_bytes_ != 0U &&
        _config.maximum_store_bytes_ > _config.reserve_free_bytes_ &&
        _config.maximum_encoded_chunk_bytes_ != 0U &&
        _config.maximum_query_results_ != 0U && _config.busy_timeout_ms_ != 0U &&
        _config.wal_autocheckpoint_pages_ != 0U &&
        _config.maximum_encoded_chunk_bytes_ <= _config.maximum_shard_bytes_ &&
        _config.busy_timeout_ms_ <= static_cast<std::uint32_t>(std::numeric_limits<int>::max()) &&
        _config.wal_autocheckpoint_pages_ <=
            static_cast<std::uint32_t>(std::numeric_limits<int>::max());
}

status vqec_vision_ai_stor_stsql_open_database(
    const std::string& _path, const spatiotemporal_store_config& _config,
    int _flags, sqlite3*& _database) {
    if (sqlite3_open_v2(_path.c_str(), &_database, _flags, nullptr) != SQLITE_OK) {
        auto result = vqec_vision_ai_stor_stsql_make_error(_database, "open metadata database");
        if (_database != nullptr) {
            sqlite3_close(_database);
            _database = nullptr;
        }
        return result;
    }
    auto result = vqec_vision_ai_stor_stsql_configure_database(_database, _config);
    if (result.code_ != status_code::ok) {
        sqlite3_close(_database);
        _database = nullptr;
    }
    return result;
}

status vqec_vision_ai_stor_stsql_reserve_sequence(
    sqlite3* _catalog, std::uint64_t& _sequence, bool _keep_transaction_open = false) {
    auto result = vqec_vision_ai_stor_stsql_execute(_catalog, "BEGIN IMMEDIATE;");
    if (result.code_ != status_code::ok) {
        return result;
    }
    sqlite_statement statement;
    result = vqec_vision_ai_stor_stsql_prepare(_catalog,
        "UPDATE store_state SET value=value+1 WHERE key='global_sequence' RETURNING value;",
        statement);
    if (result.code_ != status_code::ok ||
        sqlite3_step(statement.vqec_vision_ai_stor_stsql_get()) != SQLITE_ROW) {
        (void)vqec_vision_ai_stor_stsql_execute(_catalog, "ROLLBACK;");
        return result.code_ == status_code::ok
                   ? vqec_vision_ai_stor_stsql_make_error(_catalog, "reserve metadata sequence")
                   : result;
    }
    const auto value = sqlite3_column_int64(statement.vqec_vision_ai_stor_stsql_get(), 0);
    if (value <= 0) {
        (void)vqec_vision_ai_stor_stsql_execute(_catalog, "ROLLBACK;");
        return {status_code::resource_exhausted, "metadata sequence overflow"};
    }
    if (sqlite3_step(statement.vqec_vision_ai_stor_stsql_get()) != SQLITE_DONE) {
        (void)vqec_vision_ai_stor_stsql_execute(_catalog, "ROLLBACK;");
        return vqec_vision_ai_stor_stsql_make_error(_catalog, "finish metadata sequence");
    }
    if (!_keep_transaction_open) {
        result = vqec_vision_ai_stor_stsql_execute(_catalog, "COMMIT;");
    }
    if (result.code_ == status_code::ok) {
        _sequence = static_cast<std::uint64_t>(value);
    }
    return result;
}

struct shard_descriptor {
    std::string shard_id_;
    std::string relative_path_;
    std::uint64_t bucket_begin_ns_{0};
    std::uint64_t bucket_end_ns_{0};
    std::uint64_t generation_{0};
    std::uint64_t committed_bytes_{0};
};

status vqec_vision_ai_stor_stsql_find_or_create_shard(
    sqlite3* _catalog, const spatiotemporal_store_config& _config,
    std::uint64_t _source_pts_ns, std::size_t _encoded_bytes,
    shard_descriptor& _descriptor) {
    const auto bucket_begin =
        (_source_pts_ns / _config.shard_duration_ns_) * _config.shard_duration_ns_;
    if (bucket_begin > std::numeric_limits<std::uint64_t>::max() -
            _config.shard_duration_ns_) {
        return {status_code::invalid_argument, "trajectory shard time overflows"};
    }
    const auto bucket_end = bucket_begin + _config.shard_duration_ns_;
    sqlite_statement current_statement;
    auto result = vqec_vision_ai_stor_stsql_prepare(_catalog,
        "SELECT shard_id,relative_path,generation,committed_bytes FROM shard_manifests "
        "WHERE bucket_begin_ns=?1 AND state='active' ORDER BY generation DESC LIMIT 1;",
        current_statement);
    auto* current = current_statement.vqec_vision_ai_stor_stsql_get();
    if (result.code_ != status_code::ok ||
        sqlite3_bind_int64(current, 1, static_cast<sqlite3_int64>(bucket_begin)) != SQLITE_OK) {
        return result.code_ == status_code::ok
                   ? vqec_vision_ai_stor_stsql_make_error(_catalog, "bind shard bucket")
                   : result;
    }
    std::uint64_t generation = 0U;
    if (sqlite3_step(current) == SQLITE_ROW) {
        _descriptor.shard_id_ = vqec_vision_ai_stor_stsql_read_text(current, 0);
        _descriptor.relative_path_ = vqec_vision_ai_stor_stsql_read_text(current, 1);
        generation = static_cast<std::uint64_t>(sqlite3_column_int64(current, 2));
        _descriptor.committed_bytes_ =
            static_cast<std::uint64_t>(sqlite3_column_int64(current, 3));
        if (_descriptor.committed_bytes_ <= _config.maximum_shard_bytes_ &&
            _encoded_bytes <= _config.maximum_shard_bytes_ - _descriptor.committed_bytes_) {
            _descriptor.bucket_begin_ns_ = bucket_begin;
            _descriptor.bucket_end_ns_ = bucket_end;
            _descriptor.generation_ = generation;
            return {};
        }
        sqlite_statement seal_statement;
        result = vqec_vision_ai_stor_stsql_prepare(_catalog,
            "UPDATE shard_manifests SET state='sealed' WHERE shard_id=?1;", seal_statement);
        if (result.code_ != status_code::ok ||
            !vqec_vision_ai_stor_stsql_bind_text(
                seal_statement.vqec_vision_ai_stor_stsql_get(), 1, _descriptor.shard_id_) ||
            sqlite3_step(seal_statement.vqec_vision_ai_stor_stsql_get()) != SQLITE_DONE) {
            return result.code_ == status_code::ok
                       ? vqec_vision_ai_stor_stsql_make_error(_catalog, "seal full shard")
                       : result;
        }
    } else {
        sqlite_statement generation_statement;
        result = vqec_vision_ai_stor_stsql_prepare(_catalog,
            "SELECT COALESCE(MAX(generation),0) FROM shard_manifests WHERE bucket_begin_ns=?1;",
            generation_statement);
        auto* generation_query = generation_statement.vqec_vision_ai_stor_stsql_get();
        if (result.code_ != status_code::ok ||
            sqlite3_bind_int64(generation_query, 1,
                static_cast<sqlite3_int64>(bucket_begin)) != SQLITE_OK ||
            sqlite3_step(generation_query) != SQLITE_ROW) {
            return result.code_ == status_code::ok
                       ? vqec_vision_ai_stor_stsql_make_error(_catalog, "read shard generation")
                       : result;
        }
        generation = static_cast<std::uint64_t>(sqlite3_column_int64(generation_query, 0));
    }
    if (generation == std::numeric_limits<std::uint64_t>::max()) {
        return {status_code::resource_exhausted, "trajectory shard generation overflows"};
    }
    ++generation;
    _descriptor.bucket_begin_ns_ = bucket_begin;
    _descriptor.bucket_end_ns_ = bucket_end;
    _descriptor.generation_ = generation;
    _descriptor.shard_id_ = "shard." + std::to_string(bucket_begin) + "." +
        std::to_string(generation);
    _descriptor.relative_path_ = std::string(g_spatiotemporal_shard_prefix) +
        std::to_string(bucket_begin) + "_" + std::to_string(generation) +
        g_spatiotemporal_shard_suffix;
    _descriptor.committed_bytes_ = 0U;
    sqlite_statement insert_statement;
    result = vqec_vision_ai_stor_stsql_prepare(_catalog,
        "INSERT INTO shard_manifests(shard_id,relative_path,bucket_begin_ns,bucket_end_ns,"
        "generation,state,committed_bytes) VALUES(?1,?2,?3,?4,?5,'active',0);",
        insert_statement);
    auto* insert = insert_statement.vqec_vision_ai_stor_stsql_get();
    if (result.code_ != status_code::ok ||
        !vqec_vision_ai_stor_stsql_bind_text(insert, 1, _descriptor.shard_id_) ||
        !vqec_vision_ai_stor_stsql_bind_text(insert, 2, _descriptor.relative_path_) ||
        sqlite3_bind_int64(insert, 3, static_cast<sqlite3_int64>(bucket_begin)) != SQLITE_OK ||
        sqlite3_bind_int64(insert, 4, static_cast<sqlite3_int64>(bucket_end)) != SQLITE_OK ||
        sqlite3_bind_int64(insert, 5, static_cast<sqlite3_int64>(generation)) != SQLITE_OK ||
        sqlite3_step(insert) != SQLITE_DONE) {
        return result.code_ == status_code::ok
                   ? vqec_vision_ai_stor_stsql_make_error(_catalog, "insert shard manifest")
                   : result;
    }
    return {};
}

bool vqec_vision_ai_stor_stsql_validate_sinks(const std::vector<std::string>& _sinks) {
    if (_sinks.size() > g_spatiotemporal_max_outbox_sinks) {
        return false;
    }
    std::vector<std::string> sorted = _sinks;
    std::sort(sorted.begin(), sorted.end());
    return std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end() &&
        std::all_of(sorted.begin(), sorted.end(), [](const auto& _sink) {
            return vqec_vision_ai_cntr_ident_is_valid(
                _sink, g_spatiotemporal_max_identifier_bytes);
        });
}

status vqec_vision_ai_stor_stsql_insert_detail(
    sqlite3* _detail, const trajectory_chunk& _chunk,
    const encoded_trajectory_points& _encoded, std::uint64_t _global_sequence,
    const std::vector<std::string>& _outbox_sinks) {
    auto result = vqec_vision_ai_stor_stsql_execute(_detail, "BEGIN IMMEDIATE;");
    if (result.code_ != status_code::ok) {
        return result;
    }
    const auto rollback = [_detail]() {
        (void)vqec_vision_ai_stor_stsql_execute(_detail, "ROLLBACK;");
    };
    constexpr const char* insert_sql =
        "INSERT INTO trajectory_chunks(chunk_id,global_sequence,device_id,source_id,boot_id,"
        "source_epoch,local_track_id,subject_ref,entity_category,chunk_sequence,first_frame_id,"
        "first_pts_ns,last_frame_id,last_pts_ns,first_has_utc,first_utc_ns,last_has_utc,"
        "last_utc_ns,clock_uncertainty_ns,clock_mapping_revision,scene_revision,"
        "coordinate_revision,model_revision,tracker_revision,first_media_reference,"
        "last_media_reference,coordinate_space,anchor,resolution,sample_mode,"
        "max_spatial_error_units,max_time_error_ns,required_access_mask,bounds_left,bounds_top,"
        "bounds_right,bounds_bottom,checksum_crc32,encoded_points) VALUES("
        "?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20,"
        "?21,?22,?23,?24,?25,?26,?27,?28,?29,?30,?31,?32,?33,?34,?35,?36,?37,?38,?39);";
    sqlite_statement statement;
    result = vqec_vision_ai_stor_stsql_prepare(_detail, insert_sql, statement);
    auto* insert = statement.vqec_vision_ai_stor_stsql_get();
    const auto& first = _chunk.first_frame_;
    const auto& last = _chunk.last_frame_;
    const bool is_bound = result.code_ == status_code::ok &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 1, _chunk.chunk_id_) &&
        sqlite3_bind_int64(insert, 2, static_cast<sqlite3_int64>(_global_sequence)) == SQLITE_OK &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 3, _chunk.track_.device_id_) &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 4, _chunk.track_.source_id_) &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 5, _chunk.track_.boot_id_) &&
        sqlite3_bind_int64(insert, 6, static_cast<sqlite3_int64>(_chunk.track_.source_epoch_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 7, static_cast<sqlite3_int64>(_chunk.track_.local_track_id_)) == SQLITE_OK &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 8, _chunk.subject_ref_) &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 9, _chunk.entity_category_) &&
        sqlite3_bind_int64(insert, 10, static_cast<sqlite3_int64>(_chunk.chunk_sequence_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 11, static_cast<sqlite3_int64>(first.frame_id_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 12, static_cast<sqlite3_int64>(first.source_pts_ns_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 13, static_cast<sqlite3_int64>(last.frame_id_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 14, static_cast<sqlite3_int64>(last.source_pts_ns_)) == SQLITE_OK &&
        sqlite3_bind_int(insert, 15, first.has_capture_utc_ ? 1 : 0) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 16, first.capture_utc_ns_) == SQLITE_OK &&
        sqlite3_bind_int(insert, 17, last.has_capture_utc_ ? 1 : 0) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 18, last.capture_utc_ns_) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 19, static_cast<sqlite3_int64>(first.clock_uncertainty_ns_)) == SQLITE_OK &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 20, first.clock_mapping_revision_) &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 21, first.scene_revision_) &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 22, first.coordinate_revision_) &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 23, first.model_revision_) &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 24, first.tracker_revision_) &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 25, first.media_reference_) &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 26, last.media_reference_) &&
        sqlite3_bind_int(insert, 27, static_cast<int>(_chunk.coordinate_space_)) == SQLITE_OK &&
        sqlite3_bind_int(insert, 28, static_cast<int>(_chunk.anchor_)) == SQLITE_OK &&
        sqlite3_bind_int(insert, 29, static_cast<int>(_chunk.resolution_)) == SQLITE_OK &&
        sqlite3_bind_int(insert, 30, static_cast<int>(_chunk.sample_mode_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 31, _chunk.max_spatial_error_units_) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 32, static_cast<sqlite3_int64>(_chunk.max_time_error_ns_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 33, _chunk.required_access_domain_mask_) == SQLITE_OK &&
        sqlite3_bind_int(insert, 34, _chunk.bounds_left_) == SQLITE_OK &&
        sqlite3_bind_int(insert, 35, _chunk.bounds_top_) == SQLITE_OK &&
        sqlite3_bind_int(insert, 36, _chunk.bounds_right_) == SQLITE_OK &&
        sqlite3_bind_int(insert, 37, _chunk.bounds_bottom_) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 38, _encoded.checksum_crc32_) == SQLITE_OK &&
        sqlite3_bind_blob(insert, 39, _encoded.bytes_.data(),
            static_cast<int>(_encoded.bytes_.size()), SQLITE_TRANSIENT) == SQLITE_OK;
    if (!is_bound || sqlite3_step(insert) != SQLITE_DONE) {
        rollback();
        return result.code_ == status_code::ok
                   ? vqec_vision_ai_stor_stsql_make_error(_detail, "insert trajectory chunk")
                   : result;
    }
    for (const auto& sink : _outbox_sinks) {
        sqlite_statement outbox_statement;
        result = vqec_vision_ai_stor_stsql_prepare(_detail,
            "INSERT INTO trajectory_outbox(sink_id,chunk_id,state,attempt_revision) "
            "VALUES(?1,?2,?3,0);", outbox_statement);
        auto* outbox = outbox_statement.vqec_vision_ai_stor_stsql_get();
        if (result.code_ != status_code::ok ||
            !vqec_vision_ai_stor_stsql_bind_text(outbox, 1, sink) ||
            !vqec_vision_ai_stor_stsql_bind_text(outbox, 2, _chunk.chunk_id_) ||
            !vqec_vision_ai_stor_stsql_bind_text(outbox, 3, g_spatiotemporal_outbox_pending) ||
            sqlite3_step(outbox) != SQLITE_DONE) {
            rollback();
            return result.code_ == status_code::ok
                       ? vqec_vision_ai_stor_stsql_make_error(_detail, "insert trajectory outbox")
                       : result;
        }
    }
    return vqec_vision_ai_stor_stsql_execute(_detail, "COMMIT;");
}

status vqec_vision_ai_stor_stsql_insert_chunk_index(
    sqlite3* _catalog, const trajectory_chunk& _chunk,
    const encoded_trajectory_points& _encoded, std::uint64_t _global_sequence,
    const shard_descriptor& _shard) {
    sqlite_statement statement;
    auto result = vqec_vision_ai_stor_stsql_prepare(_catalog,
        "INSERT INTO chunk_index(chunk_id,global_sequence,shard_id,source_id,subject_ref,"
        "entity_category,begin_ns,end_ns,resolution,required_access_mask,bounds_left,bounds_top,"
        "bounds_right,bounds_bottom,encoded_bytes,checksum_crc32) VALUES("
        "?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16);", statement);
    auto* insert = statement.vqec_vision_ai_stor_stsql_get();
    const bool is_bound = result.code_ == status_code::ok &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 1, _chunk.chunk_id_) &&
        sqlite3_bind_int64(insert, 2, static_cast<sqlite3_int64>(_global_sequence)) == SQLITE_OK &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 3, _shard.shard_id_) &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 4, _chunk.track_.source_id_) &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 5, _chunk.subject_ref_) &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 6, _chunk.entity_category_) &&
        sqlite3_bind_int64(insert, 7, static_cast<sqlite3_int64>(_chunk.first_frame_.source_pts_ns_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 8, static_cast<sqlite3_int64>(_chunk.last_frame_.source_pts_ns_ + 1U)) == SQLITE_OK &&
        sqlite3_bind_int(insert, 9, static_cast<int>(_chunk.resolution_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 10, _chunk.required_access_domain_mask_) == SQLITE_OK &&
        sqlite3_bind_int(insert, 11, _chunk.bounds_left_) == SQLITE_OK &&
        sqlite3_bind_int(insert, 12, _chunk.bounds_top_) == SQLITE_OK &&
        sqlite3_bind_int(insert, 13, _chunk.bounds_right_) == SQLITE_OK &&
        sqlite3_bind_int(insert, 14, _chunk.bounds_bottom_) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 15, static_cast<sqlite3_int64>(_encoded.bytes_.size())) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 16, _encoded.checksum_crc32_) == SQLITE_OK;
    if (!is_bound || sqlite3_step(insert) != SQLITE_DONE) {
        return result.code_ == status_code::ok
                   ? vqec_vision_ai_stor_stsql_make_error(_catalog, "insert trajectory index")
                   : result;
    }
    sqlite_statement update_statement;
    result = vqec_vision_ai_stor_stsql_prepare(_catalog,
        "UPDATE shard_manifests SET committed_bytes=committed_bytes+?1 WHERE shard_id=?2;",
        update_statement);
    auto* update = update_statement.vqec_vision_ai_stor_stsql_get();
    if (result.code_ != status_code::ok ||
        sqlite3_bind_int64(update, 1, static_cast<sqlite3_int64>(_encoded.bytes_.size())) != SQLITE_OK ||
        !vqec_vision_ai_stor_stsql_bind_text(update, 2, _shard.shard_id_) ||
        sqlite3_step(update) != SQLITE_DONE) {
        return result.code_ == status_code::ok
                   ? vqec_vision_ai_stor_stsql_make_error(_catalog, "update shard bytes")
                   : result;
    }
    return {};
}

trajectory_chunk vqec_vision_ai_stor_stsql_read_chunk_header(sqlite3_stmt* _statement) {
    trajectory_chunk chunk;
    chunk.chunk_id_ = vqec_vision_ai_stor_stsql_read_text(_statement, 0);
    chunk.track_.device_id_ = vqec_vision_ai_stor_stsql_read_text(_statement, 2);
    chunk.track_.source_id_ = vqec_vision_ai_stor_stsql_read_text(_statement, 3);
    chunk.track_.boot_id_ = vqec_vision_ai_stor_stsql_read_text(_statement, 4);
    chunk.track_.source_epoch_ = static_cast<std::uint64_t>(sqlite3_column_int64(_statement, 5));
    chunk.track_.local_track_id_ = static_cast<std::uint64_t>(sqlite3_column_int64(_statement, 6));
    chunk.subject_ref_ = vqec_vision_ai_stor_stsql_read_text(_statement, 7);
    chunk.entity_category_ = vqec_vision_ai_stor_stsql_read_text(_statement, 8);
    chunk.chunk_sequence_ = static_cast<std::uint64_t>(sqlite3_column_int64(_statement, 9));
    chunk.first_frame_.device_id_ = chunk.track_.device_id_;
    chunk.first_frame_.source_id_ = chunk.track_.source_id_;
    chunk.first_frame_.boot_id_ = chunk.track_.boot_id_;
    chunk.first_frame_.source_epoch_ = chunk.track_.source_epoch_;
    chunk.last_frame_ = chunk.first_frame_;
    chunk.first_frame_.frame_id_ = static_cast<std::uint64_t>(sqlite3_column_int64(_statement, 10));
    chunk.first_frame_.source_pts_ns_ = static_cast<std::uint64_t>(sqlite3_column_int64(_statement, 11));
    chunk.last_frame_.frame_id_ = static_cast<std::uint64_t>(sqlite3_column_int64(_statement, 12));
    chunk.last_frame_.source_pts_ns_ = static_cast<std::uint64_t>(sqlite3_column_int64(_statement, 13));
    chunk.first_frame_.has_capture_utc_ = sqlite3_column_int(_statement, 14) != 0;
    chunk.first_frame_.capture_utc_ns_ = sqlite3_column_int64(_statement, 15);
    chunk.last_frame_.has_capture_utc_ = sqlite3_column_int(_statement, 16) != 0;
    chunk.last_frame_.capture_utc_ns_ = sqlite3_column_int64(_statement, 17);
    chunk.first_frame_.clock_uncertainty_ns_ = static_cast<std::uint64_t>(sqlite3_column_int64(_statement, 18));
    chunk.last_frame_.clock_uncertainty_ns_ = chunk.first_frame_.clock_uncertainty_ns_;
    chunk.first_frame_.clock_mapping_revision_ = vqec_vision_ai_stor_stsql_read_text(_statement, 19);
    chunk.last_frame_.clock_mapping_revision_ = chunk.first_frame_.clock_mapping_revision_;
    chunk.first_frame_.scene_revision_ = vqec_vision_ai_stor_stsql_read_text(_statement, 20);
    chunk.last_frame_.scene_revision_ = chunk.first_frame_.scene_revision_;
    chunk.first_frame_.coordinate_revision_ = vqec_vision_ai_stor_stsql_read_text(_statement, 21);
    chunk.last_frame_.coordinate_revision_ = chunk.first_frame_.coordinate_revision_;
    chunk.first_frame_.model_revision_ = vqec_vision_ai_stor_stsql_read_text(_statement, 22);
    chunk.last_frame_.model_revision_ = chunk.first_frame_.model_revision_;
    chunk.first_frame_.tracker_revision_ = vqec_vision_ai_stor_stsql_read_text(_statement, 23);
    chunk.last_frame_.tracker_revision_ = chunk.first_frame_.tracker_revision_;
    chunk.first_frame_.media_reference_ = vqec_vision_ai_stor_stsql_read_text(_statement, 24);
    chunk.last_frame_.media_reference_ = vqec_vision_ai_stor_stsql_read_text(_statement, 25);
    chunk.coordinate_space_ = static_cast<spatiotemporal_coordinate_space>(sqlite3_column_int(_statement, 26));
    chunk.anchor_ = static_cast<spatiotemporal_anchor>(sqlite3_column_int(_statement, 27));
    chunk.resolution_ = static_cast<trajectory_resolution>(sqlite3_column_int(_statement, 28));
    chunk.sample_mode_ = static_cast<trajectory_sample_mode>(sqlite3_column_int(_statement, 29));
    chunk.max_spatial_error_units_ = static_cast<std::uint32_t>(sqlite3_column_int64(_statement, 30));
    chunk.max_time_error_ns_ = static_cast<std::uint64_t>(sqlite3_column_int64(_statement, 31));
    chunk.required_access_domain_mask_ = static_cast<std::uint32_t>(sqlite3_column_int64(_statement, 32));
    chunk.bounds_left_ = sqlite3_column_int(_statement, 33);
    chunk.bounds_top_ = sqlite3_column_int(_statement, 34);
    chunk.bounds_right_ = sqlite3_column_int(_statement, 35);
    chunk.bounds_bottom_ = sqlite3_column_int(_statement, 36);
    return chunk;
}

constexpr const char* g_spatiotemporal_detail_select =
    "chunk_id,global_sequence,device_id,source_id,boot_id,source_epoch,local_track_id,"
    "subject_ref,entity_category,chunk_sequence,first_frame_id,first_pts_ns,last_frame_id,"
    "last_pts_ns,first_has_utc,first_utc_ns,last_has_utc,last_utc_ns,clock_uncertainty_ns,"
    "clock_mapping_revision,scene_revision,coordinate_revision,model_revision,tracker_revision,"
    "first_media_reference,last_media_reference,coordinate_space,anchor,resolution,sample_mode,"
    "max_spatial_error_units,max_time_error_ns,required_access_mask,bounds_left,bounds_top,"
    "bounds_right,bounds_bottom,checksum_crc32,encoded_points";

status vqec_vision_ai_stor_stsql_load_chunk(
    sqlite3* _detail, const std::string& _chunk_id, std::size_t _maximum_encoded_bytes,
    trajectory_chunk& _chunk, std::size_t& _encoded_bytes) {
    sqlite_statement statement;
    auto result = vqec_vision_ai_stor_stsql_prepare(_detail,
        std::string("SELECT ") + g_spatiotemporal_detail_select +
            " FROM trajectory_chunks WHERE chunk_id=?1;", statement);
    auto* query = statement.vqec_vision_ai_stor_stsql_get();
    if (result.code_ != status_code::ok ||
        !vqec_vision_ai_stor_stsql_bind_text(query, 1, _chunk_id)) {
        return result.code_ == status_code::ok
                   ? vqec_vision_ai_stor_stsql_make_error(_detail, "bind trajectory chunk")
                   : result;
    }
    if (sqlite3_step(query) != SQLITE_ROW) {
        return {status_code::io_error, "trajectory chunk index points to missing detail"};
    }
    auto chunk = vqec_vision_ai_stor_stsql_read_chunk_header(query);
    const auto checksum = static_cast<std::uint32_t>(sqlite3_column_int64(query, 37));
    const auto blob_bytes = sqlite3_column_bytes(query, 38);
    const auto* blob = static_cast<const std::uint8_t*>(sqlite3_column_blob(query, 38));
    if (blob == nullptr || blob_bytes <= 0 ||
        static_cast<std::size_t>(blob_bytes) > _maximum_encoded_bytes) {
        return {status_code::protocol_error, "trajectory detail blob is invalid"};
    }
    encoded_trajectory_points encoded;
    encoded.checksum_crc32_ = checksum;
    encoded.bytes_.assign(blob, blob + blob_bytes);
    result = vqec_vision_ai_cntr_trcod_decode_points(encoded, _maximum_encoded_bytes,
        g_spatiotemporal_max_points_per_chunk, chunk.points_);
    if (result.code_ != status_code::ok) {
        return result;
    }
    result = vqec_vision_ai_cntr_stmet_validate_trajectory_chunk(chunk);
    if (result.code_ != status_code::ok) {
        return {status_code::protocol_error, "persisted trajectory contract is invalid"};
    }
    _encoded_bytes = static_cast<std::size_t>(blob_bytes);
    _chunk = std::move(chunk);
    return {};
}

bool vqec_vision_ai_stor_stsql_point_inside(
    const trajectory_point& _point, const spatiotemporal_query& _query) {
    return _point.anchor_x_ >= _query.bounds_left_ &&
           _point.anchor_x_ <= _query.bounds_right_ &&
           _point.anchor_y_ >= _query.bounds_top_ &&
           _point.anchor_y_ <= _query.bounds_bottom_;
}

long double vqec_vision_ai_stor_stsql_cross(
    std::int32_t _ax, std::int32_t _ay, std::int32_t _bx, std::int32_t _by,
    std::int32_t _cx, std::int32_t _cy) {
    return static_cast<long double>(_bx - _ax) * static_cast<long double>(_cy - _ay) -
        static_cast<long double>(_by - _ay) * static_cast<long double>(_cx - _ax);
}

bool vqec_vision_ai_stor_stsql_segments_intersect(
    std::int32_t _ax, std::int32_t _ay, std::int32_t _bx, std::int32_t _by,
    std::int32_t _cx, std::int32_t _cy, std::int32_t _dx, std::int32_t _dy) {
    const auto first = vqec_vision_ai_stor_stsql_cross(_ax, _ay, _bx, _by, _cx, _cy);
    const auto second = vqec_vision_ai_stor_stsql_cross(_ax, _ay, _bx, _by, _dx, _dy);
    const auto third = vqec_vision_ai_stor_stsql_cross(_cx, _cy, _dx, _dy, _ax, _ay);
    const auto fourth = vqec_vision_ai_stor_stsql_cross(_cx, _cy, _dx, _dy, _bx, _by);
    return ((first <= 0.0L && second >= 0.0L) || (first >= 0.0L && second <= 0.0L)) &&
        ((third <= 0.0L && fourth >= 0.0L) || (third >= 0.0L && fourth <= 0.0L));
}

bool vqec_vision_ai_stor_stsql_path_intersects(
    const trajectory_chunk& _chunk, const spatiotemporal_query& _query) {
    for (const auto& point : _chunk.points_) {
        if (vqec_vision_ai_stor_stsql_point_inside(point, _query)) {
            return true;
        }
    }
    for (std::size_t index = 1U; index < _chunk.points_.size(); ++index) {
        const auto& first = _chunk.points_[index - 1U];
        const auto& second = _chunk.points_[index];
        if ((second.flags_ & static_cast<std::uint32_t>(trajectory_point_flag::gap_before)) != 0U) {
            continue;
        }
        if (vqec_vision_ai_stor_stsql_segments_intersect(first.anchor_x_, first.anchor_y_,
                second.anchor_x_, second.anchor_y_, _query.bounds_left_, _query.bounds_top_,
                _query.bounds_right_, _query.bounds_top_) ||
            vqec_vision_ai_stor_stsql_segments_intersect(first.anchor_x_, first.anchor_y_,
                second.anchor_x_, second.anchor_y_, _query.bounds_right_, _query.bounds_top_,
                _query.bounds_right_, _query.bounds_bottom_) ||
            vqec_vision_ai_stor_stsql_segments_intersect(first.anchor_x_, first.anchor_y_,
                second.anchor_x_, second.anchor_y_, _query.bounds_right_, _query.bounds_bottom_,
                _query.bounds_left_, _query.bounds_bottom_) ||
            vqec_vision_ai_stor_stsql_segments_intersect(first.anchor_x_, first.anchor_y_,
                second.anchor_x_, second.anchor_y_, _query.bounds_left_, _query.bounds_bottom_,
                _query.bounds_left_, _query.bounds_top_)) {
            return true;
        }
    }
    return false;
}

bool vqec_vision_ai_stor_stsql_matches_exact_spatial(
    const trajectory_chunk& _chunk, const spatiotemporal_query& _query) {
    if (!_query.has_spatial_bounds_ ||
        _query.spatial_relation_ == spatiotemporal_spatial_relation::bounds_intersect) {
        return true;
    }
    if (_query.spatial_relation_ == spatiotemporal_spatial_relation::path_intersects) {
        return vqec_vision_ai_stor_stsql_path_intersects(_chunk, _query);
    }
    if (_query.spatial_relation_ == spatiotemporal_spatial_relation::inside) {
        return std::all_of(_chunk.points_.begin(), _chunk.points_.end(),
            [&_query](const auto& _point) {
                return vqec_vision_ai_stor_stsql_point_inside(_point, _query);
            });
    }
    return false;
}

struct chunk_candidate {
    std::uint64_t sequence_{0};
    std::string chunk_id_;
    std::string relative_path_;
    std::size_t encoded_bytes_{0};
    trajectory_resolution resolution_{trajectory_resolution::trajectory_bounded};
};

bool vqec_vision_ai_stor_stsql_frame_locators_match(
    const spatiotemporal_frame_locator& _left,
    const spatiotemporal_frame_locator& _right) {
    return _left.device_id_ == _right.device_id_ &&
        _left.source_id_ == _right.source_id_ && _left.boot_id_ == _right.boot_id_ &&
        _left.source_epoch_ == _right.source_epoch_ && _left.frame_id_ == _right.frame_id_ &&
        _left.source_pts_ns_ == _right.source_pts_ns_ &&
        _left.has_capture_utc_ == _right.has_capture_utc_ &&
        _left.capture_utc_ns_ == _right.capture_utc_ns_ &&
        _left.clock_uncertainty_ns_ == _right.clock_uncertainty_ns_ &&
        _left.clock_mapping_revision_ == _right.clock_mapping_revision_ &&
        _left.scene_revision_ == _right.scene_revision_ &&
        _left.coordinate_revision_ == _right.coordinate_revision_ &&
        _left.model_revision_ == _right.model_revision_ &&
        _left.tracker_revision_ == _right.tracker_revision_ &&
        _left.media_reference_ == _right.media_reference_;
}

bool vqec_vision_ai_stor_stsql_points_match(
    const trajectory_point& _left, const trajectory_point& _right) {
    return _left.frame_id_ == _right.frame_id_ &&
        _left.source_pts_ns_ == _right.source_pts_ns_ &&
        _left.has_capture_utc_ == _right.has_capture_utc_ &&
        _left.capture_utc_ns_ == _right.capture_utc_ns_ &&
        _left.anchor_x_ == _right.anchor_x_ && _left.anchor_y_ == _right.anchor_y_ &&
        _left.box_left_ == _right.box_left_ && _left.box_top_ == _right.box_top_ &&
        _left.box_right_ == _right.box_right_ && _left.box_bottom_ == _right.box_bottom_ &&
        _left.flags_ == _right.flags_;
}

bool vqec_vision_ai_stor_stsql_chunks_match(
    const trajectory_chunk& _left, const trajectory_chunk& _right) {
    return _left.chunk_id_ == _right.chunk_id_ &&
        _left.track_.device_id_ == _right.track_.device_id_ &&
        _left.track_.source_id_ == _right.track_.source_id_ &&
        _left.track_.boot_id_ == _right.track_.boot_id_ &&
        _left.track_.source_epoch_ == _right.track_.source_epoch_ &&
        _left.track_.local_track_id_ == _right.track_.local_track_id_ &&
        _left.subject_ref_ == _right.subject_ref_ &&
        _left.entity_category_ == _right.entity_category_ &&
        _left.chunk_sequence_ == _right.chunk_sequence_ &&
        vqec_vision_ai_stor_stsql_frame_locators_match(
            _left.first_frame_, _right.first_frame_) &&
        vqec_vision_ai_stor_stsql_frame_locators_match(
            _left.last_frame_, _right.last_frame_) &&
        _left.coordinate_space_ == _right.coordinate_space_ &&
        _left.anchor_ == _right.anchor_ && _left.resolution_ == _right.resolution_ &&
        _left.sample_mode_ == _right.sample_mode_ &&
        _left.max_spatial_error_units_ == _right.max_spatial_error_units_ &&
        _left.max_time_error_ns_ == _right.max_time_error_ns_ &&
        _left.required_access_domain_mask_ == _right.required_access_domain_mask_ &&
        _left.bounds_left_ == _right.bounds_left_ &&
        _left.bounds_top_ == _right.bounds_top_ &&
        _left.bounds_right_ == _right.bounds_right_ &&
        _left.bounds_bottom_ == _right.bounds_bottom_ &&
        _left.points_.size() == _right.points_.size() &&
        std::equal(_left.points_.begin(), _left.points_.end(), _right.points_.begin(),
            vqec_vision_ai_stor_stsql_points_match);
}

bool vqec_vision_ai_stor_stsql_association_matches_row(
    const track_association_revision& _association, sqlite3_stmt* _row) {
    return _association.revision_ ==
            static_cast<std::uint64_t>(sqlite3_column_int64(_row, 0)) &&
        _association.supersedes_revision_ ==
            static_cast<std::uint64_t>(sqlite3_column_int64(_row, 1)) &&
        _association.entity_id_ == vqec_vision_ai_stor_stsql_read_text(_row, 2) &&
        _association.left_chunk_id_ == vqec_vision_ai_stor_stsql_read_text(_row, 3) &&
        _association.right_chunk_id_ == vqec_vision_ai_stor_stsql_read_text(_row, 4) &&
        _association.method_revision_ == vqec_vision_ai_stor_stsql_read_text(_row, 5) &&
        _association.topology_path_ == vqec_vision_ai_stor_stsql_read_text(_row, 6) &&
        _association.score_ppm_ ==
            static_cast<std::uint32_t>(sqlite3_column_int64(_row, 7)) &&
        _association.minimum_travel_ns_ ==
            static_cast<std::uint64_t>(sqlite3_column_int64(_row, 8)) &&
        _association.maximum_travel_ns_ ==
            static_cast<std::uint64_t>(sqlite3_column_int64(_row, 9)) &&
        _association.clock_uncertainty_ns_ ==
            static_cast<std::uint64_t>(sqlite3_column_int64(_row, 10)) &&
        _association.review_state_ ==
            static_cast<association_review_state>(sqlite3_column_int(_row, 11)) &&
        _association.recorded_ns_ == sqlite3_column_int64(_row, 12);
}

}  // namespace

sqlite_spatiotemporal_store::sqlite_spatiotemporal_store(spatiotemporal_store_config _config)
    : config_(std::move(_config)) {}

sqlite_spatiotemporal_store::~sqlite_spatiotemporal_store() noexcept {
    (void)vqec_vision_ai_stor_stsql_close();
}

status sqlite_spatiotemporal_store::vqec_vision_ai_stor_stsql_open() {
    if (catalog_ != nullptr) {
        return {status_code::invalid_state, "spatiotemporal store is already open"};
    }
    if (!vqec_vision_ai_stor_stsql_is_config_valid(config_)) {
        return {status_code::invalid_argument, "spatiotemporal store configuration is invalid"};
    }
    std::error_code error;
    const std::filesystem::path root(config_.root_directory_);
    std::filesystem::create_directories(root, error);
    if (error || !std::filesystem::is_directory(root, error) || error) {
        return {status_code::io_error, "create spatiotemporal store directory failed"};
    }
    const auto catalog_path = (root / g_spatiotemporal_catalog_filename).string();
    auto result = vqec_vision_ai_stor_stsql_open_database(catalog_path, config_,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, catalog_);
    if (result.code_ != status_code::ok) {
        return result;
    }
    result = vqec_vision_ai_stor_stsql_execute(catalog_, g_spatiotemporal_catalog_schema_sql);
    if (result.code_ != status_code::ok) {
        (void)vqec_vision_ai_stor_stsql_close();
        return result;
    }
    result = vqec_vision_ai_stor_stsql_recover_index();
    if (result.code_ != status_code::ok) {
        (void)vqec_vision_ai_stor_stsql_close();
        return result;
    }
    stats_.store_bytes_ = vqec_vision_ai_stor_stsql_get_regular_file_bytes(root);
    return {};
}

status sqlite_spatiotemporal_store::vqec_vision_ai_stor_stsql_close() noexcept {
    if (catalog_ == nullptr && detail_writers_.empty()) {
        return {};
    }
    status result;
    for (auto& entry : detail_writers_) {
        if (entry.second != nullptr && sqlite3_close(entry.second) != SQLITE_OK &&
            result.code_ == status_code::ok) {
            result = {status_code::io_error, "close trajectory detail writer failed"};
        }
    }
    detail_writers_.clear();
    if (catalog_ == nullptr) {
        return result;
    }
    if (sqlite3_close(catalog_) != SQLITE_OK) {
        return vqec_vision_ai_stor_stsql_make_error(catalog_, "close spatiotemporal catalog");
    }
    catalog_ = nullptr;
    return result;
}

status sqlite_spatiotemporal_store::vqec_vision_ai_stor_stsql_ingest_trajectory(
    const trajectory_chunk& _chunk, const std::vector<std::string>& _outbox_sinks) {
    if (catalog_ == nullptr) {
        return {status_code::invalid_state, "spatiotemporal store is not open"};
    }
    auto result = vqec_vision_ai_cntr_stmet_validate_trajectory_chunk(_chunk);
    if (result.code_ != status_code::ok) {
        return result;
    }
    if (!vqec_vision_ai_stor_stsql_validate_sinks(_outbox_sinks)) {
        return {status_code::invalid_argument, "trajectory outbox sinks are invalid"};
    }
    encoded_trajectory_points encoded;
    result = vqec_vision_ai_cntr_trcod_encode_points(
        _chunk, config_.maximum_encoded_chunk_bytes_, encoded);
    if (result.code_ != status_code::ok) {
        return result;
    }
    const std::filesystem::path root(config_.root_directory_);
    const auto current_bytes = vqec_vision_ai_stor_stsql_get_regular_file_bytes(root);
    std::error_code space_error;
    const auto space = std::filesystem::space(root, space_error);
    if (current_bytes == std::numeric_limits<std::uint64_t>::max() ||
        space_error ||
        encoded.bytes_.size() > config_.maximum_store_bytes_ -
            std::min(config_.maximum_store_bytes_, current_bytes) ||
        space.available < config_.reserve_free_bytes_ ||
        encoded.bytes_.size() > space.available -
            std::min(space.available, config_.reserve_free_bytes_)) {
        ++stats_.rejected_quota_writes_;
        return {status_code::resource_exhausted, "spatiotemporal storage quota is exhausted"};
    }
    sqlite_statement existing_statement;
    result = vqec_vision_ai_stor_stsql_prepare(catalog_,
        "SELECT c.checksum_crc32,c.encoded_bytes,m.relative_path FROM chunk_index c "
        "JOIN shard_manifests m ON m.shard_id=c.shard_id WHERE c.chunk_id=?1;",
        existing_statement);
    auto* existing = existing_statement.vqec_vision_ai_stor_stsql_get();
    if (result.code_ != status_code::ok ||
        !vqec_vision_ai_stor_stsql_bind_text(existing, 1, _chunk.chunk_id_)) {
        return result.code_ == status_code::ok
                   ? vqec_vision_ai_stor_stsql_make_error(catalog_, "bind existing chunk")
                   : result;
    }
    if (sqlite3_step(existing) == SQLITE_ROW) {
        const auto checksum = static_cast<std::uint32_t>(sqlite3_column_int64(existing, 0));
        const auto bytes = static_cast<std::size_t>(sqlite3_column_int64(existing, 1));
        const auto relative_path = vqec_vision_ai_stor_stsql_read_text(existing, 2);
        sqlite3* persisted_database = nullptr;
        const auto persisted_path = (root / relative_path).string();
        if (checksum == encoded.checksum_crc32_ && bytes == encoded.bytes_.size() &&
            sqlite3_open_v2(persisted_path.c_str(), &persisted_database,
                SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr) == SQLITE_OK) {
            trajectory_chunk persisted_chunk;
            std::size_t persisted_bytes = 0U;
            const auto load_result = vqec_vision_ai_stor_stsql_load_chunk(persisted_database,
                _chunk.chunk_id_, config_.maximum_encoded_chunk_bytes_, persisted_chunk,
                persisted_bytes);
            sqlite3_close(persisted_database);
            if (load_result.code_ == status_code::ok &&
                persisted_bytes == bytes &&
                vqec_vision_ai_stor_stsql_chunks_match(persisted_chunk, _chunk)) {
                return {};
            }
        } else if (persisted_database != nullptr) {
            sqlite3_close(persisted_database);
        }
        return {status_code::invalid_argument,
            "trajectory chunk id conflicts with persisted data"};
    }
    shard_descriptor shard;
    result = vqec_vision_ai_stor_stsql_find_or_create_shard(catalog_, config_,
        _chunk.first_frame_.source_pts_ns_, encoded.bytes_.size(), shard);
    if (result.code_ != status_code::ok) {
        return result;
    }
    const auto detail_path = (root / shard.relative_path_).string();
    sqlite3* detail = nullptr;
    const auto existing_writer = detail_writers_.find(shard.relative_path_);
    if (existing_writer != detail_writers_.end()) {
        detail = existing_writer->second;
    } else {
        result = vqec_vision_ai_stor_stsql_open_database(detail_path, config_,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, detail);
        if (result.code_ != status_code::ok) {
            return result;
        }
        result = vqec_vision_ai_stor_stsql_execute(detail, g_spatiotemporal_detail_schema_sql);
        if (result.code_ == status_code::ok) {
            try {
                detail_writers_.emplace(shard.relative_path_, detail);
            } catch (const std::bad_alloc&) {
                sqlite3_close(detail);
                return {status_code::resource_exhausted,
                    "trajectory detail writer registry allocation failed"};
            }
        } else {
            sqlite3_close(detail);
            return result;
        }
    }
    std::uint64_t global_sequence = 0U;
    if (result.code_ == status_code::ok) {
        result = vqec_vision_ai_stor_stsql_reserve_sequence(
            catalog_, global_sequence, true);
    }
    if (result.code_ == status_code::ok) {
        result = vqec_vision_ai_stor_stsql_insert_detail(
            detail, _chunk, encoded, global_sequence, _outbox_sinks);
    }
    if (result.code_ != status_code::ok) {
        if (sqlite3_get_autocommit(catalog_) == 0) {
            (void)vqec_vision_ai_stor_stsql_execute(catalog_, "ROLLBACK;");
        }
        return result;
    }
    result = vqec_vision_ai_stor_stsql_insert_chunk_index(
        catalog_, _chunk, encoded, global_sequence, shard);
    if (result.code_ == status_code::ok) {
        result = vqec_vision_ai_stor_stsql_execute(catalog_, "COMMIT;");
    } else if (sqlite3_get_autocommit(catalog_) == 0) {
        (void)vqec_vision_ai_stor_stsql_execute(catalog_, "ROLLBACK;");
    }
    if (result.code_ != status_code::ok) {
        if (sqlite3_get_autocommit(catalog_) == 0) {
            (void)vqec_vision_ai_stor_stsql_execute(catalog_, "ROLLBACK;");
        }
        return result;
    }
    ++stats_.committed_chunks_;
    stats_.store_bytes_ = vqec_vision_ai_stor_stsql_get_regular_file_bytes(root);
    return {};
}

status sqlite_spatiotemporal_store::vqec_vision_ai_stor_stsql_ingest_association(
    const track_association_revision& _association) {
    if (catalog_ == nullptr) {
        return {status_code::invalid_state, "spatiotemporal store is not open"};
    }
    auto result = vqec_vision_ai_cntr_stmet_validate_association_revision(_association);
    if (result.code_ != status_code::ok) {
        return result;
    }
    sqlite_statement revision_statement;
    result = vqec_vision_ai_stor_stsql_prepare(catalog_,
        "SELECT revision,supersedes_revision,entity_id,left_chunk_id,right_chunk_id,"
        "method_revision,topology_path,score_ppm,minimum_travel_ns,maximum_travel_ns,"
        "clock_uncertainty_ns,review_state,recorded_ns FROM association_revisions "
        "WHERE association_id=?1 ORDER BY revision DESC LIMIT 1;", revision_statement);
    auto* revision_query = revision_statement.vqec_vision_ai_stor_stsql_get();
    if (result.code_ != status_code::ok ||
        !vqec_vision_ai_stor_stsql_bind_text(
            revision_query, 1, _association.association_id_)) {
        return result.code_ == status_code::ok
                   ? vqec_vision_ai_stor_stsql_make_error(catalog_, "bind association revision")
                   : result;
    }
    const auto revision_step = sqlite3_step(revision_query);
    const auto latest_revision = revision_step == SQLITE_ROW
        ? static_cast<std::uint64_t>(sqlite3_column_int64(revision_query, 0))
        : 0U;
    if (latest_revision >= _association.revision_) {
        if (latest_revision == _association.revision_ &&
            vqec_vision_ai_stor_stsql_association_matches_row(_association, revision_query)) {
            return {};
        }
        return {status_code::invalid_argument,
            "association revision is stale or conflicts with persisted data"};
    }
    if (latest_revision != _association.supersedes_revision_) {
        return {status_code::invalid_argument,
            "association supersedes revision is not current"};
    }
    sqlite_statement reference_statement;
    result = vqec_vision_ai_stor_stsql_prepare(catalog_,
        "SELECT COUNT(*) FROM chunk_index WHERE chunk_id IN (?1,?2);", reference_statement);
    auto* reference = reference_statement.vqec_vision_ai_stor_stsql_get();
    if (result.code_ != status_code::ok ||
        !vqec_vision_ai_stor_stsql_bind_text(reference, 1, _association.left_chunk_id_) ||
        !vqec_vision_ai_stor_stsql_bind_text(reference, 2, _association.right_chunk_id_) ||
        sqlite3_step(reference) != SQLITE_ROW || sqlite3_column_int(reference, 0) != 2) {
        return result.code_ == status_code::ok
                   ? status{status_code::invalid_argument,
                         "association references unavailable trajectory chunks"}
                   : result;
    }
    std::uint64_t sequence = 0U;
    result = vqec_vision_ai_stor_stsql_reserve_sequence(catalog_, sequence);
    if (result.code_ != status_code::ok) {
        return result;
    }
    sqlite_statement statement;
    result = vqec_vision_ai_stor_stsql_prepare(catalog_,
        "INSERT INTO association_revisions(global_sequence,association_id,revision,"
        "supersedes_revision,entity_id,left_chunk_id,right_chunk_id,method_revision,"
        "topology_path,score_ppm,minimum_travel_ns,maximum_travel_ns,clock_uncertainty_ns,"
        "review_state,recorded_ns) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15);",
        statement);
    auto* insert = statement.vqec_vision_ai_stor_stsql_get();
    const bool is_bound = result.code_ == status_code::ok &&
        sqlite3_bind_int64(insert, 1, static_cast<sqlite3_int64>(sequence)) == SQLITE_OK &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 2, _association.association_id_) &&
        sqlite3_bind_int64(insert, 3, static_cast<sqlite3_int64>(_association.revision_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 4, static_cast<sqlite3_int64>(_association.supersedes_revision_)) == SQLITE_OK &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 5, _association.entity_id_) &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 6, _association.left_chunk_id_) &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 7, _association.right_chunk_id_) &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 8, _association.method_revision_) &&
        vqec_vision_ai_stor_stsql_bind_text(insert, 9, _association.topology_path_) &&
        sqlite3_bind_int64(insert, 10, _association.score_ppm_) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 11, static_cast<sqlite3_int64>(_association.minimum_travel_ns_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 12, static_cast<sqlite3_int64>(_association.maximum_travel_ns_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 13, static_cast<sqlite3_int64>(_association.clock_uncertainty_ns_)) == SQLITE_OK &&
        sqlite3_bind_int(insert, 14, static_cast<int>(_association.review_state_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 15, _association.recorded_ns_) == SQLITE_OK;
    if (!is_bound || sqlite3_step(insert) != SQLITE_DONE) {
        return result.code_ == status_code::ok
                   ? vqec_vision_ai_stor_stsql_make_error(catalog_, "insert association revision")
                   : result;
    }
    ++stats_.committed_associations_;
    return {};
}

status sqlite_spatiotemporal_store::vqec_vision_ai_stor_stsql_query(
    const spatiotemporal_query& _query, spatiotemporal_query_page& _page) {
    if (catalog_ == nullptr) {
        return {status_code::invalid_state, "spatiotemporal store is not open"};
    }
    auto result = vqec_vision_ai_cntr_stmet_validate_query(_query);
    if (result.code_ != status_code::ok) {
        return result;
    }
    if (query_cancel_requested_.exchange(false, std::memory_order_acq_rel)) {
        _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
        return {status_code::timeout, "metadata query was cancelled"};
    }
    _page = {};
    std::uint64_t snapshot = _query.snapshot_sequence_;
    if (snapshot == 0U) {
        sqlite_statement snapshot_statement;
        result = vqec_vision_ai_stor_stsql_prepare(catalog_,
            "SELECT value FROM store_state WHERE key='global_sequence';", snapshot_statement);
        if (result.code_ != status_code::ok ||
            sqlite3_step(snapshot_statement.vqec_vision_ai_stor_stsql_get()) != SQLITE_ROW) {
            return result.code_ == status_code::ok
                       ? vqec_vision_ai_stor_stsql_make_error(catalog_, "read query snapshot")
                       : result;
        }
        snapshot = static_cast<std::uint64_t>(
            sqlite3_column_int64(snapshot_statement.vqec_vision_ai_stor_stsql_get(), 0));
    }
    _page.snapshot_sequence_ = snapshot;
    if (_query.collection_ == spatiotemporal_collection::episodes ||
        _query.collection_ == spatiotemporal_collection::aggregates) {
        return vqec_vision_ai_stor_stsql_query_projection(_query, snapshot, _page);
    }
    if (_query.collection_ == spatiotemporal_collection::entities) {
        const auto identity_mask = vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::identity);
        if ((_query.allowed_access_domain_mask_ & identity_mask) == 0U) {
            return {status_code::unauthorized, "entity association access is denied"};
        }
        sqlite_statement statement;
        std::string sql =
            "SELECT global_sequence,association_id,revision,supersedes_revision,entity_id,"
            "left_chunk_id,right_chunk_id,method_revision,topology_path,score_ppm,"
            "minimum_travel_ns,maximum_travel_ns,clock_uncertainty_ns,review_state,recorded_ns "
            "FROM association_revisions WHERE global_sequence>?1 AND global_sequence<=?2 "
            "AND recorded_ns>=?3 AND recorded_ns<?4";
        if (_query.revision_view_ == spatiotemporal_revision_view::as_observed) {
            sql += " AND revision=1";
        } else if (_query.revision_view_ == spatiotemporal_revision_view::as_known_at) {
            sql += " AND recorded_ns<=" + std::to_string(_query.known_at_ns_) +
                " AND revision=(SELECT MAX(a2.revision) FROM association_revisions a2 "
                "WHERE a2.association_id=association_revisions.association_id "
                "AND a2.recorded_ns<=" + std::to_string(_query.known_at_ns_) + ")";
        } else {
            sql += " AND revision=(SELECT MAX(a2.revision) FROM association_revisions a2 "
                "WHERE a2.association_id=association_revisions.association_id)";
        }
        if (!_query.entity_id_.empty()) {
            sql += " AND entity_id=?5";
        }
        sql += " ORDER BY global_sequence LIMIT ?6;";
        result = vqec_vision_ai_stor_stsql_prepare(catalog_, sql, statement);
        auto* query = statement.vqec_vision_ai_stor_stsql_get();
        const auto result_limit = std::min(
            _query.budget_.maximum_results_, config_.maximum_query_results_);
        const bool is_bound = result.code_ == status_code::ok &&
            sqlite3_bind_int64(query, 1, static_cast<sqlite3_int64>(_query.cursor_sequence_)) == SQLITE_OK &&
            sqlite3_bind_int64(query, 2, static_cast<sqlite3_int64>(snapshot)) == SQLITE_OK &&
            sqlite3_bind_int64(query, 3, _query.begin_ns_) == SQLITE_OK &&
            sqlite3_bind_int64(query, 4, _query.end_ns_) == SQLITE_OK &&
            (_query.entity_id_.empty() ||
                vqec_vision_ai_stor_stsql_bind_text(query, 5, _query.entity_id_)) &&
            sqlite3_bind_int64(query, 6, static_cast<sqlite3_int64>(result_limit + 1U)) == SQLITE_OK;
        if (!is_bound) {
            return result.code_ == status_code::ok
                       ? vqec_vision_ai_stor_stsql_make_error(catalog_, "bind association query")
                       : result;
        }
        while (sqlite3_step(query) == SQLITE_ROW) {
            if (query_cancel_requested_.exchange(false, std::memory_order_acq_rel)) {
                _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
                return {status_code::timeout, "metadata query was cancelled"};
            }
            if (_page.associations_.size() == result_limit) {
                _page.has_more_ = true;
                break;
            }
            track_association_revision association;
            _page.next_cursor_sequence_ = static_cast<std::uint64_t>(sqlite3_column_int64(query, 0));
            association.association_id_ = vqec_vision_ai_stor_stsql_read_text(query, 1);
            association.revision_ = static_cast<std::uint64_t>(sqlite3_column_int64(query, 2));
            association.supersedes_revision_ = static_cast<std::uint64_t>(sqlite3_column_int64(query, 3));
            association.entity_id_ = vqec_vision_ai_stor_stsql_read_text(query, 4);
            association.left_chunk_id_ = vqec_vision_ai_stor_stsql_read_text(query, 5);
            association.right_chunk_id_ = vqec_vision_ai_stor_stsql_read_text(query, 6);
            association.method_revision_ = vqec_vision_ai_stor_stsql_read_text(query, 7);
            association.topology_path_ = vqec_vision_ai_stor_stsql_read_text(query, 8);
            association.score_ppm_ = static_cast<std::uint32_t>(sqlite3_column_int64(query, 9));
            association.minimum_travel_ns_ = static_cast<std::uint64_t>(sqlite3_column_int64(query, 10));
            association.maximum_travel_ns_ = static_cast<std::uint64_t>(sqlite3_column_int64(query, 11));
            association.clock_uncertainty_ns_ = static_cast<std::uint64_t>(sqlite3_column_int64(query, 12));
            association.review_state_ = static_cast<association_review_state>(sqlite3_column_int(query, 13));
            association.recorded_ns_ = sqlite3_column_int64(query, 14);
            _page.associations_.push_back(std::move(association));
        }
        if (query_cancel_requested_.exchange(false, std::memory_order_acq_rel)) {
            _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
            return {status_code::timeout, "metadata query was cancelled"};
        }
        _page.completeness_ = spatiotemporal_result_completeness::complete;
        _page.delivered_resolution_ = trajectory_resolution::episode_fact;
        return {};
    }
    if (_query.collection_ != spatiotemporal_collection::tracklets ||
        _query.spatial_relation_ == spatiotemporal_spatial_relation::nearest ||
        _query.minimum_resolution_ > trajectory_resolution::trajectory_bounded) {
        _page.completeness_ = spatiotemporal_result_completeness::unsupported;
        return {status_code::unsupported, "spatiotemporal collection is not implemented"};
    }
    const auto trajectory_mask = vqec_vision_ai_cntr_stmet_get_access_domain_mask(
        spatiotemporal_access_domain::trajectory);
    if ((_query.allowed_access_domain_mask_ & trajectory_mask) == 0U) {
        return {status_code::unauthorized, "trajectory access is denied"};
    }
    sqlite_statement gap_statement;
    result = vqec_vision_ai_stor_stsql_prepare(catalog_,
        "SELECT COUNT(*) FROM shard_manifests WHERE state='missing' "
        "AND bucket_begin_ns<?1 AND bucket_end_ns>?2;", gap_statement);
    auto* gap_query = gap_statement.vqec_vision_ai_stor_stsql_get();
    if (result.code_ != status_code::ok ||
        sqlite3_bind_int64(gap_query, 1, _query.end_ns_) != SQLITE_OK ||
        sqlite3_bind_int64(gap_query, 2, _query.begin_ns_) != SQLITE_OK ||
        sqlite3_step(gap_query) != SQLITE_ROW) {
        return result.code_ == status_code::ok
                   ? vqec_vision_ai_stor_stsql_make_error(catalog_, "read trajectory gaps")
                   : result;
    }
    _page.has_coverage_gap_ = sqlite3_column_int64(gap_query, 0) != 0;
    std::ostringstream sql;
    sql << "SELECT c.global_sequence,c.chunk_id,m.relative_path,c.encoded_bytes,c.resolution "
           "FROM chunk_index c JOIN shard_manifests m ON m.shard_id=c.shard_id "
           "WHERE c.global_sequence>?1 AND c.global_sequence<=?2 AND c.begin_ns<?3 "
           "AND c.end_ns>?4 AND m.state!='missing' "
           "AND (c.required_access_mask & ?5)=c.required_access_mask";
    int bind_index = 6;
    sql << " AND c.source_id IN (";
    for (std::size_t index = 0U; index < _query.source_ids_.size(); ++index) {
        if (index != 0U) {
            sql << ',';
        }
        sql << '?' << bind_index++;
    }
    sql << ')';
    const int subject_index = _query.subject_ref_.empty() ? 0 : bind_index++;
    if (subject_index != 0) {
        sql << " AND c.subject_ref=?" << subject_index;
    }
    const int category_index = _query.semantic_type_.empty() ? 0 : bind_index++;
    if (category_index != 0) {
        sql << " AND c.entity_category=?" << category_index;
    }
    if (_query.minimum_resolution_ == trajectory_resolution::observation_exact) {
        sql << " AND c.resolution=" << static_cast<int>(trajectory_resolution::observation_exact);
    }
    if (_query.has_spatial_bounds_) {
        const int bounds_left_index = bind_index++;
        const int bounds_right_index = bind_index++;
        const int bounds_top_index = bind_index++;
        const int bounds_bottom_index = bind_index++;
        sql << " AND c.bounds_right>=?" << bounds_left_index
            << " AND c.bounds_left<=?" << bounds_right_index
            << " AND c.bounds_bottom>=?" << bounds_top_index
            << " AND c.bounds_top<=?" << bounds_bottom_index;
    }
    sql << " ORDER BY c.global_sequence LIMIT ?" << bind_index << ';';
    sqlite_statement candidate_statement;
    result = vqec_vision_ai_stor_stsql_prepare(catalog_, sql.str(), candidate_statement);
    auto* candidate_query = candidate_statement.vqec_vision_ai_stor_stsql_get();
    if (result.code_ != status_code::ok ||
        sqlite3_bind_int64(candidate_query, 1, static_cast<sqlite3_int64>(_query.cursor_sequence_)) != SQLITE_OK ||
        sqlite3_bind_int64(candidate_query, 2, static_cast<sqlite3_int64>(snapshot)) != SQLITE_OK ||
        sqlite3_bind_int64(candidate_query, 3, _query.end_ns_) != SQLITE_OK ||
        sqlite3_bind_int64(candidate_query, 4, _query.begin_ns_) != SQLITE_OK ||
        sqlite3_bind_int64(candidate_query, 5, _query.allowed_access_domain_mask_) != SQLITE_OK) {
        return result.code_ == status_code::ok
                   ? vqec_vision_ai_stor_stsql_make_error(catalog_, "bind trajectory query")
                   : result;
    }
    int value_index = 6;
    for (const auto& source : _query.source_ids_) {
        if (!vqec_vision_ai_stor_stsql_bind_text(candidate_query, value_index++, source)) {
            return vqec_vision_ai_stor_stsql_make_error(catalog_, "bind trajectory source");
        }
    }
    if (subject_index != 0 &&
        !vqec_vision_ai_stor_stsql_bind_text(candidate_query, value_index++, _query.subject_ref_)) {
        return vqec_vision_ai_stor_stsql_make_error(catalog_, "bind trajectory subject");
    }
    if (category_index != 0 &&
        !vqec_vision_ai_stor_stsql_bind_text(candidate_query, value_index++, _query.semantic_type_)) {
        return vqec_vision_ai_stor_stsql_make_error(catalog_, "bind trajectory category");
    }
    if (_query.has_spatial_bounds_) {
        if (sqlite3_bind_int(candidate_query, value_index++, _query.bounds_left_) != SQLITE_OK ||
            sqlite3_bind_int(candidate_query, value_index++, _query.bounds_right_) != SQLITE_OK ||
            sqlite3_bind_int(candidate_query, value_index++, _query.bounds_top_) != SQLITE_OK ||
            sqlite3_bind_int(candidate_query, value_index++, _query.bounds_bottom_) != SQLITE_OK) {
            return vqec_vision_ai_stor_stsql_make_error(catalog_, "bind trajectory bounds");
        }
    }
    const auto result_limit = std::min(
        _query.budget_.maximum_results_, config_.maximum_query_results_);
    if (sqlite3_bind_int64(candidate_query, value_index,
            static_cast<sqlite3_int64>(result_limit + 1U)) != SQLITE_OK) {
        return vqec_vision_ai_stor_stsql_make_error(catalog_, "bind trajectory limit");
    }
    std::vector<chunk_candidate> candidates;
    while (sqlite3_step(candidate_query) == SQLITE_ROW) {
        if (query_cancel_requested_.exchange(false, std::memory_order_acq_rel)) {
            _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
            return {status_code::timeout, "metadata query was cancelled"};
        }
        chunk_candidate candidate;
        candidate.sequence_ = static_cast<std::uint64_t>(sqlite3_column_int64(candidate_query, 0));
        candidate.chunk_id_ = vqec_vision_ai_stor_stsql_read_text(candidate_query, 1);
        candidate.relative_path_ = vqec_vision_ai_stor_stsql_read_text(candidate_query, 2);
        candidate.encoded_bytes_ = static_cast<std::size_t>(sqlite3_column_int64(candidate_query, 3));
        candidate.resolution_ = static_cast<trajectory_resolution>(sqlite3_column_int(candidate_query, 4));
        candidates.push_back(std::move(candidate));
    }
    if (query_cancel_requested_.exchange(false, std::memory_order_acq_rel)) {
        _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
        return {status_code::timeout, "metadata query was cancelled"};
    }
    sqlite_connection_map connections;
    bool is_approximate = false;
    for (std::size_t candidate_index = 0U;
         candidate_index < candidates.size(); ++candidate_index) {
        if (query_cancel_requested_.exchange(false, std::memory_order_acq_rel)) {
            _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
            return {status_code::timeout, "metadata query was cancelled"};
        }
        if (candidate_index == result_limit) {
            _page.has_more_ = true;
            break;
        }
        const auto& candidate = candidates[candidate_index];
        _page.next_cursor_sequence_ = candidate.sequence_;
        if (vqec_vision_ai_stor_stsql_get_deadline_now_ns() >= _query.budget_.deadline_ns_ ||
            candidate.encoded_bytes_ > _query.budget_.maximum_scan_bytes_ -
                std::min(_query.budget_.maximum_scan_bytes_, _page.scanned_bytes_)) {
            _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
            _page.has_more_ = true;
            return {};
        }
        const auto path = (std::filesystem::path(config_.root_directory_) /
            candidate.relative_path_).string();
        auto* detail = connections.vqec_vision_ai_stor_stsql_get_connection(
            path, config_.busy_timeout_ms_);
        if (detail == nullptr) {
            _page.has_coverage_gap_ = true;
            continue;
        }
        trajectory_chunk chunk;
        std::size_t encoded_bytes = 0U;
        result = vqec_vision_ai_stor_stsql_load_chunk(detail, candidate.chunk_id_,
            config_.maximum_encoded_chunk_bytes_, chunk, encoded_bytes);
        _page.scanned_bytes_ += candidate.encoded_bytes_;
        if (result.code_ != status_code::ok) {
            _page.has_coverage_gap_ = true;
            continue;
        }
        if (!vqec_vision_ai_stor_stsql_matches_exact_spatial(chunk, _query)) {
            continue;
        }
        if (encoded_bytes > _query.budget_.maximum_result_bytes_ -
                std::min(_query.budget_.maximum_result_bytes_, _page.result_bytes_)) {
            _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
            _page.has_more_ = true;
            return {};
        }
        _page.result_bytes_ += encoded_bytes;
        is_approximate = is_approximate ||
            candidate.resolution_ == trajectory_resolution::trajectory_bounded;
        _page.trajectory_chunks_.push_back(std::move(chunk));
    }
    _page.delivered_resolution_ = is_approximate
        ? trajectory_resolution::trajectory_bounded
        : trajectory_resolution::observation_exact;
    _page.completeness_ = _page.has_coverage_gap_
        ? spatiotemporal_result_completeness::partial
        : (is_approximate ? spatiotemporal_result_completeness::approximate
                          : spatiotemporal_result_completeness::complete);
    return {};
}

void sqlite_spatiotemporal_store::vqec_vision_ai_stor_stsql_cancel_query() noexcept {
    query_cancel_requested_.store(true, std::memory_order_release);
    if (catalog_ != nullptr) {
        sqlite3_interrupt(catalog_);
    }
}

void sqlite_spatiotemporal_store::
vqec_vision_ai_stor_stsql_clear_query_cancellation() noexcept {
    query_cancel_requested_.store(false, std::memory_order_release);
}

status sqlite_spatiotemporal_store::vqec_vision_ai_stor_stsql_seal_before(
    std::uint64_t _source_pts_ns) {
    if (catalog_ == nullptr) {
        return {status_code::invalid_state, "spatiotemporal store is not open"};
    }
    sqlite_statement path_statement;
    auto result = vqec_vision_ai_stor_stsql_prepare(catalog_,
        "SELECT relative_path FROM shard_manifests WHERE state='active' AND bucket_end_ns<=?1;",
        path_statement);
    auto* path_query = path_statement.vqec_vision_ai_stor_stsql_get();
    if (result.code_ != status_code::ok ||
        sqlite3_bind_int64(path_query, 1, static_cast<sqlite3_int64>(_source_pts_ns)) != SQLITE_OK) {
        return result.code_ == status_code::ok
                   ? vqec_vision_ai_stor_stsql_make_error(catalog_, "bind shards to seal")
                   : result;
    }
    std::vector<std::string> paths;
    while (sqlite3_step(path_query) == SQLITE_ROW) {
        paths.push_back(vqec_vision_ai_stor_stsql_read_text(path_query, 0));
    }
    for (const auto& relative_path : paths) {
        const auto writer = detail_writers_.find(relative_path);
        if (writer != detail_writers_.end()) {
            if (sqlite3_close(writer->second) != SQLITE_OK) {
                return {status_code::io_error,
                    "close trajectory detail writer before seal failed"};
            }
            detail_writers_.erase(writer);
        }
        sqlite3* detail = nullptr;
        const auto full_path =
            (std::filesystem::path(config_.root_directory_) / relative_path).string();
        result = vqec_vision_ai_stor_stsql_open_database(full_path, config_,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, detail);
        if (result.code_ != status_code::ok) {
            return result;
        }
        result = vqec_vision_ai_stor_stsql_execute(detail, "PRAGMA wal_checkpoint(TRUNCATE);");
        const auto close_result = sqlite3_close(detail);
        if (result.code_ != status_code::ok || close_result != SQLITE_OK) {
            return result.code_ == status_code::ok
                       ? status{status_code::io_error, "close sealed trajectory shard failed"}
                       : result;
        }
    }
    sqlite_statement statement;
    result = vqec_vision_ai_stor_stsql_prepare(catalog_,
        "UPDATE shard_manifests SET state='sealed' WHERE state='active' AND bucket_end_ns<=?1;",
        statement);
    auto* update = statement.vqec_vision_ai_stor_stsql_get();
    if (result.code_ != status_code::ok ||
        sqlite3_bind_int64(update, 1, static_cast<sqlite3_int64>(_source_pts_ns)) != SQLITE_OK ||
        sqlite3_step(update) != SQLITE_DONE) {
        return result.code_ == status_code::ok
                   ? vqec_vision_ai_stor_stsql_make_error(catalog_, "seal trajectory shards")
                   : result;
    }
    stats_.sealed_shards_ += static_cast<std::uint64_t>(sqlite3_changes(catalog_));
    return {};
}

status sqlite_spatiotemporal_store::vqec_vision_ai_stor_stsql_acknowledge_outbox(
    const metadata_outbox_receipt& _receipt) {
    if (catalog_ == nullptr) {
        return {status_code::invalid_state, "spatiotemporal store is not open"};
    }
    if (!vqec_vision_ai_cntr_ident_is_valid(
            _receipt.sink_id_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _receipt.record_id_, g_spatiotemporal_max_identifier_bytes) ||
        _receipt.revision_ == 0U || _receipt.revision_ == UINT64_MAX) {
        return {status_code::invalid_argument, "metadata outbox receipt is invalid"};
    }
    sqlite3* database = catalog_;
    bool close_database = false;
    const char* family = nullptr;
    if (_receipt.record_family_ == metadata_outbox_record_family::trajectory) {
        if (_receipt.revision_ != 1U) {
            return {status_code::invalid_argument, "trajectory receipt revision must be one"};
        }
        sqlite_statement path_statement;
        auto result = vqec_vision_ai_stor_stsql_prepare(catalog_,
            "SELECT m.relative_path FROM chunk_index c JOIN shard_manifests m "
            "ON m.shard_id=c.shard_id WHERE c.chunk_id=?1 AND m.state!='retired';",
            path_statement);
        auto* path_query = path_statement.vqec_vision_ai_stor_stsql_get();
        if (result.code_ != status_code::ok ||
            !vqec_vision_ai_stor_stsql_bind_text(path_query, 1, _receipt.record_id_) ||
            sqlite3_step(path_query) != SQLITE_ROW) {
            return result.code_ == status_code::ok
                ? status{status_code::invalid_argument, "trajectory receipt record is unknown"}
                : result;
        }
        const auto relative_path = vqec_vision_ai_stor_stsql_read_text(path_query, 0);
        const auto writer = detail_writers_.find(relative_path);
        if (writer != detail_writers_.end()) {
            database = writer->second;
        } else {
            const auto full_path = (std::filesystem::path(config_.root_directory_) /
                relative_path).string();
            result = vqec_vision_ai_stor_stsql_open_database(full_path, config_,
                SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, database);
            if (result.code_ != status_code::ok) {
                return result;
            }
            close_database = true;
        }
    } else if (_receipt.record_family_ == metadata_outbox_record_family::episode) {
        family = "episode";
    } else if (_receipt.record_family_ ==
               metadata_outbox_record_family::aggregate_contribution) {
        family = "aggregate_contribution";
    } else {
        return {status_code::invalid_argument, "metadata outbox family is invalid"};
    }

    status result;
    {
        sqlite_statement statement;
        std::string sql;
        if (_receipt.record_family_ == metadata_outbox_record_family::trajectory) {
            sql = "UPDATE trajectory_outbox SET state=?1 WHERE sink_id=?2 AND chunk_id=?3 "
                  "AND state IN ('pending','acknowledged');";
        } else {
            sql = "UPDATE metadata_outbox SET state=?1 WHERE sink_id=?2 AND record_family=?3 "
                  "AND record_id=?4 AND revision=?5 AND state IN ('pending','acknowledged');";
        }
        result = vqec_vision_ai_stor_stsql_prepare(database, sql, statement);
        auto* update = statement.vqec_vision_ai_stor_stsql_get();
        bool is_bound = result.code_ == status_code::ok &&
            vqec_vision_ai_stor_stsql_bind_text(
                update, 1, g_spatiotemporal_outbox_acknowledged) &&
            vqec_vision_ai_stor_stsql_bind_text(update, 2, _receipt.sink_id_);
        if (_receipt.record_family_ == metadata_outbox_record_family::trajectory) {
            is_bound = is_bound &&
                vqec_vision_ai_stor_stsql_bind_text(update, 3, _receipt.record_id_);
        } else {
            is_bound = is_bound &&
                vqec_vision_ai_stor_stsql_bind_text(update, 3, family) &&
                vqec_vision_ai_stor_stsql_bind_text(update, 4, _receipt.record_id_) &&
                sqlite3_bind_int64(update, 5,
                    static_cast<sqlite3_int64>(_receipt.revision_)) == SQLITE_OK;
        }
        if (!is_bound || sqlite3_step(update) != SQLITE_DONE ||
            sqlite3_changes(database) != 1) {
            result = result.code_ == status_code::ok
                ? status{status_code::invalid_argument,
                      "metadata outbox receipt does not match"}
                : result;
        }
    }
    if (close_database && sqlite3_close(database) != SQLITE_OK &&
        result.code_ == status_code::ok) {
        result = {status_code::io_error, "close receipt detail database failed"};
    }
    if (result.code_ == status_code::ok) {
        ++stats_.acknowledged_outbox_records_;
    }
    return result;
}

status sqlite_spatiotemporal_store::vqec_vision_ai_stor_stsql_apply_retention(
    const spatiotemporal_retention_policy& _policy,
    spatiotemporal_retention_report& _report) {
    _report = {};
    if (catalog_ == nullptr) {
        return {status_code::invalid_state, "spatiotemporal store is not open"};
    }
    if (_policy.trajectory_before_ns_ < 0 || _policy.episode_before_ns_ < 0 ||
        _policy.contribution_before_ns_ < 0 || _policy.rollup_before_ns_ < 0) {
        return {status_code::invalid_argument, "metadata retention cutoff is invalid"};
    }
    auto result = vqec_vision_ai_stor_stsql_seal_before(
        static_cast<std::uint64_t>(_policy.trajectory_before_ns_));
    if (result.code_ != status_code::ok) {
        return result;
    }
    sqlite_statement shard_statement;
    result = vqec_vision_ai_stor_stsql_prepare(catalog_,
        "SELECT shard_id,relative_path FROM shard_manifests WHERE state='sealed' "
        "AND bucket_end_ns<=?1 ORDER BY bucket_end_ns;", shard_statement);
    auto* shard_query = shard_statement.vqec_vision_ai_stor_stsql_get();
    if (result.code_ != status_code::ok ||
        sqlite3_bind_int64(shard_query, 1, _policy.trajectory_before_ns_) != SQLITE_OK) {
        return result.code_ == status_code::ok
            ? vqec_vision_ai_stor_stsql_make_error(catalog_, "bind retention shard cutoff")
            : result;
    }
    std::vector<std::pair<std::string, std::string>> shards;
    while (sqlite3_step(shard_query) == SQLITE_ROW) {
        shards.emplace_back(vqec_vision_ai_stor_stsql_read_text(shard_query, 0),
            vqec_vision_ai_stor_stsql_read_text(shard_query, 1));
    }
    const std::filesystem::path root(config_.root_directory_);
    for (const auto& shard : shards) {
        sqlite3* detail = nullptr;
        const auto path = (root / shard.second).string();
        result = vqec_vision_ai_stor_stsql_open_database(path, config_,
            SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, detail);
        if (result.code_ != status_code::ok) {
            return result;
        }
        int step = SQLITE_ERROR;
        sqlite3_int64 pending = -1;
        {
            sqlite_statement pending_statement;
            result = vqec_vision_ai_stor_stsql_prepare(detail,
                "SELECT COUNT(*) FROM trajectory_outbox WHERE state='pending';",
                pending_statement);
            step = result.code_ == status_code::ok
                ? sqlite3_step(pending_statement.vqec_vision_ai_stor_stsql_get())
                : SQLITE_ERROR;
            pending = step == SQLITE_ROW
                ? sqlite3_column_int64(
                      pending_statement.vqec_vision_ai_stor_stsql_get(), 0)
                : -1;
        }
        const int close_result = sqlite3_close(detail);
        if (result.code_ != status_code::ok || step != SQLITE_ROW || close_result != SQLITE_OK) {
            return result.code_ == status_code::ok
                ? status{status_code::io_error, "inspect retention outbox failed"} : result;
        }
        if (pending != 0) {
            ++_report.blocked_shards_;
            continue;
        }
        sqlite_statement retiring_statement;
        result = vqec_vision_ai_stor_stsql_prepare(catalog_,
            "UPDATE shard_manifests SET state=?1 WHERE shard_id=?2 AND state='sealed';",
            retiring_statement);
        auto* retiring = retiring_statement.vqec_vision_ai_stor_stsql_get();
        if (result.code_ != status_code::ok ||
            !vqec_vision_ai_stor_stsql_bind_text(retiring, 1, g_spatiotemporal_shard_retiring) ||
            !vqec_vision_ai_stor_stsql_bind_text(retiring, 2, shard.first) ||
            sqlite3_step(retiring) != SQLITE_DONE) {
            return result.code_ == status_code::ok
                ? vqec_vision_ai_stor_stsql_make_error(catalog_, "mark shard retiring") : result;
        }
        std::error_code remove_error;
        const bool removed = std::filesystem::remove(path, remove_error);
        if (remove_error || !removed) {
            return {status_code::io_error, "remove retired trajectory shard failed"};
        }
        result = vqec_vision_ai_stor_stsql_execute(catalog_, "BEGIN IMMEDIATE;");
        if (result.code_ != status_code::ok) {
            return result;
        }
        sqlite_statement delete_statement;
        result = vqec_vision_ai_stor_stsql_prepare(catalog_,
            "DELETE FROM chunk_index WHERE shard_id=?1;", delete_statement);
        auto* delete_rows = delete_statement.vqec_vision_ai_stor_stsql_get();
        if (result.code_ == status_code::ok &&
            vqec_vision_ai_stor_stsql_bind_text(delete_rows, 1, shard.first) &&
            sqlite3_step(delete_rows) == SQLITE_DONE) {
            sqlite_statement retired_statement;
            result = vqec_vision_ai_stor_stsql_prepare(catalog_,
                "UPDATE shard_manifests SET state=?1,committed_bytes=0 WHERE shard_id=?2;",
                retired_statement);
            auto* retired = retired_statement.vqec_vision_ai_stor_stsql_get();
            if (result.code_ == status_code::ok &&
                vqec_vision_ai_stor_stsql_bind_text(retired, 1, g_spatiotemporal_shard_retired) &&
                vqec_vision_ai_stor_stsql_bind_text(retired, 2, shard.first) &&
                sqlite3_step(retired) == SQLITE_DONE) {
                result = vqec_vision_ai_stor_stsql_execute(catalog_, "COMMIT;");
            }
        }
        if (result.code_ != status_code::ok) {
            (void)vqec_vision_ai_stor_stsql_execute(catalog_, "ROLLBACK;");
            return result;
        }
        ++_report.retired_shards_;
        ++stats_.retired_shards_;
    }

    result = vqec_vision_ai_stor_stsql_execute(catalog_, "BEGIN IMMEDIATE;");
    if (result.code_ != status_code::ok) {
        return result;
    }
    const auto purge = [this](const std::string& _sql, std::int64_t _cutoff,
                           std::uint64_t& _count) -> status {
        sqlite_statement statement;
        auto current = vqec_vision_ai_stor_stsql_prepare(catalog_, _sql, statement);
        auto* query = statement.vqec_vision_ai_stor_stsql_get();
        if (current.code_ != status_code::ok ||
            sqlite3_bind_int64(query, 1, _cutoff) != SQLITE_OK ||
            sqlite3_step(query) != SQLITE_DONE) {
            return current.code_ == status_code::ok
                ? vqec_vision_ai_stor_stsql_make_error(catalog_, "purge metadata retention")
                : current;
        }
        _count = static_cast<std::uint64_t>(sqlite3_changes(catalog_));
        return {};
    };
    result = purge(
        "DELETE FROM episode_revisions WHERE end_ns<=?1 AND NOT EXISTS (SELECT 1 FROM "
        "metadata_outbox o WHERE o.record_family='episode' AND o.record_id=episode_id "
        "AND o.revision=revision AND o.state='pending');",
        _policy.episode_before_ns_, _report.purged_episode_revisions_);
    if (result.code_ == status_code::ok) {
        result = purge(
            "DELETE FROM aggregate_contribution_revisions WHERE bucket_end_ns<=?1 AND NOT "
            "EXISTS (SELECT 1 FROM metadata_outbox o WHERE "
            "o.record_family='aggregate_contribution' AND o.record_id=contribution_id "
            "AND o.revision=revision AND o.state='pending');",
            _policy.contribution_before_ns_, _report.purged_contribution_revisions_);
    }
    if (result.code_ == status_code::ok) {
        result = purge(
            "DELETE FROM aggregate_rollups WHERE bucket_end_ns<=?1 AND NOT EXISTS (SELECT 1 "
            "FROM aggregate_contribution_revisions a JOIN metadata_outbox o ON "
            "o.record_family='aggregate_contribution' AND o.record_id=a.contribution_id "
            "AND o.revision=a.revision AND o.state='pending' WHERE "
            "a.aggregate_definition_id=aggregate_rollups.aggregate_definition_id AND "
            "a.source_id=aggregate_rollups.source_id AND "
            "a.scene_revision=aggregate_rollups.scene_revision AND "
            "a.definition_revision=aggregate_rollups.definition_revision AND "
            "a.bucket_begin_ns=aggregate_rollups.bucket_begin_ns AND "
            "a.bucket_end_ns=aggregate_rollups.bucket_end_ns);",
            _policy.rollup_before_ns_, _report.purged_rollups_);
    }
    if (result.code_ == status_code::ok) {
        result = vqec_vision_ai_stor_stsql_execute(catalog_,
            "DELETE FROM metadata_outbox WHERE state='acknowledged' AND "
            "((record_family='episode' AND NOT EXISTS (SELECT 1 FROM episode_revisions e "
            "WHERE e.episode_id=record_id AND e.revision=metadata_outbox.revision)) OR "
            "(record_family='aggregate_contribution' AND NOT EXISTS (SELECT 1 FROM "
            "aggregate_contribution_revisions a WHERE a.contribution_id=record_id AND "
            "a.revision=metadata_outbox.revision))); COMMIT;");
    }
    if (result.code_ != status_code::ok) {
        (void)vqec_vision_ai_stor_stsql_execute(catalog_, "ROLLBACK;");
        return result;
    }
    stats_.store_bytes_ = vqec_vision_ai_stor_stsql_get_regular_file_bytes(root);
    return {};
}

status sqlite_spatiotemporal_store::vqec_vision_ai_stor_stsql_get_stats(
    spatiotemporal_store_stats& _stats) const {
    if (catalog_ == nullptr) {
        return {status_code::invalid_state, "spatiotemporal store is not open"};
    }
    _stats = stats_;
    return {};
}

status sqlite_spatiotemporal_store::vqec_vision_ai_stor_stsql_recover_index() {
    // Complete the second phase of any shard retirement interrupted after the durable
    // `retiring` marker. Retrying removal is safe; the catalog index is removed only after
    // the detail file no longer exists.
    sqlite_statement retiring_query_statement;
    auto result = vqec_vision_ai_stor_stsql_prepare(catalog_,
        "SELECT shard_id,relative_path FROM shard_manifests WHERE state='retiring';",
        retiring_query_statement);
    if (result.code_ != status_code::ok) {
        return result;
    }
    std::vector<std::pair<std::string, std::string>> retiring_shards;
    while (sqlite3_step(retiring_query_statement.vqec_vision_ai_stor_stsql_get()) ==
           SQLITE_ROW) {
        retiring_shards.emplace_back(
            vqec_vision_ai_stor_stsql_read_text(
                retiring_query_statement.vqec_vision_ai_stor_stsql_get(), 0),
            vqec_vision_ai_stor_stsql_read_text(
                retiring_query_statement.vqec_vision_ai_stor_stsql_get(), 1));
    }
    const std::filesystem::path root(config_.root_directory_);
    for (const auto& shard : retiring_shards) {
        std::error_code remove_error;
        const auto path = root / shard.second;
        if (std::filesystem::exists(path, remove_error) && !remove_error) {
            (void)std::filesystem::remove(path, remove_error);
        }
        if (remove_error || std::filesystem::exists(path, remove_error) || remove_error) {
            return {status_code::io_error, "recover retiring trajectory shard failed"};
        }
        result = vqec_vision_ai_stor_stsql_execute(catalog_, "BEGIN IMMEDIATE;");
        if (result.code_ != status_code::ok) {
            return result;
        }
        sqlite_statement delete_statement;
        result = vqec_vision_ai_stor_stsql_prepare(catalog_,
            "DELETE FROM chunk_index WHERE shard_id=?1;", delete_statement);
        auto* delete_rows = delete_statement.vqec_vision_ai_stor_stsql_get();
        if (result.code_ == status_code::ok &&
            vqec_vision_ai_stor_stsql_bind_text(delete_rows, 1, shard.first) &&
            sqlite3_step(delete_rows) == SQLITE_DONE) {
            sqlite_statement retire_statement;
            result = vqec_vision_ai_stor_stsql_prepare(catalog_,
                "UPDATE shard_manifests SET state='retired',committed_bytes=0 "
                "WHERE shard_id=?1;", retire_statement);
            auto* retire = retire_statement.vqec_vision_ai_stor_stsql_get();
            if (result.code_ == status_code::ok &&
                vqec_vision_ai_stor_stsql_bind_text(retire, 1, shard.first) &&
                sqlite3_step(retire) == SQLITE_DONE) {
                result = vqec_vision_ai_stor_stsql_execute(catalog_, "COMMIT;");
            }
        }
        if (result.code_ != status_code::ok) {
            (void)vqec_vision_ai_stor_stsql_execute(catalog_, "ROLLBACK;");
            return result;
        }
        ++stats_.retired_shards_;
    }
    sqlite_statement manifest_statement;
    result = vqec_vision_ai_stor_stsql_prepare(catalog_,
        "SELECT shard_id,relative_path FROM shard_manifests "
        "WHERE state NOT IN ('missing','retired');",
        manifest_statement);
    if (result.code_ != status_code::ok) {
        return result;
    }
    while (sqlite3_step(manifest_statement.vqec_vision_ai_stor_stsql_get()) == SQLITE_ROW) {
        const auto shard_id = vqec_vision_ai_stor_stsql_read_text(
            manifest_statement.vqec_vision_ai_stor_stsql_get(), 0);
        const auto relative_path = vqec_vision_ai_stor_stsql_read_text(
            manifest_statement.vqec_vision_ai_stor_stsql_get(), 1);
        const auto path = (root / relative_path).string();
        sqlite3* detail = nullptr;
        if (sqlite3_open_v2(path.c_str(), &detail,
                SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr) != SQLITE_OK) {
            if (detail != nullptr) {
                sqlite3_close(detail);
            }
            sqlite_statement missing_statement;
            result = vqec_vision_ai_stor_stsql_prepare(catalog_,
                "UPDATE shard_manifests SET state='missing' WHERE shard_id=?1;",
                missing_statement);
            if (result.code_ == status_code::ok &&
                vqec_vision_ai_stor_stsql_bind_text(
                    missing_statement.vqec_vision_ai_stor_stsql_get(), 1, shard_id)) {
                (void)sqlite3_step(missing_statement.vqec_vision_ai_stor_stsql_get());
            }
            continue;
        }
        sqlite_statement chunk_statement;
        result = vqec_vision_ai_stor_stsql_prepare(detail,
            "SELECT chunk_id,global_sequence,source_id,subject_ref,entity_category,first_pts_ns,"
            "last_pts_ns,resolution,required_access_mask,bounds_left,bounds_top,bounds_right,"
            "bounds_bottom,length(encoded_points),checksum_crc32 FROM trajectory_chunks;",
            chunk_statement);
        if (result.code_ != status_code::ok) {
            sqlite3_close(detail);
            return result;
        }
        while (sqlite3_step(chunk_statement.vqec_vision_ai_stor_stsql_get()) == SQLITE_ROW) {
            sqlite_statement insert_statement;
            result = vqec_vision_ai_stor_stsql_prepare(catalog_,
                "INSERT OR IGNORE INTO chunk_index(chunk_id,global_sequence,shard_id,source_id,"
                "subject_ref,entity_category,begin_ns,end_ns,resolution,required_access_mask,"
                "bounds_left,bounds_top,bounds_right,bounds_bottom,encoded_bytes,checksum_crc32)"
                " VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16);",
                insert_statement);
            auto* insert = insert_statement.vqec_vision_ai_stor_stsql_get();
            auto* row = chunk_statement.vqec_vision_ai_stor_stsql_get();
            const bool is_bound = result.code_ == status_code::ok &&
                vqec_vision_ai_stor_stsql_bind_text(insert, 1,
                    vqec_vision_ai_stor_stsql_read_text(row, 0)) &&
                sqlite3_bind_int64(insert, 2, sqlite3_column_int64(row, 1)) == SQLITE_OK &&
                vqec_vision_ai_stor_stsql_bind_text(insert, 3, shard_id) &&
                vqec_vision_ai_stor_stsql_bind_text(insert, 4,
                    vqec_vision_ai_stor_stsql_read_text(row, 2)) &&
                vqec_vision_ai_stor_stsql_bind_text(insert, 5,
                    vqec_vision_ai_stor_stsql_read_text(row, 3)) &&
                vqec_vision_ai_stor_stsql_bind_text(insert, 6,
                    vqec_vision_ai_stor_stsql_read_text(row, 4));
            for (int column = 5; is_bound && column < 15; ++column) {
                if (sqlite3_bind_int64(insert, column + 2,
                        sqlite3_column_int64(row, column)) != SQLITE_OK) {
                    sqlite3_close(detail);
                    return vqec_vision_ai_stor_stsql_make_error(catalog_, "bind recovered index");
                }
            }
            if (!is_bound || sqlite3_step(insert) != SQLITE_DONE) {
                sqlite3_close(detail);
                return result.code_ == status_code::ok
                           ? vqec_vision_ai_stor_stsql_make_error(catalog_, "recover chunk index")
                           : result;
            }
            if (sqlite3_changes(catalog_) != 0) {
                ++stats_.orphan_chunks_recovered_;
            }
        }
        sqlite3_close(detail);
    }
    return {};
}

}  // namespace vqec::vision::ai
