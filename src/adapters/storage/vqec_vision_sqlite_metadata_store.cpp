#include "vqec_vision_sqlite_metadata_store.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <limits>
#include <sstream>
#include <utility>

namespace vqec::vision::ai {
namespace {

constexpr const char* g_metadata_outbox_pending = "pending";
constexpr const char* g_metadata_coverage_complete = "complete";
constexpr const char* g_metadata_coverage_missing = "coverage_gap";

constexpr const char* g_metadata_schema_sql = R"sql(
CREATE TABLE IF NOT EXISTS metadata_records (
    sequence INTEGER PRIMARY KEY AUTOINCREMENT,
    record_id TEXT NOT NULL,
    revision INTEGER NOT NULL,
    supersedes_revision INTEGER NOT NULL,
    family INTEGER NOT NULL,
    device_id TEXT NOT NULL,
    source_id TEXT NOT NULL,
    boot_id TEXT NOT NULL,
    source_epoch INTEGER NOT NULL,
    subject_ref TEXT NOT NULL,
    object_ref TEXT NOT NULL,
    scene_ref TEXT NOT NULL,
    semantic_type TEXT NOT NULL,
    typed_value TEXT NOT NULL,
    value_state INTEGER NOT NULL,
    valid_begin_ns INTEGER NOT NULL,
    valid_end_ns INTEGER NOT NULL,
    recorded_ns INTEGER NOT NULL,
    clock_uncertainty_ns INTEGER NOT NULL,
    confidence_ppm INTEGER NOT NULL,
    sensitivity_scope INTEGER NOT NULL,
    producer_revision TEXT NOT NULL,
    payload TEXT NOT NULL,
    tombstone INTEGER NOT NULL,
    UNIQUE(record_id, revision)
);
CREATE INDEX IF NOT EXISTS metadata_source_family_time_idx
    ON metadata_records(source_id, family, valid_begin_ns, sequence);
CREATE INDEX IF NOT EXISTS metadata_subject_time_idx
    ON metadata_records(subject_ref, valid_begin_ns, valid_end_ns, sequence);
CREATE INDEX IF NOT EXISTS metadata_attribute_time_idx
    ON metadata_records(family, semantic_type, typed_value, valid_begin_ns, valid_end_ns);
CREATE INDEX IF NOT EXISTS metadata_scene_time_idx
    ON metadata_records(scene_ref, valid_begin_ns, sequence);
CREATE TABLE IF NOT EXISTS metadata_outbox (
    sink_id TEXT NOT NULL,
    record_id TEXT NOT NULL,
    record_revision INTEGER NOT NULL,
    state TEXT NOT NULL,
    attempt_revision INTEGER NOT NULL,
    PRIMARY KEY(sink_id, record_id, record_revision)
);
CREATE INDEX IF NOT EXISTS metadata_outbox_state_idx
    ON metadata_outbox(sink_id, state, record_revision);
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

    sqlite3_stmt** vqec_vision_ai_stor_mdsql_get_output() noexcept {
        return &statement_;
    }

