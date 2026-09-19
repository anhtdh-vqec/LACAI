#include "vqec_vision_sqlite_evidence_outbox.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <mutex>
#include <new>
#include <string>
#include <utility>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

constexpr std::uint64_t g_evidence_minimum_database_bytes = 1024U * 1024U;
constexpr std::uint64_t g_evidence_maximum_database_bytes = 256U * 1024U * 1024U;
constexpr int g_evidence_maximum_busy_timeout_ms = 60000;
constexpr char g_evidence_state_pending[] = "pending";
constexpr char g_evidence_state_claimed[] = "claimed";
constexpr char g_evidence_state_complete[] = "complete";

constexpr char g_evidence_outbox_schema[] = R"sql(
CREATE TABLE IF NOT EXISTS evidence_outbox(
  request_id TEXT PRIMARY KEY,
  event_id TEXT NOT NULL,
  event_revision INTEGER NOT NULL,
  policy_revision INTEGER NOT NULL,
  command_wire BLOB NOT NULL,
  state TEXT NOT NULL,
  attempt_count INTEGER NOT NULL DEFAULT 0,
  next_attempt_monotonic_ns INTEGER NOT NULL DEFAULT 0,
  last_reason TEXT NOT NULL DEFAULT '',
  receipt_state INTEGER NOT NULL DEFAULT 0,
  media_id TEXT NOT NULL DEFAULT '',
  actual_begin_ns INTEGER NOT NULL DEFAULT 0,
  actual_end_ns INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS evidence_outbox_due_idx
  ON evidence_outbox(state,next_attempt_monotonic_ns);
)sql";

class evidence_sqlite_statement final {
public:
    evidence_sqlite_statement() = default;
    ~evidence_sqlite_statement() noexcept {
        if (statement_ != nullptr) {
            sqlite3_finalize(statement_);
        }
    }
    evidence_sqlite_statement(const evidence_sqlite_statement&) = delete;
    evidence_sqlite_statement& operator=(const evidence_sqlite_statement&) = delete;

    sqlite3_stmt** vqec_vision_ai_stor_evobx_out() noexcept {
        return &statement_;
    }
    sqlite3_stmt* vqec_vision_ai_stor_evobx_get() const noexcept {
        return statement_;
    }

private:
    sqlite3_stmt* statement_{nullptr};
};

status vqec_vision_ai_stor_evobx_error(sqlite3* _database, const char* _operation) {
    const int code = _database == nullptr ? SQLITE_ERROR : sqlite3_errcode(_database);
    const std::string detail = _database == nullptr ?
        "database unavailable" : sqlite3_errmsg(_database);
    return {code == SQLITE_FULL ? status_code::resource_exhausted : status_code::io_error,
        std::string(_operation) + ": " + detail};
}

status vqec_vision_ai_stor_evobx_execute(sqlite3* _database, const char* _sql) {
    if (sqlite3_exec(_database, _sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
        return vqec_vision_ai_stor_evobx_error(_database, "execute evidence outbox SQL");
    }
    return {};
}

status vqec_vision_ai_stor_evobx_prepare(sqlite3* _database, const char* _sql,
    evidence_sqlite_statement& _statement) {
    if (sqlite3_prepare_v2(_database, _sql, -1,
            _statement.vqec_vision_ai_stor_evobx_out(), nullptr) != SQLITE_OK) {
        return vqec_vision_ai_stor_evobx_error(
            _database, "prepare evidence outbox SQL");
    }
    return {};
}

bool vqec_vision_ai_stor_evobx_bind_text(
    sqlite3_stmt* _statement, int _index, const std::string& _value) noexcept {
    return _value.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()) &&
        sqlite3_bind_text(_statement, _index, _value.data(),
            static_cast<int>(_value.size()), SQLITE_TRANSIENT) == SQLITE_OK;
}

bool vqec_vision_ai_stor_evobx_bind_u64(
    sqlite3_stmt* _statement, int _index, std::uint64_t _value) noexcept {
    return _value <= static_cast<std::uint64_t>(std::numeric_limits<sqlite3_int64>::max()) &&
        sqlite3_bind_int64(_statement, _index,
            static_cast<sqlite3_int64>(_value)) == SQLITE_OK;
}

