#include "vqec_vision_sqlite_app_inventory.hpp"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <sys/stat.h>
#include <unistd.h>

#include <sqlite3.h>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

constexpr std::uint64_t g_min_database_bytes = 1024U * 1024U;
constexpr std::uint64_t g_max_database_bytes = 256U * 1024U * 1024U;
constexpr int g_max_busy_timeout_ms = 60000;
constexpr char g_schema_sql[] = R"sql(
CREATE TABLE IF NOT EXISTS app_meta(
  singleton INTEGER PRIMARY KEY CHECK(singleton=1),
  snapshot_revision INTEGER NOT NULL,
  inventory_revision INTEGER NOT NULL,
  entitlement_revision INTEGER NOT NULL,
  desired_revision INTEGER NOT NULL
);
INSERT OR IGNORE INTO app_meta VALUES(1,1,1,1,1);
CREATE TABLE IF NOT EXISTS applications(
  app_id TEXT PRIMARY KEY,
  app_version TEXT NOT NULL,
  usecase_version TEXT NOT NULL,
  release_sequence INTEGER NOT NULL,
  manifest_sha256 TEXT NOT NULL,
  configuration_schema_id TEXT NOT NULL,
  configuration_revision INTEGER NOT NULL,
  configuration_sha256 TEXT NOT NULL,
  configuration_payload BLOB NOT NULL,
  requested_output_scopes TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS app_sources(
  app_id TEXT NOT NULL REFERENCES applications(app_id) ON DELETE CASCADE,
  source_id TEXT NOT NULL,
  entitled INTEGER NOT NULL DEFAULT 0,
  desired INTEGER NOT NULL DEFAULT 0,
  supported INTEGER NOT NULL DEFAULT 0,
  compatible INTEGER NOT NULL DEFAULT 0,
  admitted INTEGER NOT NULL DEFAULT 0,
  entitlement_expires_utc_ns INTEGER NOT NULL DEFAULT 0,
  output_scopes TEXT NOT NULL DEFAULT '',
  reason_code TEXT NOT NULL DEFAULT 'not_entitled',
  PRIMARY KEY(app_id,source_id)
);
CREATE TABLE IF NOT EXISTS components(
  component_id TEXT NOT NULL,
  component_version TEXT NOT NULL,
  target_id TEXT NOT NULL,
  artifact_sha256 TEXT NOT NULL,
  semantic_contract_sha256 TEXT NOT NULL,
  reference_count INTEGER NOT NULL CHECK(reference_count>=0),
  PRIMARY KEY(component_id,component_version,target_id,artifact_sha256,semantic_contract_sha256)
);
CREATE TABLE IF NOT EXISTS app_components(
  app_id TEXT NOT NULL REFERENCES applications(app_id) ON DELETE CASCADE,
  component_id TEXT NOT NULL,
  component_version TEXT NOT NULL,
  target_id TEXT NOT NULL,
  artifact_sha256 TEXT NOT NULL,
  semantic_contract_sha256 TEXT NOT NULL,
  PRIMARY KEY(app_id,component_id,component_version,target_id)
);
)sql";

class sqlite_statement final {
public:
    sqlite_statement() = default;
    ~sqlite_statement() {
        if (statement_ != nullptr) {
            sqlite3_finalize(statement_);
        }
    }
    sqlite_statement(const sqlite_statement&) = delete;
    sqlite_statement& operator=(const sqlite_statement&) = delete;

    sqlite3_stmt** vqec_vision_ai_stor_apinv_out() noexcept {
        return &statement_;
    }
    sqlite3_stmt* vqec_vision_ai_stor_apinv_get() const noexcept {
        return statement_;
    }

private:
    sqlite3_stmt* statement_{nullptr};
};

status vqec_vision_ai_stor_apinv_exec(sqlite3* _database, const char* _sql) {
    char* error = nullptr;
    const int result = sqlite3_exec(_database, _sql, nullptr, nullptr, &error);
    std::string message;
    if (error != nullptr) {
        message = error;
        sqlite3_free(error);
    }
    if (result == SQLITE_OK) {
        return {};
    }
    return {result == SQLITE_FULL ? status_code::resource_exhausted : status_code::io_error,
        message.empty() ? "app inventory SQLite operation failed" : message};
}

status vqec_vision_ai_stor_apinv_prepare(
    sqlite3* _database, const char* _sql, sqlite_statement& _statement) {
    if (sqlite3_prepare_v2(_database, _sql, -1,
            _statement.vqec_vision_ai_stor_apinv_out(), nullptr) != SQLITE_OK) {
        return {status_code::io_error, sqlite3_errmsg(_database)};
    }
    return {};
}

status vqec_vision_ai_stor_apinv_step_done(
    sqlite3* _database, sqlite3_stmt* _statement) {
    const int result = sqlite3_step(_statement);
    if (result == SQLITE_DONE) {
        return {};
    }
    return {result == SQLITE_FULL ? status_code::resource_exhausted : status_code::io_error,
        sqlite3_errmsg(_database)};
}

bool vqec_vision_ai_stor_apinv_bind_text(
    sqlite3_stmt* _statement, int _index, const std::string& _value) noexcept {
    return sqlite3_bind_text(_statement, _index, _value.data(),
               static_cast<int>(_value.size()), SQLITE_TRANSIENT) == SQLITE_OK;
}