    sqlite3_stmt* vqec_vision_ai_stor_mdsql_get() const noexcept {
        return statement_;
    }

private:
    sqlite3_stmt* statement_{nullptr};
};

status vqec_vision_ai_stor_mdsql_make_error(sqlite3* _database, const char* _operation) {
    const char* detail = _database == nullptr ? "database is not open" : sqlite3_errmsg(_database);
    return {status_code::io_error, std::string(_operation) + ": " + detail};
}

status vqec_vision_ai_stor_mdsql_execute(sqlite3* _database, const char* _sql) {
    char* error_message = nullptr;
    const auto result = sqlite3_exec(_database, _sql, nullptr, nullptr, &error_message);
    if (result == SQLITE_OK) {
        return {};
    }
    std::string message = error_message == nullptr ? sqlite3_errmsg(_database) : error_message;
    sqlite3_free(error_message);
    return {status_code::io_error, std::move(message)};
}

status vqec_vision_ai_stor_mdsql_prepare(
    sqlite3* _database, const std::string& _sql, sqlite_statement& _statement) {
    if (sqlite3_prepare_v2(_database, _sql.c_str(), static_cast<int>(_sql.size()),
            _statement.vqec_vision_ai_stor_mdsql_get_output(), nullptr) != SQLITE_OK) {
        return vqec_vision_ai_stor_mdsql_make_error(_database, "prepare metadata statement");
    }
    return {};
}

bool vqec_vision_ai_stor_mdsql_bind_text(
    sqlite3_stmt* _statement, int _index, const std::string& _value) {
    return sqlite3_bind_text(_statement, _index, _value.data(),
               static_cast<int>(_value.size()), SQLITE_TRANSIENT) == SQLITE_OK;
}

std::string vqec_vision_ai_stor_mdsql_read_text(sqlite3_stmt* _statement, int _column) {
    const auto* value = sqlite3_column_text(_statement, _column);
    const auto bytes = sqlite3_column_bytes(_statement, _column);
    return value == nullptr || bytes <= 0
               ? std::string{}
               : std::string(reinterpret_cast<const char*>(value), static_cast<std::size_t>(bytes));
}

std::uint32_t vqec_vision_ai_stor_mdsql_get_required_scope(metadata_query_kind _kind) {
    switch (_kind) {
    case metadata_query_kind::q01_object_attribute:
    case metadata_query_kind::q02_attribute_at_event:
        return vqec_vision_ai_cntr_mdqry_get_scope_mask(metadata_scope::visual_attribute);
    case metadata_query_kind::q03_trajectory_detail:
    case metadata_query_kind::q04_ordered_passage:
    case metadata_query_kind::q05_presence_dwell:
    case metadata_query_kind::q19_lane_movement:
    case metadata_query_kind::q24_origin_destination:
    case metadata_query_kind::q25_retroactive_geometry:
        return vqec_vision_ai_cntr_mdqry_get_scope_mask(metadata_scope::trajectory);
    case metadata_query_kind::q09_recognition:
    case metadata_query_kind::q10_attendance:
        return vqec_vision_ai_cntr_mdqry_get_scope_mask(metadata_scope::identity);
    case metadata_query_kind::q12_plate_search:
    case metadata_query_kind::q13_plate_fuzzy:
    case metadata_query_kind::q20_parking:
        return vqec_vision_ai_cntr_mdqry_get_scope_mask(metadata_scope::plate);
    case metadata_query_kind::q14_flow_heatmap:
    case metadata_query_kind::q15_trend:
    case metadata_query_kind::q18_vehicle_flow:
    case metadata_query_kind::q22_congestion:
        return vqec_vision_ai_cntr_mdqry_get_scope_mask(metadata_scope::aggregate);
    case metadata_query_kind::q16_similarity:
        return vqec_vision_ai_cntr_mdqry_get_scope_mask(metadata_scope::biometric);
    case metadata_query_kind::q26_explain:
    case metadata_query_kind::q28_export:
    case metadata_query_kind::q29_correction_purge:
    case metadata_query_kind::q30_recompute:
        return vqec_vision_ai_cntr_mdqry_get_scope_mask(metadata_scope::audit);
    case metadata_query_kind::q27_coverage:
        return vqec_vision_ai_cntr_mdqry_get_scope_mask(metadata_scope::operational);
    default:
        return vqec_vision_ai_cntr_mdqry_get_scope_mask(metadata_scope::object);
    }
}

bool vqec_vision_ai_stor_mdsql_is_supported(metadata_query_kind _kind) {
    switch (_kind) {
    case metadata_query_kind::q01_object_attribute:
    case metadata_query_kind::q02_attribute_at_event:
    case metadata_query_kind::q03_trajectory_detail:
    case metadata_query_kind::q04_ordered_passage:
    case metadata_query_kind::q05_presence_dwell:
    case metadata_query_kind::q06_relation:
    case metadata_query_kind::q08_event:
    case metadata_query_kind::q12_plate_search:
    case metadata_query_kind::q14_flow_heatmap:
    case metadata_query_kind::q15_trend:
    case metadata_query_kind::q18_vehicle_flow:
    case metadata_query_kind::q26_explain:
    case metadata_query_kind::q27_coverage:
    case metadata_query_kind::q28_export:
    case metadata_query_kind::q29_correction_purge:
        return true;
    default:
        return false;
    }
}

std::vector<int> vqec_vision_ai_stor_mdsql_get_families(metadata_query_kind _kind) {
    using family = metadata_record_family;
    switch (_kind) {
    case metadata_query_kind::q01_object_attribute:
        return {static_cast<int>(family::attribute_assertion)};
    case metadata_query_kind::q03_trajectory_detail:
        return {static_cast<int>(family::track_segment),
            static_cast<int>(family::trajectory_chunk),
            static_cast<int>(family::evidence_reference)};
    case metadata_query_kind::q04_ordered_passage:
    case metadata_query_kind::q05_presence_dwell:
        return {static_cast<int>(family::passage_presence)};
    case metadata_query_kind::q06_relation:
        return {static_cast<int>(family::relation_interval),
            static_cast<int>(family::association_hypothesis)};
    case metadata_query_kind::q08_event:
        return {static_cast<int>(family::event_episode),
            static_cast<int>(family::evidence_reference)};
    case metadata_query_kind::q12_plate_search:
        return {static_cast<int>(family::plate_read_consensus)};
    case metadata_query_kind::q14_flow_heatmap:
    case metadata_query_kind::q15_trend:
    case metadata_query_kind::q18_vehicle_flow:
        return {static_cast<int>(family::aggregate_bucket),
            static_cast<int>(family::passage_presence)};
    case metadata_query_kind::q27_coverage:
        return {static_cast<int>(family::coverage_health_interval),
            static_cast<int>(family::delivery_archive_audit)};
    case metadata_query_kind::q29_correction_purge:
        return {static_cast<int>(family::delivery_archive_audit)};
    case metadata_query_kind::q26_explain:
    case metadata_query_kind::q28_export:
        return {static_cast<int>(family::source_scene_revision),
            static_cast<int>(family::observation_sample),
            static_cast<int>(family::track_segment),
            static_cast<int>(family::trajectory_chunk),
            static_cast<int>(family::attribute_assertion),
            static_cast<int>(family::relation_interval),
            static_cast<int>(family::passage_presence),
            static_cast<int>(family::measurement_sample),
            static_cast<int>(family::external_state_interval),
            static_cast<int>(family::event_episode),
            static_cast<int>(family::recognition_attendance),
            static_cast<int>(family::plate_read_consensus),
            static_cast<int>(family::context_assessment),
            static_cast<int>(family::aggregate_bucket),
            static_cast<int>(family::evidence_reference),
            static_cast<int>(family::association_hypothesis),
            static_cast<int>(family::coverage_health_interval),
            static_cast<int>(family::delivery_archive_audit)};
    default:
        return {};
    }
}

metadata_record vqec_vision_ai_stor_mdsql_read_record(sqlite3_stmt* _statement) {
    metadata_record record;
    record.record_id_ = vqec_vision_ai_stor_mdsql_read_text(_statement, 1);
    record.revision_ = static_cast<std::uint64_t>(sqlite3_column_int64(_statement, 2));
    record.supersedes_revision_ = static_cast<std::uint64_t>(sqlite3_column_int64(_statement, 3));
    record.family_ = static_cast<metadata_record_family>(sqlite3_column_int(_statement, 4));
    record.device_id_ = vqec_vision_ai_stor_mdsql_read_text(_statement, 5);
    record.source_id_ = vqec_vision_ai_stor_mdsql_read_text(_statement, 6);
    record.boot_id_ = vqec_vision_ai_stor_mdsql_read_text(_statement, 7);
    record.source_epoch_ = static_cast<std::uint64_t>(sqlite3_column_int64(_statement, 8));
    record.subject_ref_ = vqec_vision_ai_stor_mdsql_read_text(_statement, 9);
    record.object_ref_ = vqec_vision_ai_stor_mdsql_read_text(_statement, 10);
    record.scene_ref_ = vqec_vision_ai_stor_mdsql_read_text(_statement, 11);
    record.semantic_type_ = vqec_vision_ai_stor_mdsql_read_text(_statement, 12);
    record.typed_value_ = vqec_vision_ai_stor_mdsql_read_text(_statement, 13);
    record.value_state_ = static_cast<metadata_value_state>(sqlite3_column_int(_statement, 14));
    record.valid_begin_ns_ = sqlite3_column_int64(_statement, 15);
    record.valid_end_ns_ = sqlite3_column_int64(_statement, 16);
    record.recorded_ns_ = sqlite3_column_int64(_statement, 17);
    record.clock_uncertainty_ns_ = static_cast<std::uint64_t>(sqlite3_column_int64(_statement, 18));
    record.confidence_ppm_ = static_cast<std::uint32_t>(sqlite3_column_int(_statement, 19));
    record.sensitivity_scope_ = static_cast<metadata_scope>(sqlite3_column_int64(_statement, 20));
    record.producer_revision_ = vqec_vision_ai_stor_mdsql_read_text(_statement, 21);
    record.payload_ = vqec_vision_ai_stor_mdsql_read_text(_statement, 22);
    record.is_tombstone_ = sqlite3_column_int(_statement, 23) != 0;
    return record;
}

bool vqec_vision_ai_stor_mdsql_records_match(
    const metadata_record& _left, const metadata_record& _right) {
    return _left.record_id_ == _right.record_id_ && _left.revision_ == _right.revision_ &&
           _left.supersedes_revision_ == _right.supersedes_revision_ &&
           _left.family_ == _right.family_ && _left.device_id_ == _right.device_id_ &&
           _left.source_id_ == _right.source_id_ && _left.boot_id_ == _right.boot_id_ &&
           _left.source_epoch_ == _right.source_epoch_ &&
           _left.subject_ref_ == _right.subject_ref_ && _left.object_ref_ == _right.object_ref_ &&
           _left.scene_ref_ == _right.scene_ref_ &&
           _left.semantic_type_ == _right.semantic_type_ &&
           _left.typed_value_ == _right.typed_value_ &&
           _left.value_state_ == _right.value_state_ &&
           _left.valid_begin_ns_ == _right.valid_begin_ns_ &&
           _left.valid_end_ns_ == _right.valid_end_ns_ &&
           _left.recorded_ns_ == _right.recorded_ns_ &&
           _left.clock_uncertainty_ns_ == _right.clock_uncertainty_ns_ &&
           _left.confidence_ppm_ == _right.confidence_ppm_ &&
           _left.sensitivity_scope_ == _right.sensitivity_scope_ &&
           _left.producer_revision_ == _right.producer_revision_ &&
           _left.payload_ == _right.payload_ && _left.is_tombstone_ == _right.is_tombstone_;
}

bool vqec_vision_ai_stor_mdsql_has_projection(
    const metadata_query_request& _request, const char* _field) {
    return std::find(_request.projection_.begin(), _request.projection_.end(), _field) !=
           _request.projection_.end();
}

void vqec_vision_ai_stor_mdsql_apply_projection(
    const metadata_query_request& _request, metadata_record& _record) {
    if (!vqec_vision_ai_stor_mdsql_has_projection(_request, "subject_ref")) {
        _record.subject_ref_.clear();
    }
    if (!vqec_vision_ai_stor_mdsql_has_projection(_request, "object_ref")) {
        _record.object_ref_.clear();
    }
    if (!vqec_vision_ai_stor_mdsql_has_projection(_request, "scene_ref")) {
        _record.scene_ref_.clear();
    }
    if (!vqec_vision_ai_stor_mdsql_has_projection(_request, "semantic_type")) {
        _record.semantic_type_.clear();
    }
    if (!vqec_vision_ai_stor_mdsql_has_projection(_request, "typed_value")) {
        _record.typed_value_.clear();
    }
    if (!vqec_vision_ai_stor_mdsql_has_projection(_request, "producer_revision")) {
        _record.producer_revision_.clear();
    }
    if (!vqec_vision_ai_stor_mdsql_has_projection(_request, "payload")) {
        _record.payload_.clear();
    }
}

constexpr const char* g_metadata_select_columns =
    "r.sequence,r.record_id,r.revision,r.supersedes_revision,r.family,r.device_id,r.source_id,"
    "r.boot_id,r.source_epoch,r.subject_ref,r.object_ref,r.scene_ref,r.semantic_type,"
    "r.typed_value,r.value_state,r.valid_begin_ns,r.valid_end_ns,r.recorded_ns,"
    "r.clock_uncertainty_ns,r.confidence_ppm,r.sensitivity_scope,r.producer_revision,r.payload,"
    "r.tombstone";

}  // namespace