bool vqec_vision_ai_stor_evobx_bind_blob(sqlite3_stmt* _statement, int _index,
    const std::vector<std::uint8_t>& _value) noexcept {
    return _value.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()) &&
        sqlite3_bind_blob(_statement, _index, _value.data(),
            static_cast<int>(_value.size()), SQLITE_TRANSIENT) == SQLITE_OK;
}

std::vector<std::uint8_t> vqec_vision_ai_stor_evobx_read_blob(
    sqlite3_stmt* _statement, int _column) {
    const auto* bytes = static_cast<const std::uint8_t*>(
        sqlite3_column_blob(_statement, _column));
    const int count = sqlite3_column_bytes(_statement, _column);
    return bytes == nullptr || count <= 0 ? std::vector<std::uint8_t>{} :
        std::vector<std::uint8_t>(bytes, bytes + count);
}

status vqec_vision_ai_stor_evobx_validate_path(const std::string& _path) {
    const std::filesystem::path path(_path);
    const auto parent = path.parent_path();
    struct stat path_status {};
    if (parent.empty() || lstat(parent.c_str(), &path_status) != 0 ||
        !S_ISDIR(path_status.st_mode) || path_status.st_uid != getuid() ||
        (path_status.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
        return {status_code::unauthorized,
            "evidence outbox parent must be an owner-only directory"};
    }
    if (lstat(path.c_str(), &path_status) == 0 &&
        (!S_ISREG(path_status.st_mode) || path_status.st_uid != getuid() ||
         (path_status.st_mode & (S_IRWXG | S_IRWXO)) != 0)) {
        return {status_code::unauthorized,
            "evidence outbox must be an owner-only regular file"};
    }
    return {};
}

bool vqec_vision_ai_stor_evobx_is_terminal(evidence_receipt_state _state) noexcept {
    return _state == evidence_receipt_state::ready ||
        _state == evidence_receipt_state::partial ||
        _state == evidence_receipt_state::failed ||
        _state == evidence_receipt_state::rejected ||
        _state == evidence_receipt_state::expired;
}

}  // namespace

sqlite_evidence_outbox::sqlite_evidence_outbox(sqlite_evidence_outbox_config _config)
    : config_(std::move(_config)) {}

sqlite_evidence_outbox::~sqlite_evidence_outbox() noexcept {
    if (database_ != nullptr) {
        sqlite3_close(database_);
    }
}