bool vqec_vision_ai_stor_apinv_bind_uint64(
    sqlite3_stmt* _statement, int _index, std::uint64_t _value) noexcept {
    return _value <= static_cast<std::uint64_t>(std::numeric_limits<sqlite3_int64>::max()) &&
        sqlite3_bind_int64(_statement, _index,
            static_cast<sqlite3_int64>(_value)) == SQLITE_OK;
}

bool vqec_vision_ai_stor_apinv_bind_blob(sqlite3_stmt* _statement, int _index,
    const std::vector<std::uint8_t>& _value) noexcept {
    return _value.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()) &&
        sqlite3_bind_blob(_statement, _index, _value.data(),
            static_cast<int>(_value.size()), SQLITE_TRANSIENT) == SQLITE_OK;
}

std::string vqec_vision_ai_stor_apinv_join_scopes(
    const std::vector<std::string>& _scopes) {
    std::string value;
    for (std::size_t index = 0; index < _scopes.size(); ++index) {
        if (index != 0) {
            value.push_back('\n');
        }
        value += _scopes[index];
    }
    return value;
}

status vqec_vision_ai_stor_apinv_split_scopes(
    const unsigned char* _text, std::vector<std::string>& _scopes) {
    std::vector<std::string> candidate;
    if (_text == nullptr || *_text == '\0') {
        _scopes = std::move(candidate);
        return {};
    }
    const std::string value(reinterpret_cast<const char*>(_text));
    std::size_t begin = 0;
    while (begin <= value.size()) {
        const auto end = value.find('\n', begin);
        const auto length = end == std::string::npos ? value.size() - begin : end - begin;
        auto scope = value.substr(begin, length);
        if (!vqec_vision_ai_cntr_ident_is_valid(
                scope, app_lifecycle_limits::g_max_identifier_bytes) ||
            std::find(candidate.begin(), candidate.end(), scope) != candidate.end() ||
            candidate.size() == app_lifecycle_limits::g_max_scopes) {
            return {status_code::invalid_state, "corrupt app inventory scope list"};
        }
        candidate.push_back(std::move(scope));
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1U;
    }
    _scopes = std::move(candidate);
    return {};
}

status vqec_vision_ai_stor_apinv_read_revisions(sqlite3* _database,
    std::uint64_t& _snapshot, std::uint64_t& _inventory,
    std::uint64_t& _entitlement, std::uint64_t& _desired) {
    sqlite_statement statement;
    const auto prepared = vqec_vision_ai_stor_apinv_prepare(_database,
        "SELECT snapshot_revision,inventory_revision,entitlement_revision,desired_revision "
        "FROM app_meta WHERE singleton=1", statement);
    if (prepared.code_ != status_code::ok) {
        return prepared;
    }
    if (sqlite3_step(statement.vqec_vision_ai_stor_apinv_get()) != SQLITE_ROW) {
        return {status_code::invalid_state, "app inventory revision row is missing"};
    }
    const auto read = [&](int _column, std::uint64_t& _value) -> bool {
        const auto value = sqlite3_column_int64(
            statement.vqec_vision_ai_stor_apinv_get(), _column);
        if (value <= 0) {
            return false;
        }
        _value = static_cast<std::uint64_t>(value);
        return true;
    };
    if (!read(0, _snapshot) || !read(1, _inventory) ||
        !read(2, _entitlement) || !read(3, _desired)) {
        return {status_code::invalid_state, "invalid app inventory revisions"};
    }
    return {};
}

status vqec_vision_ai_stor_apinv_update_revisions(sqlite3* _database,
    std::uint64_t _snapshot, std::uint64_t _inventory,
    std::uint64_t _entitlement, std::uint64_t _desired) {
    sqlite_statement statement;
    const auto prepared = vqec_vision_ai_stor_apinv_prepare(_database,
        "UPDATE app_meta SET snapshot_revision=?,inventory_revision=?,"
        "entitlement_revision=?,desired_revision=? WHERE singleton=1", statement);
    if (prepared.code_ != status_code::ok) {
        return prepared;
    }
    auto* handle = statement.vqec_vision_ai_stor_apinv_get();
    if (!vqec_vision_ai_stor_apinv_bind_uint64(handle, 1, _snapshot) ||
        !vqec_vision_ai_stor_apinv_bind_uint64(handle, 2, _inventory) ||
        !vqec_vision_ai_stor_apinv_bind_uint64(handle, 3, _entitlement) ||
        !vqec_vision_ai_stor_apinv_bind_uint64(handle, 4, _desired)) {
        return {status_code::resource_exhausted, "app inventory revision overflow"};
    }
    return vqec_vision_ai_stor_apinv_step_done(_database, handle);
}

status vqec_vision_ai_stor_apinv_begin(sqlite3* _database) {
    return vqec_vision_ai_stor_apinv_exec(_database, "BEGIN IMMEDIATE");
}

void vqec_vision_ai_stor_apinv_rollback(sqlite3* _database) noexcept {
    (void)sqlite3_exec(_database, "ROLLBACK", nullptr, nullptr, nullptr);
}

status vqec_vision_ai_stor_apinv_commit(sqlite3* _database) {
    return vqec_vision_ai_stor_apinv_exec(_database, "COMMIT");
}