sqlite_metadata_store::sqlite_metadata_store(sqlite_metadata_store_config _config)
    : config_(std::move(_config)) {}

sqlite_metadata_store::~sqlite_metadata_store() noexcept {
    (void)vqec_vision_ai_stor_mdsql_close();
}

status sqlite_metadata_store::vqec_vision_ai_stor_mdsql_open() {
    if (database_ != nullptr) {
        return {status_code::invalid_state, "metadata store is already open"};
    }
    if (config_.database_path_.empty() || config_.max_payload_bytes_ == 0U ||
        config_.max_page_size_ == 0U || config_.busy_timeout_ms_ == 0U ||
        config_.wal_autocheckpoint_pages_ == 0U ||
        config_.max_payload_bytes_ > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        config_.max_page_size_ >= static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        config_.busy_timeout_ms_ > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        config_.wal_autocheckpoint_pages_ >
            static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        return {status_code::invalid_argument, "metadata store configuration is invalid"};
    }
    if (sqlite3_open_v2(config_.database_path_.c_str(), &database_,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, nullptr) != SQLITE_OK) {
        auto error = vqec_vision_ai_stor_mdsql_make_error(database_, "open metadata database");
        (void)vqec_vision_ai_stor_mdsql_close();
        return error;
    }
    if (sqlite3_busy_timeout(database_, static_cast<int>(config_.busy_timeout_ms_)) != SQLITE_OK) {
        return vqec_vision_ai_stor_mdsql_make_error(database_, "set metadata busy timeout");
    }
    auto result = vqec_vision_ai_stor_mdsql_execute(database_, "PRAGMA journal_mode=WAL;");
    if (result.code_ != status_code::ok) {
        return result;
    }
    result = vqec_vision_ai_stor_mdsql_execute(
        database_, config_.is_full_sync_ ? "PRAGMA synchronous=FULL;" : "PRAGMA synchronous=NORMAL;");
    if (result.code_ != status_code::ok) {
        return result;
    }
    const std::string checkpoint_sql =
        "PRAGMA wal_autocheckpoint=" + std::to_string(config_.wal_autocheckpoint_pages_) + ";";
    result = vqec_vision_ai_stor_mdsql_execute(database_, checkpoint_sql.c_str());
    if (result.code_ != status_code::ok) {
        return result;
    }
    return vqec_vision_ai_stor_mdsql_execute(database_, g_metadata_schema_sql);
}