status sqlite_evidence_outbox::vqec_vision_ai_ports_evobx_open() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (database_ != nullptr) {
        return {};
    }
    if (config_.database_path_.empty() ||
        !std::filesystem::path(config_.database_path_).is_absolute() ||
        config_.maximum_database_bytes_ < g_evidence_minimum_database_bytes ||
        config_.maximum_database_bytes_ > g_evidence_maximum_database_bytes ||
        config_.busy_timeout_ms_ < 0 ||
        config_.busy_timeout_ms_ > g_evidence_maximum_busy_timeout_ms) {
        return {status_code::invalid_argument,
            "invalid evidence outbox configuration"};
    }
    auto result = vqec_vision_ai_stor_evobx_validate_path(config_.database_path_);
    if (result.code_ != status_code::ok) {
        return result;
    }
    sqlite3* candidate = nullptr;
    if (sqlite3_open_v2(config_.database_path_.c_str(), &candidate,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
            nullptr) != SQLITE_OK || candidate == nullptr) {
        if (candidate != nullptr) {
            sqlite3_close(candidate);
        }
        return {status_code::io_error, "cannot open evidence outbox"};
    }
    database_ = candidate;
    if (chmod(config_.database_path_.c_str(), S_IRUSR | S_IWUSR) != 0) {
        sqlite3_close(database_);
        database_ = nullptr;
        return {status_code::io_error, "cannot protect evidence outbox"};
    }
    sqlite3_busy_timeout(database_, config_.busy_timeout_ms_);
    result = vqec_vision_ai_stor_evobx_execute(database_,
        "PRAGMA journal_mode=WAL;PRAGMA synchronous=FULL;PRAGMA foreign_keys=ON;");
    if (result.code_ == status_code::ok) {
        result = vqec_vision_ai_stor_evobx_execute(database_, g_evidence_outbox_schema);
    }
    evidence_sqlite_statement page_size;
    if (result.code_ == status_code::ok) {
        result = vqec_vision_ai_stor_evobx_prepare(
            database_, "PRAGMA page_size", page_size);
    }
    if (result.code_ != status_code::ok ||
        sqlite3_step(page_size.vqec_vision_ai_stor_evobx_get()) != SQLITE_ROW) {
        sqlite3_close(database_);
        database_ = nullptr;
        return result.code_ == status_code::ok ?
            status{status_code::io_error, "cannot read evidence outbox page size"} : result;
    }
    const auto page_bytes = sqlite3_column_int64(
        page_size.vqec_vision_ai_stor_evobx_get(), 0);
    if (page_bytes <= 0) {
        sqlite3_close(database_);
        database_ = nullptr;
        return {status_code::invalid_state, "invalid evidence outbox page size"};
    }
    const auto maximum_pages = config_.maximum_database_bytes_ /
        static_cast<std::uint64_t>(page_bytes);
    result = vqec_vision_ai_stor_evobx_execute(database_,
        ("PRAGMA max_page_count=" + std::to_string(maximum_pages)).c_str());
    if (result.code_ == status_code::ok) {
        result = vqec_vision_ai_stor_evobx_execute(database_, "BEGIN IMMEDIATE");
    }
    if (result.code_ == status_code::ok) {
        result = vqec_vision_ai_stor_evobx_execute(database_,
            "UPDATE evidence_outbox SET state='pending' WHERE state='claimed'");
        stats_.recovered_claims_ = static_cast<std::uint64_t>(sqlite3_changes(database_));
    }
    if (result.code_ == status_code::ok) {
        result = vqec_vision_ai_stor_evobx_execute(database_, "COMMIT");
    } else if (database_ != nullptr) {
        (void)vqec_vision_ai_stor_evobx_execute(database_, "ROLLBACK");
    }
    if (result.code_ != status_code::ok) {
        sqlite3_close(database_);
        database_ = nullptr;
        return result;
    }
    return {};
}