status vqec_vision_ai_stor_apinv_load(
    sqlite3* _database, runtime_control_snapshot& _snapshot) {
    runtime_control_snapshot candidate;
    candidate.schema_version_ = app_lifecycle_limits::g_schema_version;
    const auto revisions = vqec_vision_ai_stor_apinv_read_revisions(_database,
        candidate.snapshot_revision_, candidate.inventory_revision_,
        candidate.entitlement_revision_, candidate.desired_revision_);
    if (revisions.code_ != status_code::ok) {
        return revisions;
    }
    sqlite_statement statement;
    const auto prepared = vqec_vision_ai_stor_apinv_prepare(_database,
        "SELECT s.app_id,s.source_id,s.entitled,s.desired,s.supported,s.compatible,"
        "s.admitted,a.configuration_revision,a.configuration_sha256,"
        "a.configuration_schema_id,a.configuration_payload,s.output_scopes,"
        "s.reason_code,s.entitlement_expires_utc_ns "
        "FROM app_sources s JOIN applications a ON a.app_id=s.app_id "
        "ORDER BY s.app_id,s.source_id", statement);
    if (prepared.code_ != status_code::ok) {
        return prepared;
    }
    auto* handle = statement.vqec_vision_ai_stor_apinv_get();
    while (true) {
        const int result = sqlite3_step(handle);
        if (result == SQLITE_DONE) {
            break;
        }
        if (result != SQLITE_ROW) {
            return {status_code::io_error, sqlite3_errmsg(_database)};
        }
        if (candidate.associations_.size() == app_lifecycle_limits::g_max_associations) {
            return {status_code::resource_exhausted,
                "app inventory association limit reached"};
        }
        app_runtime_association association;
        const auto* app_id = sqlite3_column_text(handle, 0);
        const auto* source_id = sqlite3_column_text(handle, 1);
        const auto* config_sha = sqlite3_column_text(handle, 8);
        const auto* config_schema = sqlite3_column_text(handle, 9);
        const auto* reason = sqlite3_column_text(handle, 12);
        if (app_id == nullptr || source_id == nullptr || config_sha == nullptr ||
            config_schema == nullptr || reason == nullptr) {
            return {status_code::invalid_state, "corrupt app inventory row"};
        }
        association.app_id_ = reinterpret_cast<const char*>(app_id);
        association.source_id_ = reinterpret_cast<const char*>(source_id);
        association.installed_ = true;
        association.entitled_ = sqlite3_column_int(handle, 2) != 0;
        association.desired_ = sqlite3_column_int(handle, 3) != 0;
        association.supported_ = sqlite3_column_int(handle, 4) != 0;
        association.compatible_ = sqlite3_column_int(handle, 5) != 0;
        association.admitted_ = sqlite3_column_int(handle, 6) != 0;
        const auto config_revision = sqlite3_column_int64(handle, 7);
        const auto expiry = sqlite3_column_int64(handle, 13);
        if (config_revision <= 0 || expiry < 0) {
            return {status_code::invalid_state, "corrupt app inventory numeric field"};
        }
        association.configuration_revision_ =
            static_cast<std::uint64_t>(config_revision);
        association.configuration_sha256_ = reinterpret_cast<const char*>(config_sha);
        association.configuration_schema_id_ =
            reinterpret_cast<const char*>(config_schema);
        const auto* payload = static_cast<const std::uint8_t*>(
            sqlite3_column_blob(handle, 10));
        const int payload_bytes = sqlite3_column_bytes(handle, 10);
        if (payload == nullptr || payload_bytes <= 0 ||
            static_cast<std::size_t>(payload_bytes) >
                app_lifecycle_limits::g_max_document_bytes) {
            return {status_code::invalid_state, "corrupt app configuration payload"};
        }
        association.configuration_payload_.assign(payload, payload + payload_bytes);
        const auto split = vqec_vision_ai_stor_apinv_split_scopes(
            sqlite3_column_text(handle, 11), association.output_scopes_);
        if (split.code_ != status_code::ok) {
            return split;
        }
        association.reason_code_ = reinterpret_cast<const char*>(reason);
        association.entitlement_expires_utc_ns_ = static_cast<std::uint64_t>(expiry);
        candidate.associations_.push_back(std::move(association));
    }
    const auto valid = vqec_vision_ai_core_applc_validate_runtime_snapshot(candidate);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    _snapshot = std::move(candidate);
    return {};
}

status vqec_vision_ai_stor_apinv_finish_mutation(
    sqlite3* _database, runtime_control_snapshot& _snapshot) {
    runtime_control_snapshot candidate;
    const auto loaded = vqec_vision_ai_stor_apinv_load(_database, candidate);
    if (loaded.code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(_database);
        return loaded;
    }
    const auto committed = vqec_vision_ai_stor_apinv_commit(_database);
    if (committed.code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(_database);
        return committed;
    }
    _snapshot = std::move(candidate);
    return {};
}

bool vqec_vision_ai_stor_apinv_is_subset(
    const std::vector<std::string>& _granted,
    const std::vector<std::string>& _requested) noexcept {
    for (const auto& scope : _granted) {
        if (std::find(_requested.begin(), _requested.end(), scope) == _requested.end()) {
            return false;
        }
    }
    return true;
}