status sqlite_metadata_store::vqec_vision_ai_stor_mdsql_close() noexcept {
    if (database_ == nullptr) {
        return {};
    }
    if (sqlite3_close(database_) != SQLITE_OK) {
        return vqec_vision_ai_stor_mdsql_make_error(database_, "close metadata database");
    }
    database_ = nullptr;
    return {};
}

status sqlite_metadata_store::vqec_vision_ai_stor_mdsql_ingest_record(
    const metadata_record& _record, const std::vector<std::string>& _outbox_sinks) {
    if (database_ == nullptr) {
        return {status_code::invalid_state, "metadata store is not open"};
    }
    auto result = vqec_vision_ai_cntr_mdqry_validate_record(_record, config_.max_payload_bytes_);
    if (result.code_ != status_code::ok) {
        return result;
    }
    if (_outbox_sinks.size() > g_metadata_query_max_outbox_sinks) {
        return {status_code::invalid_argument, "metadata outbox sink count exceeds contract"};
    }
    std::vector<std::string> sinks = _outbox_sinks;
    std::sort(sinks.begin(), sinks.end());
    if (std::adjacent_find(sinks.begin(), sinks.end()) != sinks.end() ||
        std::any_of(sinks.begin(), sinks.end(), [](const auto& _sink) {
            return _sink.empty() || _sink.size() > g_metadata_query_max_identifier_bytes;
        })) {
        return {status_code::invalid_argument, "metadata outbox sink identity is invalid"};
    }
    result = vqec_vision_ai_stor_mdsql_execute(database_, "BEGIN IMMEDIATE;");
    if (result.code_ != status_code::ok) {
        return result;
    }
    const auto rollback = [this]() {
        (void)vqec_vision_ai_stor_mdsql_execute(database_, "ROLLBACK;");
    };

    std::uint64_t latest_revision = 0U;
    {
        sqlite_statement revision_statement;
        result = vqec_vision_ai_stor_mdsql_prepare(database_,
            "SELECT COALESCE(MAX(revision),0) FROM metadata_records WHERE record_id=?1;",
            revision_statement);
        if (result.code_ != status_code::ok ||
            !vqec_vision_ai_stor_mdsql_bind_text(
                revision_statement.vqec_vision_ai_stor_mdsql_get(), 1, _record.record_id_)) {
            rollback();
            return result.code_ == status_code::ok
                       ? vqec_vision_ai_stor_mdsql_make_error(database_, "bind metadata revision")
                       : result;
        }
        const auto revision_step = sqlite3_step(revision_statement.vqec_vision_ai_stor_mdsql_get());
        if (revision_step != SQLITE_ROW) {
            rollback();
            return vqec_vision_ai_stor_mdsql_make_error(database_, "read metadata revision");
        }
        latest_revision = static_cast<std::uint64_t>(
            sqlite3_column_int64(revision_statement.vqec_vision_ai_stor_mdsql_get(), 0));
    }
    if (latest_revision >= _record.revision_) {
        bool is_idempotent = false;
        if (latest_revision == _record.revision_) {
            sqlite_statement existing_statement;
            const std::string existing_sql = std::string("SELECT ") +
                g_metadata_select_columns +
                " FROM metadata_records r WHERE r.record_id=?1 AND r.revision=?2;";
            result = vqec_vision_ai_stor_mdsql_prepare(
                database_, existing_sql, existing_statement);
            auto* existing = existing_statement.vqec_vision_ai_stor_mdsql_get();
            if (result.code_ == status_code::ok &&
                vqec_vision_ai_stor_mdsql_bind_text(existing, 1, _record.record_id_) &&
                sqlite3_bind_int64(existing, 2,
                    static_cast<sqlite3_int64>(_record.revision_)) == SQLITE_OK &&
                sqlite3_step(existing) == SQLITE_ROW) {
                is_idempotent = vqec_vision_ai_stor_mdsql_records_match(
                    vqec_vision_ai_stor_mdsql_read_record(existing), _record);
            }
        }
        rollback();
        if (is_idempotent) {
            return {};
        }
        return {status_code::invalid_argument,
            latest_revision == _record.revision_
                ? "metadata idempotency key conflicts with existing payload"
                : "metadata revision is stale"};
    }
    if (latest_revision != _record.supersedes_revision_) {
        rollback();
        return {status_code::invalid_argument, "metadata supersedes revision is not current"};
    }

    constexpr const char* insert_sql =
        "INSERT INTO metadata_records(record_id,revision,supersedes_revision,family,device_id,"
        "source_id,boot_id,source_epoch,subject_ref,object_ref,scene_ref,semantic_type,typed_value,"
        "value_state,valid_begin_ns,valid_end_ns,recorded_ns,clock_uncertainty_ns,confidence_ppm,"
        "sensitivity_scope,producer_revision,payload,tombstone) VALUES("
        "?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20,?21,?22,?23);";
    sqlite_statement insert_statement;
    result = vqec_vision_ai_stor_mdsql_prepare(database_, insert_sql, insert_statement);
    auto* statement = insert_statement.vqec_vision_ai_stor_mdsql_get();
    const bool is_bound = result.code_ == status_code::ok &&
        vqec_vision_ai_stor_mdsql_bind_text(statement, 1, _record.record_id_) &&
        sqlite3_bind_int64(statement, 2, static_cast<sqlite3_int64>(_record.revision_)) == SQLITE_OK &&
        sqlite3_bind_int64(statement, 3,
            static_cast<sqlite3_int64>(_record.supersedes_revision_)) == SQLITE_OK &&
        sqlite3_bind_int(statement, 4, static_cast<int>(_record.family_)) == SQLITE_OK &&
        vqec_vision_ai_stor_mdsql_bind_text(statement, 5, _record.device_id_) &&
        vqec_vision_ai_stor_mdsql_bind_text(statement, 6, _record.source_id_) &&
        vqec_vision_ai_stor_mdsql_bind_text(statement, 7, _record.boot_id_) &&
        sqlite3_bind_int64(statement, 8,
            static_cast<sqlite3_int64>(_record.source_epoch_)) == SQLITE_OK &&
        vqec_vision_ai_stor_mdsql_bind_text(statement, 9, _record.subject_ref_) &&
        vqec_vision_ai_stor_mdsql_bind_text(statement, 10, _record.object_ref_) &&
        vqec_vision_ai_stor_mdsql_bind_text(statement, 11, _record.scene_ref_) &&
        vqec_vision_ai_stor_mdsql_bind_text(statement, 12, _record.semantic_type_) &&
        vqec_vision_ai_stor_mdsql_bind_text(statement, 13, _record.typed_value_) &&
        sqlite3_bind_int(statement, 14, static_cast<int>(_record.value_state_)) == SQLITE_OK &&
        sqlite3_bind_int64(statement, 15, _record.valid_begin_ns_) == SQLITE_OK &&
        sqlite3_bind_int64(statement, 16, _record.valid_end_ns_) == SQLITE_OK &&
        sqlite3_bind_int64(statement, 17, _record.recorded_ns_) == SQLITE_OK &&
        sqlite3_bind_int64(statement, 18,
            static_cast<sqlite3_int64>(_record.clock_uncertainty_ns_)) == SQLITE_OK &&
        sqlite3_bind_int64(statement, 19,
            static_cast<sqlite3_int64>(_record.confidence_ppm_)) == SQLITE_OK &&
        sqlite3_bind_int64(statement, 20,
            static_cast<sqlite3_int64>(vqec_vision_ai_cntr_mdqry_get_scope_mask(
                _record.sensitivity_scope_))) == SQLITE_OK &&
        vqec_vision_ai_stor_mdsql_bind_text(statement, 21, _record.producer_revision_) &&
        vqec_vision_ai_stor_mdsql_bind_text(statement, 22, _record.payload_) &&
        sqlite3_bind_int(statement, 23, _record.is_tombstone_ ? 1 : 0) == SQLITE_OK;
    if (!is_bound || sqlite3_step(statement) != SQLITE_DONE) {
        rollback();
        return result.code_ == status_code::ok
                   ? vqec_vision_ai_stor_mdsql_make_error(database_, "insert metadata record")
                   : result;
    }

    for (const auto& sink : sinks) {
        sqlite_statement outbox_statement;
        result = vqec_vision_ai_stor_mdsql_prepare(database_,
            "INSERT INTO metadata_outbox(sink_id,record_id,record_revision,state,attempt_revision) "
            "VALUES(?1,?2,?3,?4,0);", outbox_statement);
        auto* outbox = outbox_statement.vqec_vision_ai_stor_mdsql_get();
        if (result.code_ != status_code::ok ||
            !vqec_vision_ai_stor_mdsql_bind_text(outbox, 1, sink) ||
            !vqec_vision_ai_stor_mdsql_bind_text(outbox, 2, _record.record_id_) ||
            sqlite3_bind_int64(outbox, 3,
                static_cast<sqlite3_int64>(_record.revision_)) != SQLITE_OK ||
            !vqec_vision_ai_stor_mdsql_bind_text(outbox, 4, g_metadata_outbox_pending) ||
            sqlite3_step(outbox) != SQLITE_DONE) {
            rollback();
            return result.code_ == status_code::ok
                       ? vqec_vision_ai_stor_mdsql_make_error(database_, "insert metadata outbox")
                       : result;
        }
    }
    result = vqec_vision_ai_stor_mdsql_execute(database_, "COMMIT;");
    if (result.code_ != status_code::ok) {
        rollback();
    }
    return result;
}