status sqlite_evidence_outbox::vqec_vision_ai_ports_evobx_enqueue(
    const evidence_command& _command) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (database_ == nullptr) {
        return {status_code::invalid_state, "evidence outbox is not open"};
    }
    std::vector<std::uint8_t> wire;
    auto result = vqec_vision_ai_core_evtrn_encode_command(_command, wire);
    if (result.code_ != status_code::ok) {
        return result;
    }
    result = vqec_vision_ai_stor_evobx_execute(database_, "BEGIN IMMEDIATE");
    if (result.code_ != status_code::ok) {
        return result;
    }
    evidence_sqlite_statement existing;
    result = vqec_vision_ai_stor_evobx_prepare(database_,
        "SELECT command_wire FROM evidence_outbox WHERE request_id=?", existing);
    auto* query = existing.vqec_vision_ai_stor_evobx_get();
    if (result.code_ != status_code::ok ||
        !vqec_vision_ai_stor_evobx_bind_text(query, 1, _command.request_id_)) {
        (void)vqec_vision_ai_stor_evobx_execute(database_, "ROLLBACK");
        return result.code_ == status_code::ok ?
            status{status_code::io_error, "cannot bind evidence dedup query"} : result;
    }
    const int step = sqlite3_step(query);
    if (step == SQLITE_ROW) {
        const auto stored = vqec_vision_ai_stor_evobx_read_blob(query, 0);
        (void)vqec_vision_ai_stor_evobx_execute(database_, "ROLLBACK");
        if (stored != wire) {
            return {status_code::invalid_argument,
                "evidence request ID conflicts with committed payload"};
        }
        ++stats_.idempotent_commands_;
        return {};
    }
    if (step != SQLITE_DONE) {
        (void)vqec_vision_ai_stor_evobx_execute(database_, "ROLLBACK");
        return vqec_vision_ai_stor_evobx_error(database_, "query evidence dedup state");
    }
    evidence_sqlite_statement insert;
    result = vqec_vision_ai_stor_evobx_prepare(database_,
        "INSERT INTO evidence_outbox(request_id,event_id,event_revision,policy_revision,"
        "command_wire,state) VALUES(?,?,?,?,?,?)", insert);
    auto* statement = insert.vqec_vision_ai_stor_evobx_get();
    const bool bound = result.code_ == status_code::ok &&
        vqec_vision_ai_stor_evobx_bind_text(statement, 1, _command.request_id_) &&
        vqec_vision_ai_stor_evobx_bind_text(statement, 2, _command.event_id_) &&
        vqec_vision_ai_stor_evobx_bind_u64(statement, 3, _command.event_revision_) &&
        vqec_vision_ai_stor_evobx_bind_u64(statement, 4, _command.policy_revision_) &&
        vqec_vision_ai_stor_evobx_bind_blob(statement, 5, wire) &&
        vqec_vision_ai_stor_evobx_bind_text(statement, 6, g_evidence_state_pending);
    if (!bound || sqlite3_step(statement) != SQLITE_DONE) {
        (void)vqec_vision_ai_stor_evobx_execute(database_, "ROLLBACK");
        return !bound ? status{status_code::resource_exhausted,
            "cannot bind evidence outbox insert"} :
            vqec_vision_ai_stor_evobx_error(database_, "insert evidence command");
    }
    result = vqec_vision_ai_stor_evobx_execute(database_, "COMMIT");
    if (result.code_ == status_code::ok) {
        ++stats_.committed_commands_;
        ++stats_.pending_commands_;
    }
    return result;
}

status sqlite_evidence_outbox::vqec_vision_ai_ports_evobx_claim_due(
    std::uint64_t _now_monotonic_ns, evidence_outbox_record& _record) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (database_ == nullptr) {
        return {status_code::invalid_state, "evidence outbox is not open"};
    }
    auto result = vqec_vision_ai_stor_evobx_execute(database_, "BEGIN IMMEDIATE");
    if (result.code_ != status_code::ok) {
        return result;
    }
    evidence_sqlite_statement select;
    result = vqec_vision_ai_stor_evobx_prepare(database_,
        "SELECT request_id,command_wire,attempt_count FROM evidence_outbox "
        "WHERE state=? AND next_attempt_monotonic_ns<=? ORDER BY rowid LIMIT 1", select);
    auto* query = select.vqec_vision_ai_stor_evobx_get();
    if (result.code_ != status_code::ok ||
        !vqec_vision_ai_stor_evobx_bind_text(query, 1, g_evidence_state_pending) ||
        !vqec_vision_ai_stor_evobx_bind_u64(query, 2, _now_monotonic_ns)) {
        (void)vqec_vision_ai_stor_evobx_execute(database_, "ROLLBACK");
        return result.code_ == status_code::ok ?
            status{status_code::resource_exhausted, "cannot bind evidence claim"} : result;
    }
    const int step = sqlite3_step(query);
    if (step == SQLITE_DONE) {
        (void)vqec_vision_ai_stor_evobx_execute(database_, "ROLLBACK");
        return {status_code::pending, "no evidence command is due"};
    }
    if (step != SQLITE_ROW) {
        (void)vqec_vision_ai_stor_evobx_execute(database_, "ROLLBACK");
        return vqec_vision_ai_stor_evobx_error(database_, "select due evidence command");
    }
    const auto* request_text = sqlite3_column_text(query, 0);
    const auto attempt = sqlite3_column_int64(query, 2);
    const auto wire = vqec_vision_ai_stor_evobx_read_blob(query, 1);
    if (request_text == nullptr || attempt < 0 ||
        attempt >= static_cast<sqlite3_int64>(evidence_transport_limits::g_max_attempts)) {
        (void)vqec_vision_ai_stor_evobx_execute(database_, "ROLLBACK");
        return {status_code::invalid_state, "corrupt or exhausted evidence command"};
    }
    const std::string request_id(reinterpret_cast<const char*>(request_text));
    evidence_sqlite_statement update;
    result = vqec_vision_ai_stor_evobx_prepare(database_,
        "UPDATE evidence_outbox SET state=?,attempt_count=attempt_count+1 "
        "WHERE request_id=? AND state=?", update);
    auto* claim = update.vqec_vision_ai_stor_evobx_get();
    if (result.code_ != status_code::ok ||
        !vqec_vision_ai_stor_evobx_bind_text(claim, 1, g_evidence_state_claimed) ||
        !vqec_vision_ai_stor_evobx_bind_text(claim, 2, request_id) ||
        !vqec_vision_ai_stor_evobx_bind_text(claim, 3, g_evidence_state_pending) ||
        sqlite3_step(claim) != SQLITE_DONE || sqlite3_changes(database_) != 1) {
        (void)vqec_vision_ai_stor_evobx_execute(database_, "ROLLBACK");
        return vqec_vision_ai_stor_evobx_error(database_, "claim evidence command");
    }
    evidence_command command;
    result = vqec_vision_ai_core_evtrn_decode_command(wire.data(), wire.size(), command);
    if (result.code_ != status_code::ok) {
        (void)vqec_vision_ai_stor_evobx_execute(database_, "ROLLBACK");
        return {status_code::invalid_state, "corrupt evidence command payload"};
    }
    result = vqec_vision_ai_stor_evobx_execute(database_, "COMMIT");
    if (result.code_ != status_code::ok) {
        return result;
    }
    evidence_outbox_record record;
    record.command_ = std::move(command);
    record.attempt_count_ = static_cast<std::size_t>(attempt) + 1U;
    record.next_attempt_monotonic_ns_ = _now_monotonic_ns;
    _record = std::move(record);
    return {};
}