bool vqec_vision_ai_stor_apinv_are_unique_scopes(
    const std::vector<std::string>& _scopes) noexcept {
    if (_scopes.size() > app_lifecycle_limits::g_max_scopes) {
        return false;
    }
    for (std::size_t index = 0; index < _scopes.size(); ++index) {
        if (!vqec_vision_ai_cntr_ident_is_valid(
                _scopes[index], app_lifecycle_limits::g_max_identifier_bytes)) {
            return false;
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (_scopes[previous] == _scopes[index]) {
                return false;
            }
        }
    }
    return true;
}

status vqec_vision_ai_stor_apinv_validate_storage_path(
    const std::string& _database_path) {
    const std::filesystem::path path(_database_path);
    const auto parent = path.parent_path();
    struct stat parent_status {};
    if (parent.empty() || lstat(parent.c_str(), &parent_status) != 0 ||
        !S_ISDIR(parent_status.st_mode) || parent_status.st_uid != getuid() ||
        (parent_status.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
        return {status_code::unauthorized,
            "app inventory parent directory must be owner-only"};
    }
    struct stat file_status {};
    if (lstat(path.c_str(), &file_status) == 0 &&
        (!S_ISREG(file_status.st_mode) || file_status.st_uid != getuid() ||
         (file_status.st_mode & (S_IRWXG | S_IRWXO)) != 0)) {
        return {status_code::unauthorized,
            "app inventory file is not an owner-only regular file"};
    }
    return {};
}

}  // namespace

sqlite_app_inventory::sqlite_app_inventory(sqlite_app_inventory_config _config)
    : config_(std::move(_config)) {}

sqlite_app_inventory::~sqlite_app_inventory() {
    if (database_ != nullptr) {
        sqlite3_close(database_);
    }
}