status sqlite_metadata_store::vqec_vision_ai_stor_mdsql_query_records(
    const metadata_query_request& _request, metadata_query_page& _page) {
    _page = {};
    if (database_ == nullptr) {
        return {status_code::invalid_state, "metadata store is not open"};
    }
    auto result = vqec_vision_ai_cntr_mdqry_validate_request(_request, config_.max_page_size_);
    if (result.code_ != status_code::ok) {
        return result;
    }
    if (!vqec_vision_ai_stor_mdsql_is_supported(_request.kind_)) {
        _page.completeness_ = metadata_query_completeness::unsupported;
        return {status_code::unsupported, "metadata query capability is not implemented"};
    }
    const auto required_scope = vqec_vision_ai_stor_mdsql_get_required_scope(_request.kind_);
    if ((_request.allowed_scope_mask_ & required_scope) != required_scope) {
        return {status_code::unauthorized, "metadata query scope is not authorized"};
    }

    std::uint64_t snapshot_sequence = _request.snapshot_sequence_;
    if (snapshot_sequence == 0U) {
        sqlite_statement snapshot_statement;
        result = vqec_vision_ai_stor_mdsql_prepare(
            database_, "SELECT COALESCE(MAX(sequence),0) FROM metadata_records;", snapshot_statement);
        if (result.code_ != status_code::ok ||
            sqlite3_step(snapshot_statement.vqec_vision_ai_stor_mdsql_get()) != SQLITE_ROW) {
            return result.code_ == status_code::ok
                       ? vqec_vision_ai_stor_mdsql_make_error(database_, "read metadata snapshot")
                       : result;
        }
        snapshot_sequence = static_cast<std::uint64_t>(sqlite3_column_int64(
            snapshot_statement.vqec_vision_ai_stor_mdsql_get(), 0));
    }

    const auto families = _request.kind_ == metadata_query_kind::q02_attribute_at_event
                              ? std::vector<int>{static_cast<int>(
                                    metadata_record_family::passage_presence)}
                              : vqec_vision_ai_stor_mdsql_get_families(_request.kind_);
    if (families.empty()) {
        _page.completeness_ = metadata_query_completeness::unsupported;
        return {status_code::unsupported, "metadata query has no local access path"};
    }

    std::ostringstream sql;
    sql << "SELECT " << g_metadata_select_columns << " FROM metadata_records r ";
    if (_request.kind_ == metadata_query_kind::q02_attribute_at_event) {
        sql << "JOIN metadata_records a ON a.family=?1 AND a.subject_ref=r.subject_ref "
               "AND a.valid_begin_ns<=r.valid_begin_ns "
               "AND (a.valid_end_ns=0 OR a.valid_end_ns>r.valid_begin_ns) ";
    }
    sql << "WHERE r.sequence<=? AND r.sequence>? AND r.tombstone=0 "
           "AND r.valid_begin_ns<? AND (r.valid_end_ns=0 OR r.valid_end_ns>?) "
           "AND (r.sensitivity_scope & ?)!=0 AND r.family IN (";
    for (std::size_t index = 0; index < families.size(); ++index) {
        sql << (index == 0U ? "?" : ",?");
    }
    sql << ") AND r.source_id IN (";
    for (std::size_t index = 0; index < _request.source_ids_.size(); ++index) {
        sql << (index == 0U ? "?" : ",?");
    }
    sql << ") AND (?='' OR r.subject_ref=?) AND (?='' OR r.scene_ref=?) ";
    if (_request.kind_ == metadata_query_kind::q02_attribute_at_event) {
        sql << "AND (?='' OR a.semantic_type=?) AND (?='' OR a.typed_value=?) "
               "AND (a.sensitivity_scope & ?)!=0 AND a.tombstone=0 AND a.sequence<=? "
               "AND NOT EXISTS(SELECT 1 FROM metadata_records newer_a WHERE "
               "newer_a.record_id=a.record_id AND newer_a.revision>a.revision "
               "AND newer_a.sequence<=?) ";
    } else {
        sql << "AND (?='' OR r.semantic_type=?) AND (?='' OR r.typed_value=?) ";
    }
    sql << "AND NOT EXISTS(SELECT 1 FROM metadata_records newer WHERE "
           "newer.record_id=r.record_id AND newer.revision>r.revision "
           "AND newer.sequence<=?) ORDER BY r.sequence LIMIT ?;";

    sqlite_statement query_statement;
    result = vqec_vision_ai_stor_mdsql_prepare(database_, sql.str(), query_statement);
    if (result.code_ != status_code::ok) {
        return result;
    }
    auto* query = query_statement.vqec_vision_ai_stor_mdsql_get();
    int bind_index = 1;
    if (_request.kind_ == metadata_query_kind::q02_attribute_at_event) {
        sqlite3_bind_int(query, bind_index++,
            static_cast<int>(metadata_record_family::attribute_assertion));
    }
    bool is_bound =
        sqlite3_bind_int64(query, bind_index++, static_cast<sqlite3_int64>(snapshot_sequence)) ==
            SQLITE_OK &&
        sqlite3_bind_int64(query, bind_index++,
            static_cast<sqlite3_int64>(_request.cursor_sequence_)) == SQLITE_OK &&
        sqlite3_bind_int64(query, bind_index++, _request.end_ns_) == SQLITE_OK &&
        sqlite3_bind_int64(query, bind_index++, _request.begin_ns_) == SQLITE_OK &&
        sqlite3_bind_int64(query, bind_index++,
            static_cast<sqlite3_int64>(_request.allowed_scope_mask_)) == SQLITE_OK;
    for (const auto family : families) {
        is_bound = is_bound && sqlite3_bind_int(query, bind_index++, family) == SQLITE_OK;
    }
    for (const auto& source : _request.source_ids_) {
        is_bound = is_bound && vqec_vision_ai_stor_mdsql_bind_text(query, bind_index++, source);
    }
    is_bound = is_bound &&
        vqec_vision_ai_stor_mdsql_bind_text(query, bind_index++, _request.subject_ref_) &&
        vqec_vision_ai_stor_mdsql_bind_text(query, bind_index++, _request.subject_ref_) &&
        vqec_vision_ai_stor_mdsql_bind_text(query, bind_index++, _request.scene_ref_) &&
        vqec_vision_ai_stor_mdsql_bind_text(query, bind_index++, _request.scene_ref_) &&
        vqec_vision_ai_stor_mdsql_bind_text(query, bind_index++, _request.semantic_type_) &&
        vqec_vision_ai_stor_mdsql_bind_text(query, bind_index++, _request.semantic_type_) &&
        vqec_vision_ai_stor_mdsql_bind_text(query, bind_index++, _request.typed_value_) &&
        vqec_vision_ai_stor_mdsql_bind_text(query, bind_index++, _request.typed_value_);
    if (_request.kind_ == metadata_query_kind::q02_attribute_at_event) {
        is_bound = is_bound &&
            sqlite3_bind_int64(query, bind_index++,
                static_cast<sqlite3_int64>(_request.allowed_scope_mask_)) == SQLITE_OK &&
            sqlite3_bind_int64(query, bind_index++,
                static_cast<sqlite3_int64>(snapshot_sequence)) == SQLITE_OK &&
            sqlite3_bind_int64(query, bind_index++,
                static_cast<sqlite3_int64>(snapshot_sequence)) == SQLITE_OK;
    }
    is_bound = is_bound &&
        sqlite3_bind_int64(query, bind_index++, static_cast<sqlite3_int64>(snapshot_sequence)) ==
            SQLITE_OK &&
        sqlite3_bind_int64(query, bind_index++,
            static_cast<sqlite3_int64>(_request.page_size_ + 1U)) == SQLITE_OK;
    if (!is_bound) {
        return vqec_vision_ai_stor_mdsql_make_error(database_, "bind metadata query");
    }

    std::vector<std::pair<std::uint64_t, metadata_record>> rows;
    rows.reserve(_request.page_size_ + 1U);
    int step = SQLITE_ROW;
    while ((step = sqlite3_step(query)) == SQLITE_ROW) {
        rows.emplace_back(static_cast<std::uint64_t>(sqlite3_column_int64(query, 0)),
            vqec_vision_ai_stor_mdsql_read_record(query));
    }
    if (step != SQLITE_DONE) {
        return vqec_vision_ai_stor_mdsql_make_error(database_, "execute metadata query");
    }
    _page.has_more_ = rows.size() > _request.page_size_;
    if (_page.has_more_) {
        rows.resize(_request.page_size_);
    }
    _page.records_.reserve(rows.size());
    for (auto& row : rows) {
        _page.next_cursor_sequence_ = row.first;
        vqec_vision_ai_stor_mdsql_apply_projection(_request, row.second);
        _page.records_.push_back(std::move(row.second));
    }
    _page.snapshot_sequence_ = snapshot_sequence;
    _page.authorization_revision_ = _request.authorization_revision_;

    std::ostringstream coverage_sql;
    coverage_sql << "SELECT COUNT(DISTINCT r.source_id) FROM metadata_records r WHERE r.family=?1 "
                    "AND r.typed_value=?2 AND r.valid_begin_ns<?3 "
                    "AND (r.valid_end_ns=0 OR r.valid_end_ns>?4) AND r.sequence<=?5 "
                    "AND r.tombstone=0 AND r.source_id IN (";
    for (std::size_t index = 0; index < _request.source_ids_.size(); ++index) {
        coverage_sql << (index == 0U ? "?" : ",?");
    }
    coverage_sql << ") AND NOT EXISTS(SELECT 1 FROM metadata_records newer WHERE "
                    "newer.record_id=r.record_id AND newer.revision>r.revision "
                    "AND newer.sequence<=?);";
    sqlite_statement coverage_statement;
    result = vqec_vision_ai_stor_mdsql_prepare(
        database_, coverage_sql.str(), coverage_statement);
    auto* coverage = coverage_statement.vqec_vision_ai_stor_mdsql_get();
    if (result.code_ != status_code::ok ||
        sqlite3_bind_int(coverage, 1,
            static_cast<int>(metadata_record_family::coverage_health_interval)) != SQLITE_OK ||
        !vqec_vision_ai_stor_mdsql_bind_text(coverage, 2, g_metadata_coverage_complete) ||
        sqlite3_bind_int64(coverage, 3, _request.end_ns_) != SQLITE_OK ||
        sqlite3_bind_int64(coverage, 4, _request.begin_ns_) != SQLITE_OK ||
        sqlite3_bind_int64(coverage, 5, static_cast<sqlite3_int64>(snapshot_sequence)) != SQLITE_OK) {
        return result.code_ == status_code::ok
                   ? vqec_vision_ai_stor_mdsql_make_error(database_, "bind metadata coverage")
                   : result;
    }
    int coverage_bind_index = 6;
    for (const auto& source : _request.source_ids_) {
        if (!vqec_vision_ai_stor_mdsql_bind_text(coverage, coverage_bind_index++, source)) {
            return vqec_vision_ai_stor_mdsql_make_error(database_, "bind metadata coverage source");
        }
    }
    if (sqlite3_bind_int64(coverage, coverage_bind_index,
            static_cast<sqlite3_int64>(snapshot_sequence)) != SQLITE_OK) {
        return vqec_vision_ai_stor_mdsql_make_error(database_, "bind metadata coverage snapshot");
    }
    const auto coverage_step = sqlite3_step(coverage);
    const auto covered_sources = coverage_step == SQLITE_ROW
                                     ? static_cast<std::size_t>(sqlite3_column_int64(coverage, 0))
                                     : 0U;
    _page.coverage_state_ = covered_sources == _request.source_ids_.size()
                                ? g_metadata_coverage_complete
                                : g_metadata_coverage_missing;
    _page.completeness_ = _page.coverage_state_ == g_metadata_coverage_complete
                              ? metadata_query_completeness::complete
                              : metadata_query_completeness::partial;
    return {};
}