status sqlite_evidence_outbox::vqec_vision_ai_ports_evobx_retry(
    const std::string& _request_id, std::size_t _attempt_count,
    std::uint64_t _next_attempt_monotonic_ns, const std::string& _reason) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (database_ == nullptr) {
        return {status_code::invalid_state, "evidence outbox is not open"};
    }
    if (!vqec_vision_ai_cntr_ident_is_valid(
            _request_id, feature_event_limits::g_max_identifier_bytes) ||
        _attempt_count == 0U ||
        _attempt_count > evidence_transport_limits::g_max_attempts ||
        _reason.size() > evidence_transport_limits::g_max_reason_bytes) {
        return {status_code::invalid_argument, "invalid evidence retry"};
    }
    evidence_sqlite_statement update;
    auto result = vqec_vision_ai_stor_evobx_prepare(database_,
        "UPDATE evidence_outbox SET state=?,next_attempt_monotonic_ns=?,last_reason=? "
        "WHERE request_id=? AND state=? AND attempt_count=?", update);
    auto* statement = update.vqec_vision_ai_stor_evobx_get();
    const bool bound = result.code_ == status_code::ok &&
        vqec_vision_ai_stor_evobx_bind_text(statement, 1, g_evidence_state_pending) &&
        vqec_vision_ai_stor_evobx_bind_u64(statement, 2, _next_attempt_monotonic_ns) &&
        vqec_vision_ai_stor_evobx_bind_text(statement, 3, _reason) &&
        vqec_vision_ai_stor_evobx_bind_text(statement, 4, _request_id) &&
        vqec_vision_ai_stor_evobx_bind_text(statement, 5, g_evidence_state_claimed) &&
        _attempt_count <= static_cast<std::size_t>(std::numeric_limits<int>::max()) &&
        sqlite3_bind_int(statement, 6, static_cast<int>(_attempt_count)) == SQLITE_OK;
    if (!bound || sqlite3_step(statement) != SQLITE_DONE ||
        sqlite3_changes(database_) != 1) {
        return !bound ? status{status_code::invalid_argument,
            "cannot bind evidence retry"} :
            status{status_code::invalid_state, "evidence retry does not own claim"};
    }
    ++stats_.retry_schedules_;
    return {};
}