status sqlite_app_inventory::vqec_vision_ai_ports_apinv_open() {
    if (database_ != nullptr) {
        return {};
    }
    if (config_.database_path_.empty() ||
        !std::filesystem::path(config_.database_path_).is_absolute() ||
        config_.max_database_bytes_ < g_min_database_bytes ||
        config_.max_database_bytes_ > g_max_database_bytes ||
        config_.busy_timeout_ms_ < 0 || config_.busy_timeout_ms_ > g_max_busy_timeout_ms) {
        return {status_code::invalid_argument, "invalid app inventory configuration"};
    }
    const auto secure_path =
        vqec_vision_ai_stor_apinv_validate_storage_path(config_.database_path_);
    if (secure_path.code_ != status_code::ok) {
        return secure_path;
    }
    sqlite3* candidate = nullptr;
    const int opened = sqlite3_open_v2(config_.database_path_.c_str(), &candidate,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (opened != SQLITE_OK || candidate == nullptr) {
        const std::string message = candidate != nullptr ?
            sqlite3_errmsg(candidate) : "cannot allocate app inventory database";
        if (candidate != nullptr) {
            sqlite3_close(candidate);
        }
        return {status_code::io_error, message};
    }
    database_ = candidate;
    if (chmod(config_.database_path_.c_str(), S_IRUSR | S_IWUSR) != 0) {
        sqlite3_close(database_);
        database_ = nullptr;
        return {status_code::io_error, "cannot protect app inventory database"};
    }
    sqlite3_busy_timeout(database_, config_.busy_timeout_ms_);
    auto configured = vqec_vision_ai_stor_apinv_exec(database_,
        "PRAGMA foreign_keys=ON;PRAGMA journal_mode=WAL;PRAGMA synchronous=FULL;");
    if (configured.code_ == status_code::ok) {
        configured = vqec_vision_ai_stor_apinv_exec(database_, g_schema_sql);
    }
    const std::string wal_path = config_.database_path_ + "-wal";
    const std::string shm_path = config_.database_path_ + "-shm";
    if (std::filesystem::exists(wal_path)) {
        (void)chmod(wal_path.c_str(), S_IRUSR | S_IWUSR);
    }
    if (std::filesystem::exists(shm_path)) {
        (void)chmod(shm_path.c_str(), S_IRUSR | S_IWUSR);
    }
    if (configured.code_ != status_code::ok) {
        sqlite3_close(database_);
        database_ = nullptr;
        return configured;
    }
    sqlite_statement page_size;
    configured = vqec_vision_ai_stor_apinv_prepare(
        database_, "PRAGMA page_size", page_size);
    if (configured.code_ != status_code::ok ||
        sqlite3_step(page_size.vqec_vision_ai_stor_apinv_get()) != SQLITE_ROW) {
        sqlite3_close(database_);
        database_ = nullptr;
        return {status_code::io_error, "cannot resolve app inventory page size"};
    }
    const auto bytes = sqlite3_column_int64(
        page_size.vqec_vision_ai_stor_apinv_get(), 0);
    if (bytes <= 0) {
        sqlite3_close(database_);
        database_ = nullptr;
        return {status_code::invalid_state, "invalid app inventory page size"};
    }
    const auto max_pages = config_.max_database_bytes_ /
        static_cast<std::uint64_t>(bytes);
    configured = vqec_vision_ai_stor_apinv_exec(database_,
        ("PRAGMA max_page_count=" + std::to_string(max_pages)).c_str());
    if (configured.code_ != status_code::ok) {
        sqlite3_close(database_);
        database_ = nullptr;
        return configured;
    }
    runtime_control_snapshot snapshot;
    return vqec_vision_ai_stor_apinv_load(database_, snapshot);
}

status sqlite_app_inventory::vqec_vision_ai_ports_apinv_install(
    const app_install_request& _request, runtime_control_snapshot& _snapshot) {
    if (database_ == nullptr) {
        return {status_code::invalid_state, "app inventory is not open"};
    }
    const auto valid = vqec_vision_ai_core_applc_validate_manifest(_request.manifest_);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    if (!vqec_vision_ai_cntr_ident_is_sha256_hex(_request.manifest_sha256_) ||
        _request.expected_inventory_revision_ == 0 ||
        _request.configuration_revision_ == 0 ||
        _request.configuration_sha256_ !=
            _request.manifest_.configuration_defaults_sha256_ ||
        _request.configuration_payload_.empty() ||
        _request.configuration_payload_.size() > app_lifecycle_limits::g_max_document_bytes ||
        _request.manifest_.requested_scopes_.sources_.empty()) {
        return {status_code::invalid_argument, "invalid app install request"};
    }
    auto started = vqec_vision_ai_stor_apinv_begin(database_);
    if (started.code_ != status_code::ok) {
        return started;
    }
    std::uint64_t snapshot_revision = 0;
    std::uint64_t inventory_revision = 0;
    std::uint64_t entitlement_revision = 0;
    std::uint64_t desired_revision = 0;
    auto current = vqec_vision_ai_stor_apinv_read_revisions(database_, snapshot_revision,
        inventory_revision, entitlement_revision, desired_revision);
    if (current.code_ != status_code::ok ||
        inventory_revision != _request.expected_inventory_revision_) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current.code_ != status_code::ok ? current :
            status{status_code::invalid_state, "stale app inventory revision"};
    }
    sqlite_statement application;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "INSERT INTO applications(app_id,app_version,usecase_version,release_sequence,"
        "manifest_sha256,configuration_schema_id,configuration_revision,"
        "configuration_sha256,configuration_payload,requested_output_scopes) "
        "VALUES(?,?,?,?,?,?,?,?,?,?)", application);
    if (current.code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    auto* app = application.vqec_vision_ai_stor_apinv_get();
    const auto requested_outputs = vqec_vision_ai_stor_apinv_join_scopes(
        _request.manifest_.requested_scopes_.outputs_);
    const bool app_bound =
        vqec_vision_ai_stor_apinv_bind_text(app, 1, _request.manifest_.app_id_) &&
        vqec_vision_ai_stor_apinv_bind_text(app, 2, _request.manifest_.app_version_) &&
        vqec_vision_ai_stor_apinv_bind_text(app, 3, _request.manifest_.usecase_version_) &&
        vqec_vision_ai_stor_apinv_bind_uint64(app, 4,
            _request.manifest_.release_sequence_) &&
        vqec_vision_ai_stor_apinv_bind_text(app, 5, _request.manifest_sha256_) &&
        vqec_vision_ai_stor_apinv_bind_text(app, 6,
            _request.manifest_.configuration_schema_id_) &&
        vqec_vision_ai_stor_apinv_bind_uint64(app, 7,
            _request.configuration_revision_) &&
        vqec_vision_ai_stor_apinv_bind_text(app, 8,
            _request.configuration_sha256_) &&
        vqec_vision_ai_stor_apinv_bind_blob(app, 9,
            _request.configuration_payload_) &&
        vqec_vision_ai_stor_apinv_bind_text(app, 10, requested_outputs);
    if (!app_bound ||
        (current = vqec_vision_ai_stor_apinv_step_done(database_, app)).code_ !=
            status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return !app_bound ? status{status_code::resource_exhausted,
            "app install binding failed"} : current;
    }
    for (const auto& source : _request.manifest_.requested_scopes_.sources_) {
        sqlite_statement statement;
        current = vqec_vision_ai_stor_apinv_prepare(database_,
            "INSERT INTO app_sources(app_id,source_id) VALUES(?,?)", statement);
        auto* handle = statement.vqec_vision_ai_stor_apinv_get();
        if (current.code_ != status_code::ok ||
            !vqec_vision_ai_stor_apinv_bind_text(
                handle, 1, _request.manifest_.app_id_) ||
            !vqec_vision_ai_stor_apinv_bind_text(handle, 2, source) ||
            (current = vqec_vision_ai_stor_apinv_step_done(database_, handle)).code_ !=
                status_code::ok) {
            vqec_vision_ai_stor_apinv_rollback(database_);
            return current.code_ == status_code::ok ?
                status{status_code::io_error, "app source binding failed"} : current;
        }
    }
    for (const auto& component : _request.manifest_.components_) {
        sqlite_statement component_statement;
        current = vqec_vision_ai_stor_apinv_prepare(database_,
            "INSERT INTO components VALUES(?,?,?,?,?,1) ON CONFLICT(component_id,"
            "component_version,target_id,artifact_sha256,semantic_contract_sha256) "
            "DO UPDATE SET reference_count=reference_count+1", component_statement);
        auto* handle = component_statement.vqec_vision_ai_stor_apinv_get();
        const bool bound = current.code_ == status_code::ok &&
            vqec_vision_ai_stor_apinv_bind_text(handle, 1, component.component_id_) &&
            vqec_vision_ai_stor_apinv_bind_text(handle, 2, component.component_version_) &&
            vqec_vision_ai_stor_apinv_bind_text(handle, 3, component.target_id_) &&
            vqec_vision_ai_stor_apinv_bind_text(handle, 4, component.artifact_sha256_) &&
            vqec_vision_ai_stor_apinv_bind_text(
                handle, 5, component.semantic_contract_sha256_);
        if (!bound ||
            (current = vqec_vision_ai_stor_apinv_step_done(database_, handle)).code_ !=
                status_code::ok) {
            vqec_vision_ai_stor_apinv_rollback(database_);
            return !bound ? status{status_code::io_error,
                "component binding failed"} : current;
        }
        sqlite_statement mapping;
        current = vqec_vision_ai_stor_apinv_prepare(database_,
            "INSERT INTO app_components VALUES(?,?,?,?,?,?)", mapping);
        handle = mapping.vqec_vision_ai_stor_apinv_get();
        const bool mapped = current.code_ == status_code::ok &&
            vqec_vision_ai_stor_apinv_bind_text(handle, 1, _request.manifest_.app_id_) &&
            vqec_vision_ai_stor_apinv_bind_text(handle, 2, component.component_id_) &&
            vqec_vision_ai_stor_apinv_bind_text(handle, 3, component.component_version_) &&
            vqec_vision_ai_stor_apinv_bind_text(handle, 4, component.target_id_) &&
            vqec_vision_ai_stor_apinv_bind_text(handle, 5, component.artifact_sha256_) &&
            vqec_vision_ai_stor_apinv_bind_text(
                handle, 6, component.semantic_contract_sha256_);
        if (!mapped ||
            (current = vqec_vision_ai_stor_apinv_step_done(database_, handle)).code_ !=
                status_code::ok) {
            vqec_vision_ai_stor_apinv_rollback(database_);
            return !mapped ? status{status_code::io_error,
                "app component mapping failed"} : current;
        }
    }
    current = vqec_vision_ai_stor_apinv_update_revisions(database_,
        snapshot_revision + 1U, inventory_revision + 1U,
        entitlement_revision, desired_revision);
    if (current.code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    return vqec_vision_ai_stor_apinv_finish_mutation(database_, _snapshot);
}

status sqlite_app_inventory::vqec_vision_ai_ports_apinv_update_configuration(
    const app_configuration_update& _update,
    runtime_control_snapshot& _snapshot) {
    if (database_ == nullptr) {
        return {status_code::invalid_state, "app inventory is not open"};
    }
    if (!vqec_vision_ai_cntr_ident_is_valid(
            _update.app_id_, app_lifecycle_limits::g_max_identifier_bytes) ||
        _update.expected_configuration_revision_ == 0 ||
        _update.configuration_revision_ !=
            _update.expected_configuration_revision_ + 1U ||
        !vqec_vision_ai_cntr_ident_is_sha256_hex(_update.configuration_sha256_) ||
        _update.configuration_payload_.empty() ||
        _update.configuration_payload_.size() > app_lifecycle_limits::g_max_document_bytes) {
        return {status_code::invalid_argument, "invalid app configuration update"};
    }
    auto current = vqec_vision_ai_stor_apinv_begin(database_);
    if (current.code_ != status_code::ok) {
        return current;
    }
    sqlite_statement statement;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "UPDATE applications SET configuration_revision=?,configuration_sha256=?,"
        "configuration_payload=? WHERE app_id=? AND configuration_revision=?", statement);
    auto* handle = statement.vqec_vision_ai_stor_apinv_get();
    const bool bound = current.code_ == status_code::ok &&
        vqec_vision_ai_stor_apinv_bind_uint64(
            handle, 1, _update.configuration_revision_) &&
        vqec_vision_ai_stor_apinv_bind_text(
            handle, 2, _update.configuration_sha256_) &&
        vqec_vision_ai_stor_apinv_bind_blob(
            handle, 3, _update.configuration_payload_) &&
        vqec_vision_ai_stor_apinv_bind_text(handle, 4, _update.app_id_) &&
        vqec_vision_ai_stor_apinv_bind_uint64(
            handle, 5, _update.expected_configuration_revision_);
    if (!bound ||
        (current = vqec_vision_ai_stor_apinv_step_done(database_, handle)).code_ !=
            status_code::ok || sqlite3_changes(database_) != 1) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return !bound || current.code_ == status_code::ok ?
            status{status_code::invalid_state, "stale app configuration revision"} : current;
    }
    std::uint64_t snapshot_revision = 0;
    std::uint64_t inventory_revision = 0;
    std::uint64_t entitlement_revision = 0;
    std::uint64_t desired_revision = 0;
    current = vqec_vision_ai_stor_apinv_read_revisions(database_, snapshot_revision,
        inventory_revision, entitlement_revision, desired_revision);
    if (current.code_ == status_code::ok) {
        current = vqec_vision_ai_stor_apinv_update_revisions(database_,
            snapshot_revision + 1U, inventory_revision + 1U,
            entitlement_revision, desired_revision);
    }
    if (current.code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    return vqec_vision_ai_stor_apinv_finish_mutation(database_, _snapshot);
}