status sqlite_metadata_store::vqec_vision_ai_stor_mdsql_get_outbox_size(
    const std::string& _sink_id, std::uint64_t& _count) const {
    _count = 0U;
    if (database_ == nullptr || _sink_id.empty() ||
        _sink_id.size() > g_metadata_query_max_identifier_bytes) {
        return {status_code::invalid_argument, "metadata outbox query is invalid"};
    }
    sqlite_statement statement;
    auto result = vqec_vision_ai_stor_mdsql_prepare(database_,
        "SELECT COUNT(*) FROM metadata_outbox WHERE sink_id=?1 AND state=?2;", statement);
    auto* query = statement.vqec_vision_ai_stor_mdsql_get();
    if (result.code_ != status_code::ok ||
        !vqec_vision_ai_stor_mdsql_bind_text(query, 1, _sink_id) ||
        !vqec_vision_ai_stor_mdsql_bind_text(query, 2, g_metadata_outbox_pending)) {
        return result.code_ == status_code::ok
                   ? vqec_vision_ai_stor_mdsql_make_error(database_, "bind metadata outbox query")
                   : result;
    }
    if (sqlite3_step(query) != SQLITE_ROW) {
        return vqec_vision_ai_stor_mdsql_make_error(database_, "execute metadata outbox query");
    }
    _count = static_cast<std::uint64_t>(sqlite3_column_int64(query, 0));
    return {};
}

}  // namespace vqec::vision::ai