status sqlite_evidence_outbox::vqec_vision_ai_ports_evobx_complete(
    const evidence_receipt& _receipt) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (database_ == nullptr) {
        return {status_code::invalid_state, "evidence outbox is not open"};
    }
    if (!vqec_vision_ai_cntr_ident_is_valid(
            _receipt.request_id_, feature_event_limits::g_max_identifier_bytes) ||
        _receipt.event_revision_ == 0U ||
        !vqec_vision_ai_stor_evobx_is_terminal(_receipt.state_) ||
        _receipt.media_id_.size() > evidence_transport_limits::g_max_media_id_bytes ||
        _receipt.reason_.size() > evidence_transport_limits::g_max_reason_bytes ||
        (_receipt.actual_end_ns_ != 0U &&
         _receipt.actual_end_ns_ < _receipt.actual_begin_ns_)) {
        return {status_code::invalid_argument, "invalid terminal evidence receipt"};
    }
    evidence_sqlite_statement update;
    auto result = vqec_vision_ai_stor_evobx_prepare(database_,
        "UPDATE evidence_outbox SET state=?,receipt_state=?,media_id=?,"
        "actual_begin_ns=?,actual_end_ns=?,last_reason=? WHERE request_id=? "
        "AND event_revision=? AND state=?", update);
    auto* statement = update.vqec_vision_ai_stor_evobx_get();
    const bool bound = result.code_ == status_code::ok &&
        vqec_vision_ai_stor_evobx_bind_text(statement, 1, g_evidence_state_complete) &&
        sqlite3_bind_int(statement, 2, static_cast<int>(_receipt.state_)) == SQLITE_OK &&
        vqec_vision_ai_stor_evobx_bind_text(statement, 3, _receipt.media_id_) &&
        vqec_vision_ai_stor_evobx_bind_u64(statement, 4, _receipt.actual_begin_ns_) &&
        vqec_vision_ai_stor_evobx_bind_u64(statement, 5, _receipt.actual_end_ns_) &&
        vqec_vision_ai_stor_evobx_bind_text(statement, 6, _receipt.reason_) &&
        vqec_vision_ai_stor_evobx_bind_text(statement, 7, _receipt.request_id_) &&
        vqec_vision_ai_stor_evobx_bind_u64(statement, 8, _receipt.event_revision_) &&
        vqec_vision_ai_stor_evobx_bind_text(statement, 9, g_evidence_state_claimed);
    if (!bound || sqlite3_step(statement) != SQLITE_DONE ||
        sqlite3_changes(database_) != 1) {
        return !bound ? status{status_code::resource_exhausted,
            "cannot bind evidence completion"} :
            status{status_code::invalid_state, "evidence receipt does not match claim"};
    }
    ++stats_.completed_commands_;
    if (stats_.pending_commands_ > 0U) {
        --stats_.pending_commands_;
    }
    return {};
}

status sqlite_evidence_outbox::vqec_vision_ai_ports_evobx_get_stats(
    evidence_outbox_stats& _stats) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (database_ == nullptr) {
        return {status_code::invalid_state, "evidence outbox is not open"};
    }
    evidence_sqlite_statement query;
    auto result = vqec_vision_ai_stor_evobx_prepare(database_,
        "SELECT COUNT(*) FROM evidence_outbox WHERE state<>?", query);
    auto* statement = query.vqec_vision_ai_stor_evobx_get();
    if (result.code_ != status_code::ok ||
        !vqec_vision_ai_stor_evobx_bind_text(statement, 1, g_evidence_state_complete) ||
        sqlite3_step(statement) != SQLITE_ROW) {
        return result.code_ == status_code::ok ?
            vqec_vision_ai_stor_evobx_error(database_, "count evidence commands") : result;
    }
    const auto count = sqlite3_column_int64(statement, 0);
    if (count < 0) {
        return {status_code::invalid_state, "invalid evidence outbox count"};
    }
    auto stats = stats_;
    stats.pending_commands_ = static_cast<std::size_t>(count);
    _stats = stats;
    return {};
}

}  // namespace vqec::vision::ai