status sqlite_app_inventory::vqec_vision_ai_ports_apinv_update_authority(
    const app_authority_update& _update,
    runtime_control_snapshot& _snapshot) {
    if (database_ == nullptr) {
        return {status_code::invalid_state, "app inventory is not open"};
    }
    if (!vqec_vision_ai_cntr_ident_is_valid(
            _update.app_id_, app_lifecycle_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _update.source_id_, app_lifecycle_limits::g_max_identifier_bytes) ||
        _update.expected_entitlement_revision_ == 0 ||
        (_update.entitled_ && _update.entitlement_expires_utc_ns_ == 0) ||
        (!_update.entitled_ && !_update.output_scopes_.empty()) ||
        !vqec_vision_ai_stor_apinv_are_unique_scopes(_update.output_scopes_) ||
        (!_update.reason_code_.empty() && !vqec_vision_ai_cntr_ident_is_valid(
            _update.reason_code_, app_lifecycle_limits::g_max_identifier_bytes))) {
        return {status_code::invalid_argument, "invalid app authority update"};
    }
    auto current = vqec_vision_ai_stor_apinv_begin(database_);
    if (current.code_ != status_code::ok) {
        return current;
    }
    std::vector<std::string> requested;
    sqlite_statement requested_statement;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "SELECT requested_output_scopes FROM applications WHERE app_id=?",
        requested_statement);
    auto* requested_handle = requested_statement.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(requested_handle, 1, _update.app_id_) ||
        sqlite3_step(requested_handle) != SQLITE_ROW ||
        (current = vqec_vision_ai_stor_apinv_split_scopes(
            sqlite3_column_text(requested_handle, 0), requested)).code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_is_subset(_update.output_scopes_, requested)) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current.code_ != status_code::ok ? current :
            status{status_code::unauthorized, "authority exceeds requested app scopes"};
    }
    std::uint64_t snapshot_revision = 0;
    std::uint64_t inventory_revision = 0;
    std::uint64_t entitlement_revision = 0;
    std::uint64_t desired_revision = 0;
    current = vqec_vision_ai_stor_apinv_read_revisions(database_, snapshot_revision,
        inventory_revision, entitlement_revision, desired_revision);
    if (current.code_ != status_code::ok ||
        entitlement_revision != _update.expected_entitlement_revision_) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current.code_ != status_code::ok ? current :
            status{status_code::invalid_state, "stale entitlement revision"};
    }
    sqlite_statement statement;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "UPDATE app_sources SET entitled=?,supported=?,compatible=?,admitted=?,"
        "entitlement_expires_utc_ns=?,output_scopes=?,reason_code=? "
        "WHERE app_id=? AND source_id=?", statement);
    auto* handle = statement.vqec_vision_ai_stor_apinv_get();
    const auto scopes = vqec_vision_ai_stor_apinv_join_scopes(_update.output_scopes_);
    const auto reason = _update.reason_code_.empty() ? "ok" : _update.reason_code_;
    const bool bound = current.code_ == status_code::ok &&
        sqlite3_bind_int(handle, 1, _update.entitled_ ? 1 : 0) == SQLITE_OK &&
        sqlite3_bind_int(handle, 2, _update.supported_ ? 1 : 0) == SQLITE_OK &&
        sqlite3_bind_int(handle, 3, _update.compatible_ ? 1 : 0) == SQLITE_OK &&
        sqlite3_bind_int(handle, 4, _update.admitted_ ? 1 : 0) == SQLITE_OK &&
        vqec_vision_ai_stor_apinv_bind_uint64(
            handle, 5, _update.entitlement_expires_utc_ns_) &&
        vqec_vision_ai_stor_apinv_bind_text(handle, 6, scopes) &&
        vqec_vision_ai_stor_apinv_bind_text(handle, 7, reason) &&
        vqec_vision_ai_stor_apinv_bind_text(handle, 8, _update.app_id_) &&
        vqec_vision_ai_stor_apinv_bind_text(handle, 9, _update.source_id_);
    if (!bound ||
        (current = vqec_vision_ai_stor_apinv_step_done(database_, handle)).code_ !=
            status_code::ok || sqlite3_changes(database_) != 1) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return !bound || current.code_ == status_code::ok ?
            status{status_code::invalid_state, "app association is not installed"} : current;
    }
    current = vqec_vision_ai_stor_apinv_update_revisions(database_,
        snapshot_revision + 1U, inventory_revision,
        entitlement_revision + 1U, desired_revision);
    if (current.code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    return vqec_vision_ai_stor_apinv_finish_mutation(database_, _snapshot);
}

status sqlite_app_inventory::vqec_vision_ai_ports_apinv_set_desired(
    const app_desired_update& _update,
    runtime_control_snapshot& _snapshot) {
    if (database_ == nullptr) {
        return {status_code::invalid_state, "app inventory is not open"};
    }
    if (!vqec_vision_ai_cntr_ident_is_valid(
            _update.app_id_, app_lifecycle_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _update.source_id_, app_lifecycle_limits::g_max_identifier_bytes) ||
        _update.expected_desired_revision_ == 0) {
        return {status_code::invalid_argument, "invalid app desired update"};
    }
    auto current = vqec_vision_ai_stor_apinv_begin(database_);
    if (current.code_ != status_code::ok) {
        return current;
    }
    std::uint64_t snapshot_revision = 0;
    std::uint64_t inventory_revision = 0;
    std::uint64_t entitlement_revision = 0;
    std::uint64_t desired_revision = 0;
    current = vqec_vision_ai_stor_apinv_read_revisions(database_, snapshot_revision,
        inventory_revision, entitlement_revision, desired_revision);
    if (current.code_ != status_code::ok ||
        desired_revision != _update.expected_desired_revision_) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current.code_ != status_code::ok ? current :
            status{status_code::invalid_state, "stale desired revision"};
    }
    sqlite_statement statement;
    const char* sql = _update.desired_ ?
        "UPDATE app_sources SET desired=1 WHERE app_id=? AND source_id=? AND "
        "entitled=1 AND supported=1 AND compatible=1 AND admitted=1" :
        "UPDATE app_sources SET desired=0 WHERE app_id=? AND source_id=?";
    current = vqec_vision_ai_stor_apinv_prepare(database_, sql, statement);
    auto* handle = statement.vqec_vision_ai_stor_apinv_get();
    const bool bound = current.code_ == status_code::ok &&
        vqec_vision_ai_stor_apinv_bind_text(handle, 1, _update.app_id_) &&
        vqec_vision_ai_stor_apinv_bind_text(handle, 2, _update.source_id_);
    if (!bound ||
        (current = vqec_vision_ai_stor_apinv_step_done(database_, handle)).code_ !=
            status_code::ok || sqlite3_changes(database_) != 1) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return !bound || current.code_ == status_code::ok ?
            status{status_code::invalid_state,
                _update.desired_ ? "app is not effective-ready" :
                    "app association is not installed"} : current;
    }
    current = vqec_vision_ai_stor_apinv_update_revisions(database_,
        snapshot_revision + 1U, inventory_revision,
        entitlement_revision, desired_revision + 1U);
    if (current.code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    return vqec_vision_ai_stor_apinv_finish_mutation(database_, _snapshot);
}

status sqlite_app_inventory::vqec_vision_ai_ports_apinv_uninstall(
    const std::string& _app_id, std::uint64_t _expected_inventory_revision,
    runtime_control_snapshot& _snapshot) {
    if (database_ == nullptr) {
        return {status_code::invalid_state, "app inventory is not open"};
    }
    if (!vqec_vision_ai_cntr_ident_is_valid(
            _app_id, app_lifecycle_limits::g_max_identifier_bytes) ||
        _expected_inventory_revision == 0) {
        return {status_code::invalid_argument, "invalid app uninstall request"};
    }
    auto current = vqec_vision_ai_stor_apinv_begin(database_);
    if (current.code_ != status_code::ok) {
        return current;
    }
    std::uint64_t snapshot_revision = 0;
    std::uint64_t inventory_revision = 0;
    std::uint64_t entitlement_revision = 0;
    std::uint64_t desired_revision = 0;
    current = vqec_vision_ai_stor_apinv_read_revisions(database_, snapshot_revision,
        inventory_revision, entitlement_revision, desired_revision);
    if (current.code_ != status_code::ok ||
        inventory_revision != _expected_inventory_revision) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current.code_ != status_code::ok ? current :
            status{status_code::invalid_state, "stale app inventory revision"};
    }
    sqlite_statement desired;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "SELECT COUNT(*) FROM app_sources WHERE app_id=? AND desired=1", desired);
    auto* handle = desired.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(handle, 1, _app_id) ||
        sqlite3_step(handle) != SQLITE_ROW || sqlite3_column_int64(handle, 0) != 0) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return {status_code::invalid_state, "app must be disabled before uninstall"};
    }
    sqlite_statement decrement;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "UPDATE components SET reference_count=reference_count-1 WHERE EXISTS("
        "SELECT 1 FROM app_components m WHERE m.app_id=? AND "
        "m.component_id=components.component_id AND "
        "m.component_version=components.component_version AND "
        "m.target_id=components.target_id AND "
        "m.artifact_sha256=components.artifact_sha256 AND "
        "m.semantic_contract_sha256=components.semantic_contract_sha256)", decrement);
    handle = decrement.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(handle, 1, _app_id) ||
        (current = vqec_vision_ai_stor_apinv_step_done(database_, handle)).code_ !=
            status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    sqlite_statement erase;
    current = vqec_vision_ai_stor_apinv_prepare(
        database_, "DELETE FROM applications WHERE app_id=?", erase);
    handle = erase.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(handle, 1, _app_id) ||
        (current = vqec_vision_ai_stor_apinv_step_done(database_, handle)).code_ !=
            status_code::ok || sqlite3_changes(database_) != 1) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current.code_ == status_code::ok ?
            status{status_code::invalid_state, "app is not installed"} : current;
    }
    current = vqec_vision_ai_stor_apinv_exec(
        database_, "DELETE FROM components WHERE reference_count=0");
    if (current.code_ == status_code::ok) {
        current = vqec_vision_ai_stor_apinv_update_revisions(database_,
            snapshot_revision + 1U, inventory_revision + 1U,
            entitlement_revision, desired_revision);
    }
    if (current.code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    return vqec_vision_ai_stor_apinv_finish_mutation(database_, _snapshot);
}

status sqlite_app_inventory::vqec_vision_ai_ports_apinv_load_snapshot(
    runtime_control_snapshot& _snapshot) const {
    if (database_ == nullptr) {
        return {status_code::invalid_state, "app inventory is not open"};
    }
    return vqec_vision_ai_stor_apinv_load(database_, _snapshot);
}

}  // namespace vqec::vision::ai
