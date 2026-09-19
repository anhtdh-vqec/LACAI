#include "vqec_vision_sqlite_app_inventory.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <sys/stat.h>
#include <unistd.h>

#include <sqlite3.h>

#include "vqec/vision/ai/contracts/base/vqec_vision_identifier.hpp"

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
CREATE TABLE IF NOT EXISTS app_grants(
  app_id TEXT NOT NULL,
  source_id TEXT NOT NULL,
  entitled INTEGER NOT NULL,
  entitlement_expires_utc_ns INTEGER NOT NULL,
  output_scopes TEXT NOT NULL,
  reason_code TEXT NOT NULL,
  PRIMARY KEY(app_id,source_id)
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
CREATE TABLE IF NOT EXISTS app_component_details(
  app_id TEXT NOT NULL REFERENCES applications(app_id) ON DELETE CASCADE,
  component_id TEXT NOT NULL,
  component_version TEXT NOT NULL,
  component_type INTEGER NOT NULL,
  target_id TEXT NOT NULL,
  artifact_sha256 TEXT NOT NULL,
  artifact_bytes INTEGER NOT NULL,
  semantic_contract_sha256 TEXT NOT NULL,
  model_role INTEGER NOT NULL,
  immutable_location TEXT NOT NULL,
  PRIMARY KEY(app_id,component_id,component_version,target_id)
);
CREATE TABLE IF NOT EXISTS app_rollback_applications(
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
CREATE TABLE IF NOT EXISTS app_rollback_components(
  app_id TEXT NOT NULL,
  component_id TEXT NOT NULL,
  component_version TEXT NOT NULL,
  component_type INTEGER NOT NULL,
  target_id TEXT NOT NULL,
  artifact_sha256 TEXT NOT NULL,
  artifact_bytes INTEGER NOT NULL,
  semantic_contract_sha256 TEXT NOT NULL,
  model_role INTEGER NOT NULL,
  immutable_location TEXT NOT NULL,
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
        "SELECT s.app_id,s.source_id,a.app_version,a.release_sequence,"
        "s.entitled,s.desired,s.supported,s.compatible,"
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
        const auto* app_version = sqlite3_column_text(handle, 2);
        const auto* config_sha = sqlite3_column_text(handle, 10);
        const auto* config_schema = sqlite3_column_text(handle, 11);
        const auto* reason = sqlite3_column_text(handle, 14);
        if (app_id == nullptr || source_id == nullptr || app_version == nullptr ||
            config_sha == nullptr || config_schema == nullptr || reason == nullptr) {
            return {status_code::invalid_state, "corrupt app inventory row"};
        }
        association.app_id_ = reinterpret_cast<const char*>(app_id);
        association.source_id_ = reinterpret_cast<const char*>(source_id);
        association.app_version_ = reinterpret_cast<const char*>(app_version);
        const auto release_sequence = sqlite3_column_int64(handle, 3);
        if (release_sequence <= 0) {
            return {status_code::invalid_state,
                "corrupt app inventory release sequence"};
        }
        association.release_sequence_ = static_cast<std::uint64_t>(release_sequence);
        association.installed_ = true;
        association.entitled_ = sqlite3_column_int(handle, 4) != 0;
        association.desired_ = sqlite3_column_int(handle, 5) != 0;
        association.supported_ = sqlite3_column_int(handle, 6) != 0;
        association.compatible_ = sqlite3_column_int(handle, 7) != 0;
        association.admitted_ = sqlite3_column_int(handle, 8) != 0;
        const auto config_revision = sqlite3_column_int64(handle, 9);
        const auto expiry = sqlite3_column_int64(handle, 15);
        if (config_revision <= 0 || expiry < 0) {
            return {status_code::invalid_state, "corrupt app inventory numeric field"};
        }
        association.configuration_revision_ =
            static_cast<std::uint64_t>(config_revision);
        association.configuration_sha256_ = reinterpret_cast<const char*>(config_sha);
        association.configuration_schema_id_ =
            reinterpret_cast<const char*>(config_schema);
        const auto* payload = static_cast<const std::uint8_t*>(
            sqlite3_column_blob(handle, 12));
        const int payload_bytes = sqlite3_column_bytes(handle, 12);
        if (payload == nullptr || payload_bytes <= 0 ||
            static_cast<std::size_t>(payload_bytes) >
                app_lifecycle_limits::g_max_document_bytes) {
            return {status_code::invalid_state, "corrupt app configuration payload"};
        }
        association.configuration_payload_.assign(payload, payload + payload_bytes);
        const auto split = vqec_vision_ai_stor_apinv_split_scopes(
            sqlite3_column_text(handle, 13), association.output_scopes_);
        if (split.code_ != status_code::ok) {
            return split;
        }
        association.reason_code_ = reinterpret_cast<const char*>(reason);
        association.entitlement_expires_utc_ns_ = static_cast<std::uint64_t>(expiry);
        sqlite_statement components;
        auto component_status = vqec_vision_ai_stor_apinv_prepare(_database,
            "SELECT component_id,component_version,component_type,target_id,"
            "artifact_sha256,artifact_bytes,semantic_contract_sha256,model_role,"
            "immutable_location FROM app_component_details WHERE app_id=? "
            "ORDER BY component_id,component_version,target_id", components);
        auto* component_handle = components.vqec_vision_ai_stor_apinv_get();
        if (component_status.code_ != status_code::ok ||
            !vqec_vision_ai_stor_apinv_bind_text(
                component_handle, 1, association.app_id_)) {
            return component_status.code_ != status_code::ok ? component_status :
                status{status_code::io_error,
                    "cannot bind runtime app component query"};
        }
        while (true) {
            const int component_result = sqlite3_step(component_handle);
            if (component_result == SQLITE_DONE) {
                break;
            }
            if (component_result != SQLITE_ROW ||
                association.components_.size() == app_lifecycle_limits::g_max_components) {
                return {status_code::invalid_state,
                    "corrupt runtime app component set"};
            }
            const auto* component_id = sqlite3_column_text(component_handle, 0);
            const auto* component_version = sqlite3_column_text(component_handle, 1);
            const auto component_type = sqlite3_column_int(component_handle, 2);
            const auto* target_id = sqlite3_column_text(component_handle, 3);
            const auto* artifact_sha = sqlite3_column_text(component_handle, 4);
            const auto artifact_bytes = sqlite3_column_int64(component_handle, 5);
            const auto* semantic_sha = sqlite3_column_text(component_handle, 6);
            const auto model_role = sqlite3_column_int(component_handle, 7);
            const auto* immutable_location = sqlite3_column_text(component_handle, 8);
            if (component_id == nullptr || component_version == nullptr ||
                target_id == nullptr || artifact_sha == nullptr || semantic_sha == nullptr ||
                immutable_location == nullptr || artifact_bytes <= 0 ||
                component_type < static_cast<int>(app_component_type::model) ||
                component_type > static_cast<int>(app_component_type::configuration) ||
                model_role < static_cast<int>(app_model_role::none) ||
                model_role > static_cast<int>(app_model_role::offline)) {
                return {status_code::invalid_state,
                    "corrupt runtime app component row"};
            }
            app_runtime_component component;
            component.component_id_ = reinterpret_cast<const char*>(component_id);
            component.component_version_ = reinterpret_cast<const char*>(component_version);
            component.type_ = static_cast<app_component_type>(component_type);
            component.target_id_ = reinterpret_cast<const char*>(target_id);
            component.artifact_sha256_ = reinterpret_cast<const char*>(artifact_sha);
            component.artifact_bytes_ = static_cast<std::uint64_t>(artifact_bytes);
            component.semantic_contract_sha256_ =
                reinterpret_cast<const char*>(semantic_sha);
            component.model_role_ = static_cast<app_model_role>(model_role);
            component.immutable_location_ =
                reinterpret_cast<const char*>(immutable_location);
            association.components_.push_back(std::move(component));
        }
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

const char* vqec_vision_ai_stor_apinv_effective_reason(
    bool _supported, bool _compatible, bool _admitted) noexcept {
    if (!_supported) {
        return "unsupported";
    }
    if (!_compatible) {
        return "incompatible";
    }
    if (!_admitted) {
        return "resource_limited";
    }
    return "verified";
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

status sqlite_app_inventory::vqec_vision_ai_ports_apinv_authorize_install(
    const usecase_app_manifest& _manifest,
    std::uint64_t _expected_inventory_revision) const {
    if (database_ == nullptr) {
        return {status_code::invalid_state, "app inventory is not open"};
    }
    const auto valid = vqec_vision_ai_core_applc_validate_manifest(_manifest);
    if (valid.code_ != status_code::ok || _expected_inventory_revision == 0) {
        return valid.code_ != status_code::ok ? valid :
            status{status_code::invalid_argument,
                "invalid app install authorization request"};
    }
    std::uint64_t snapshot_revision = 0;
    std::uint64_t inventory_revision = 0;
    std::uint64_t entitlement_revision = 0;
    std::uint64_t desired_revision = 0;
    auto current = vqec_vision_ai_stor_apinv_read_revisions(database_, snapshot_revision,
        inventory_revision, entitlement_revision, desired_revision);
    if (current.code_ != status_code::ok ||
        inventory_revision != _expected_inventory_revision) {
        return current.code_ != status_code::ok ? current :
            status{status_code::invalid_state, "stale app inventory revision"};
    }
    const auto utc_now_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    for (const auto& source : _manifest.requested_scopes_.sources_) {
        sqlite_statement statement;
        current = vqec_vision_ai_stor_apinv_prepare(database_,
            "SELECT entitled,entitlement_expires_utc_ns,output_scopes "
            "FROM app_grants WHERE app_id=? AND source_id=?", statement);
        auto* handle = statement.vqec_vision_ai_stor_apinv_get();
        if (current.code_ != status_code::ok ||
            !vqec_vision_ai_stor_apinv_bind_text(handle, 1, _manifest.app_id_) ||
            !vqec_vision_ai_stor_apinv_bind_text(handle, 2, source) ||
            sqlite3_step(handle) != SQLITE_ROW) {
            return {status_code::unauthorized,
                "active entitlement is required before package staging"};
        }
        const auto expiry = sqlite3_column_int64(handle, 1);
        std::vector<std::string> scopes;
        if (sqlite3_column_int(handle, 0) == 0 || expiry <= 0 ||
            static_cast<std::uint64_t>(expiry) <= utc_now_ns ||
            (current = vqec_vision_ai_stor_apinv_split_scopes(
                sqlite3_column_text(handle, 2), scopes)).code_ != status_code::ok ||
            !vqec_vision_ai_stor_apinv_is_subset(
                scopes, _manifest.requested_scopes_.outputs_)) {
            return current.code_ != status_code::ok ? current :
                status{status_code::unauthorized,
                    "entitlement is expired or exceeds package scopes"};
        }
    }
    return {};
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
        _request.manifest_.requested_scopes_.sources_.empty() ||
        _request.components_.empty() ||
        _request.components_.size() > app_lifecycle_limits::g_max_components) {
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
    current = vqec_vision_ai_ports_apinv_authorize_install(
        _request.manifest_, _request.expected_inventory_revision_);
    if (current.code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    struct install_grant {
        std::uint64_t expires_utc_ns_{0};
        std::vector<std::string> output_scopes_;
        std::string reason_code_;
    };
    std::vector<install_grant> grants;
    grants.reserve(_request.manifest_.requested_scopes_.sources_.size());
    const auto utc_now_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    for (const auto& source : _request.manifest_.requested_scopes_.sources_) {
        sqlite_statement grant_statement;
        current = vqec_vision_ai_stor_apinv_prepare(database_,
            "SELECT entitled,entitlement_expires_utc_ns,output_scopes,reason_code "
            "FROM app_grants WHERE app_id=? AND source_id=?", grant_statement);
        auto* grant_handle = grant_statement.vqec_vision_ai_stor_apinv_get();
        if (current.code_ != status_code::ok ||
            !vqec_vision_ai_stor_apinv_bind_text(
                grant_handle, 1, _request.manifest_.app_id_) ||
            !vqec_vision_ai_stor_apinv_bind_text(grant_handle, 2, source) ||
            sqlite3_step(grant_handle) != SQLITE_ROW) {
            vqec_vision_ai_stor_apinv_rollback(database_);
            return {status_code::unauthorized,
                "active entitlement is required before app install"};
        }
        const auto expiry = sqlite3_column_int64(grant_handle, 1);
        const auto* reason = sqlite3_column_text(grant_handle, 3);
        install_grant grant;
        if (sqlite3_column_int(grant_handle, 0) == 0 || expiry <= 0 ||
            static_cast<std::uint64_t>(expiry) <= utc_now_ns || reason == nullptr ||
            (current = vqec_vision_ai_stor_apinv_split_scopes(
                sqlite3_column_text(grant_handle, 2), grant.output_scopes_)).code_ !=
                status_code::ok ||
            !vqec_vision_ai_stor_apinv_is_subset(grant.output_scopes_,
                _request.manifest_.requested_scopes_.outputs_)) {
            vqec_vision_ai_stor_apinv_rollback(database_);
            return current.code_ != status_code::ok ? current :
                status{status_code::unauthorized,
                    "entitlement is expired or exceeds package scopes"};
        }
        grant.expires_utc_ns_ = static_cast<std::uint64_t>(expiry);
        grant.reason_code_ = reinterpret_cast<const char*>(reason);
        grants.push_back(std::move(grant));
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
    for (std::size_t source_index = 0;
         source_index < _request.manifest_.requested_scopes_.sources_.size();
         ++source_index) {
        const auto& source = _request.manifest_.requested_scopes_.sources_[source_index];
        const auto& grant = grants[source_index];
        sqlite_statement statement;
        current = vqec_vision_ai_stor_apinv_prepare(database_,
            "INSERT INTO app_sources(app_id,source_id,entitled,supported,compatible,"
            "admitted,entitlement_expires_utc_ns,output_scopes,reason_code) "
            "VALUES(?,?,?,?,?,?,?,?,?)", statement);
        auto* handle = statement.vqec_vision_ai_stor_apinv_get();
        const auto scopes = vqec_vision_ai_stor_apinv_join_scopes(grant.output_scopes_);
        if (current.code_ != status_code::ok ||
            !vqec_vision_ai_stor_apinv_bind_text(
                handle, 1, _request.manifest_.app_id_) ||
            !vqec_vision_ai_stor_apinv_bind_text(handle, 2, source) ||
            sqlite3_bind_int(handle, 3, 1) != SQLITE_OK ||
            sqlite3_bind_int(handle, 4, _request.supported_ ? 1 : 0) != SQLITE_OK ||
            sqlite3_bind_int(handle, 5, _request.compatible_ ? 1 : 0) != SQLITE_OK ||
            sqlite3_bind_int(handle, 6, _request.admitted_ ? 1 : 0) != SQLITE_OK ||
            !vqec_vision_ai_stor_apinv_bind_uint64(
                handle, 7, grant.expires_utc_ns_) ||
            !vqec_vision_ai_stor_apinv_bind_text(handle, 8, scopes) ||
            !vqec_vision_ai_stor_apinv_bind_text(handle, 9,
                vqec_vision_ai_stor_apinv_effective_reason(_request.supported_,
                    _request.compatible_, _request.admitted_)) ||
            (current = vqec_vision_ai_stor_apinv_step_done(database_, handle)).code_ !=
                status_code::ok) {
            vqec_vision_ai_stor_apinv_rollback(database_);
            return current.code_ == status_code::ok ?
                status{status_code::io_error, "app source binding failed"} : current;
        }
    }
    for (const auto& installed_component : _request.components_) {
        const auto& component = installed_component.manifest_;
        const auto declared = std::find_if(_request.manifest_.components_.begin(),
            _request.manifest_.components_.end(), [&component](const auto& _value) {
                return _value.component_id_ == component.component_id_ &&
                    _value.component_version_ == component.component_version_ &&
                    _value.target_id_ == component.target_id_ &&
                    _value.artifact_sha256_ == component.artifact_sha256_ &&
                    _value.artifact_bytes_ == component.artifact_bytes_ &&
                    _value.semantic_contract_sha256_ ==
                        component.semantic_contract_sha256_;
            });
        if (declared == _request.manifest_.components_.end() ||
            installed_component.immutable_location_.empty()) {
            vqec_vision_ai_stor_apinv_rollback(database_);
            return {status_code::protocol_error,
                "installed component differs from verified manifest"};
        }
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
        sqlite_statement details;
        current = vqec_vision_ai_stor_apinv_prepare(database_,
            "INSERT INTO app_component_details VALUES(?,?,?,?,?,?,?,?,?,?)", details);
        handle = details.vqec_vision_ai_stor_apinv_get();
        const bool detailed = current.code_ == status_code::ok &&
            vqec_vision_ai_stor_apinv_bind_text(handle, 1, _request.manifest_.app_id_) &&
            vqec_vision_ai_stor_apinv_bind_text(handle, 2, component.component_id_) &&
            vqec_vision_ai_stor_apinv_bind_text(handle, 3, component.component_version_) &&
            sqlite3_bind_int(handle, 4, static_cast<int>(component.type_)) == SQLITE_OK &&
            vqec_vision_ai_stor_apinv_bind_text(handle, 5, component.target_id_) &&
            vqec_vision_ai_stor_apinv_bind_text(handle, 6, component.artifact_sha256_) &&
            vqec_vision_ai_stor_apinv_bind_uint64(handle, 7, component.artifact_bytes_) &&
            vqec_vision_ai_stor_apinv_bind_text(
                handle, 8, component.semantic_contract_sha256_) &&
            sqlite3_bind_int(handle, 9, static_cast<int>(component.model_role_)) ==
                SQLITE_OK &&
            vqec_vision_ai_stor_apinv_bind_text(
                handle, 10, installed_component.immutable_location_);
        if (!detailed ||
            (current = vqec_vision_ai_stor_apinv_step_done(database_, handle)).code_ !=
                status_code::ok) {
            vqec_vision_ai_stor_apinv_rollback(database_);
            return !detailed ? status{status_code::io_error,
                "app component detail binding failed"} : current;
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

status sqlite_app_inventory::vqec_vision_ai_ports_apinv_update(
    const app_install_request& _request, runtime_control_snapshot& _snapshot) {
    if (database_ == nullptr) {
        return {status_code::invalid_state, "app inventory is not open"};
    }
    const auto valid = vqec_vision_ai_core_applc_validate_manifest(_request.manifest_);
    if (valid.code_ != status_code::ok ||
        !vqec_vision_ai_cntr_ident_is_sha256_hex(_request.manifest_sha256_) ||
        _request.expected_inventory_revision_ == 0 ||
        _request.components_.empty() ||
        _request.components_.size() > app_lifecycle_limits::g_max_components ||
        _request.configuration_payload_.empty() ||
        _request.configuration_payload_.size() > app_lifecycle_limits::g_max_document_bytes ||
        _request.configuration_sha256_ !=
            _request.manifest_.configuration_defaults_sha256_) {
        return valid.code_ != status_code::ok ? valid :
            status{status_code::invalid_argument, "invalid app update request"};
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
        inventory_revision != _request.expected_inventory_revision_) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current.code_ != status_code::ok ? current :
            status{status_code::invalid_state, "stale app inventory revision"};
    }
    current = vqec_vision_ai_ports_apinv_authorize_install(
        _request.manifest_, _request.expected_inventory_revision_);
    if (current.code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    sqlite_statement installed;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "SELECT app_version,release_sequence,configuration_schema_id,"
        "configuration_revision,(SELECT COUNT(*) FROM app_sources s "
        "WHERE s.app_id=a.app_id AND s.desired=1),"
        "(SELECT COUNT(*) FROM app_sources s WHERE s.app_id=a.app_id) "
        "FROM applications a WHERE app_id=?", installed);
    auto* installed_handle = installed.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(
            installed_handle, 1, _request.manifest_.app_id_) ||
        sqlite3_step(installed_handle) != SQLITE_ROW) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return {status_code::invalid_state, "application is not installed"};
    }
    const auto* current_version_text = sqlite3_column_text(installed_handle, 0);
    const auto current_release = sqlite3_column_int64(installed_handle, 1);
    const auto* current_schema_text = sqlite3_column_text(installed_handle, 2);
    const auto current_configuration_revision = sqlite3_column_int64(installed_handle, 3);
    const auto desired_count = sqlite3_column_int64(installed_handle, 4);
    const auto source_count = sqlite3_column_int64(installed_handle, 5);
    if (current_version_text == nullptr || current_schema_text == nullptr ||
        current_release <= 0 || current_configuration_revision <= 0 ||
        desired_count != 0 || source_count != static_cast<sqlite3_int64>(
            _request.manifest_.requested_scopes_.sources_.size()) ||
        _request.manifest_.release_sequence_ <=
            static_cast<std::uint64_t>(current_release) ||
        _request.manifest_.rollback_predecessor_ !=
            reinterpret_cast<const char*>(current_version_text) ||
        _request.manifest_.configuration_schema_id_ !=
            reinterpret_cast<const char*>(current_schema_text) ||
        _request.configuration_revision_ !=
            static_cast<std::uint64_t>(current_configuration_revision) + 1U) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return {status_code::invalid_state,
            "app update is not a disabled compatible successor"};
    }
    for (const auto& source : _request.manifest_.requested_scopes_.sources_) {
        sqlite_statement source_statement;
        current = vqec_vision_ai_stor_apinv_prepare(database_,
            "SELECT 1 FROM app_sources WHERE app_id=? AND source_id=?",
            source_statement);
        auto* source_handle = source_statement.vqec_vision_ai_stor_apinv_get();
        if (current.code_ != status_code::ok ||
            !vqec_vision_ai_stor_apinv_bind_text(
                source_handle, 1, _request.manifest_.app_id_) ||
            !vqec_vision_ai_stor_apinv_bind_text(source_handle, 2, source) ||
            sqlite3_step(source_handle) != SQLITE_ROW) {
            vqec_vision_ai_stor_apinv_rollback(database_);
            return {status_code::unsupported,
                "app update changes the installed source closure"};
        }
    }
    for (const auto& component : _request.components_) {
        const auto declared = std::find_if(_request.manifest_.components_.begin(),
            _request.manifest_.components_.end(), [&component](const auto& _value) {
                return _value.component_id_ == component.manifest_.component_id_ &&
                    _value.component_version_ == component.manifest_.component_version_ &&
                    _value.target_id_ == component.manifest_.target_id_ &&
                    _value.artifact_sha256_ == component.manifest_.artifact_sha256_ &&
                    _value.artifact_bytes_ == component.manifest_.artifact_bytes_ &&
                    _value.semantic_contract_sha256_ ==
                        component.manifest_.semantic_contract_sha256_;
            });
        if (declared == _request.manifest_.components_.end() ||
            component.immutable_location_.empty()) {
            vqec_vision_ai_stor_apinv_rollback(database_);
            return {status_code::protocol_error,
                "updated component differs from verified manifest"};
        }
        sqlite_statement previous;
        current = vqec_vision_ai_stor_apinv_prepare(database_,
            "SELECT component_type,semantic_contract_sha256,model_role "
            "FROM app_component_details WHERE app_id=? AND component_id=?",
            previous);
        auto* previous_handle = previous.vqec_vision_ai_stor_apinv_get();
        if (current.code_ != status_code::ok ||
            !vqec_vision_ai_stor_apinv_bind_text(
                previous_handle, 1, _request.manifest_.app_id_) ||
            !vqec_vision_ai_stor_apinv_bind_text(
                previous_handle, 2, component.manifest_.component_id_) ||
            sqlite3_step(previous_handle) != SQLITE_ROW ||
            sqlite3_column_int(previous_handle, 0) !=
                static_cast<int>(component.manifest_.type_) ||
            sqlite3_column_text(previous_handle, 1) == nullptr ||
            reinterpret_cast<const char*>(sqlite3_column_text(previous_handle, 1)) !=
                component.manifest_.semantic_contract_sha256_ ||
            sqlite3_column_int(previous_handle, 2) !=
                static_cast<int>(component.manifest_.model_role_)) {
            vqec_vision_ai_stor_apinv_rollback(database_);
            return {status_code::unsupported,
                "app update changes a component semantic contract"};
        }
    }
    sqlite_statement component_count;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "SELECT COUNT(*) FROM app_component_details WHERE app_id=?", component_count);
    auto* count_handle = component_count.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(
            count_handle, 1, _request.manifest_.app_id_) ||
        sqlite3_step(count_handle) != SQLITE_ROW ||
        sqlite3_column_int64(count_handle, 0) !=
            static_cast<sqlite3_int64>(_request.components_.size())) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return {status_code::unsupported,
            "app update changes the component closure"};
    }

    sqlite_statement release_previous_rollback;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "UPDATE components SET reference_count=reference_count-1 WHERE EXISTS("
        "SELECT 1 FROM app_rollback_components r WHERE r.app_id=? AND "
        "r.component_id=components.component_id AND "
        "r.component_version=components.component_version AND "
        "r.target_id=components.target_id AND "
        "r.artifact_sha256=components.artifact_sha256 AND "
        "r.semantic_contract_sha256=components.semantic_contract_sha256)",
        release_previous_rollback);
    auto* release_handle = release_previous_rollback.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(
            release_handle, 1, _request.manifest_.app_id_) ||
        (current = vqec_vision_ai_stor_apinv_step_done(
            database_, release_handle)).code_ != status_code::ok ||
        (current = vqec_vision_ai_stor_apinv_exec(database_,
            "DELETE FROM components WHERE reference_count=0")).code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    sqlite_statement clear_rollback_components;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "DELETE FROM app_rollback_components WHERE app_id=?",
        clear_rollback_components);
    auto* clear_handle = clear_rollback_components.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(
            clear_handle, 1, _request.manifest_.app_id_) ||
        (current = vqec_vision_ai_stor_apinv_step_done(
            database_, clear_handle)).code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    sqlite_statement clear_rollback_app;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "DELETE FROM app_rollback_applications WHERE app_id=?", clear_rollback_app);
    clear_handle = clear_rollback_app.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(
            clear_handle, 1, _request.manifest_.app_id_) ||
        (current = vqec_vision_ai_stor_apinv_step_done(
            database_, clear_handle)).code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    sqlite_statement save_app;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "INSERT INTO app_rollback_applications SELECT * FROM applications WHERE app_id=?",
        save_app);
    auto* save_handle = save_app.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(
            save_handle, 1, _request.manifest_.app_id_) ||
        (current = vqec_vision_ai_stor_apinv_step_done(database_, save_handle)).code_ !=
            status_code::ok || sqlite3_changes(database_) != 1) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return {status_code::io_error, "cannot retain rollback application generation"};
    }
    sqlite_statement save_components;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "INSERT INTO app_rollback_components SELECT * FROM app_component_details "
        "WHERE app_id=?", save_components);
    save_handle = save_components.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(
            save_handle, 1, _request.manifest_.app_id_) ||
        (current = vqec_vision_ai_stor_apinv_step_done(database_, save_handle)).code_ !=
            status_code::ok || sqlite3_changes(database_) !=
                static_cast<int>(_request.components_.size())) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return {status_code::io_error, "cannot retain rollback component generation"};
    }
    sqlite_statement clear_current_components;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "DELETE FROM app_components WHERE app_id=?", clear_current_components);
    clear_handle = clear_current_components.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(
            clear_handle, 1, _request.manifest_.app_id_) ||
        (current = vqec_vision_ai_stor_apinv_step_done(
            database_, clear_handle)).code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    sqlite_statement clear_current_details;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "DELETE FROM app_component_details WHERE app_id=?", clear_current_details);
    clear_handle = clear_current_details.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(
            clear_handle, 1, _request.manifest_.app_id_) ||
        (current = vqec_vision_ai_stor_apinv_step_done(
            database_, clear_handle)).code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    sqlite_statement update_app;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "UPDATE applications SET app_version=?,usecase_version=?,release_sequence=?,"
        "manifest_sha256=?,configuration_schema_id=?,configuration_revision=?,"
        "configuration_sha256=?,configuration_payload=?,requested_output_scopes=? "
        "WHERE app_id=?", update_app);
    auto* update_handle = update_app.vqec_vision_ai_stor_apinv_get();
    const auto requested_outputs = vqec_vision_ai_stor_apinv_join_scopes(
        _request.manifest_.requested_scopes_.outputs_);
    const bool update_bound = current.code_ == status_code::ok &&
        vqec_vision_ai_stor_apinv_bind_text(
            update_handle, 1, _request.manifest_.app_version_) &&
        vqec_vision_ai_stor_apinv_bind_text(
            update_handle, 2, _request.manifest_.usecase_version_) &&
        vqec_vision_ai_stor_apinv_bind_uint64(
            update_handle, 3, _request.manifest_.release_sequence_) &&
        vqec_vision_ai_stor_apinv_bind_text(
            update_handle, 4, _request.manifest_sha256_) &&
        vqec_vision_ai_stor_apinv_bind_text(
            update_handle, 5, _request.manifest_.configuration_schema_id_) &&
        vqec_vision_ai_stor_apinv_bind_uint64(
            update_handle, 6, _request.configuration_revision_) &&
        vqec_vision_ai_stor_apinv_bind_text(
            update_handle, 7, _request.configuration_sha256_) &&
        vqec_vision_ai_stor_apinv_bind_blob(
            update_handle, 8, _request.configuration_payload_) &&
        vqec_vision_ai_stor_apinv_bind_text(update_handle, 9, requested_outputs) &&
        vqec_vision_ai_stor_apinv_bind_text(
            update_handle, 10, _request.manifest_.app_id_);
    if (!update_bound ||
        (current = vqec_vision_ai_stor_apinv_step_done(database_, update_handle)).code_ !=
            status_code::ok || sqlite3_changes(database_) != 1) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return {status_code::io_error, "cannot publish updated application generation"};
    }
    for (const auto& installed_component : _request.components_) {
        const auto& component = installed_component.manifest_;
        sqlite_statement component_statement;
        current = vqec_vision_ai_stor_apinv_prepare(database_,
            "INSERT INTO components VALUES(?,?,?,?,?,1) ON CONFLICT(component_id,"
            "component_version,target_id,artifact_sha256,semantic_contract_sha256) "
            "DO UPDATE SET reference_count=reference_count+1", component_statement);
        auto* component_handle = component_statement.vqec_vision_ai_stor_apinv_get();
        const bool component_bound = current.code_ == status_code::ok &&
            vqec_vision_ai_stor_apinv_bind_text(
                component_handle, 1, component.component_id_) &&
            vqec_vision_ai_stor_apinv_bind_text(
                component_handle, 2, component.component_version_) &&
            vqec_vision_ai_stor_apinv_bind_text(
                component_handle, 3, component.target_id_) &&
            vqec_vision_ai_stor_apinv_bind_text(
                component_handle, 4, component.artifact_sha256_) &&
            vqec_vision_ai_stor_apinv_bind_text(
                component_handle, 5, component.semantic_contract_sha256_);
        if (!component_bound ||
            (current = vqec_vision_ai_stor_apinv_step_done(
                database_, component_handle)).code_ != status_code::ok) {
            vqec_vision_ai_stor_apinv_rollback(database_);
            return {status_code::io_error, "cannot retain updated component"};
        }
        sqlite_statement mapping;
        current = vqec_vision_ai_stor_apinv_prepare(database_,
            "INSERT INTO app_components VALUES(?,?,?,?,?,?)", mapping);
        auto* mapping_handle = mapping.vqec_vision_ai_stor_apinv_get();
        const bool mapping_bound = current.code_ == status_code::ok &&
            vqec_vision_ai_stor_apinv_bind_text(
                mapping_handle, 1, _request.manifest_.app_id_) &&
            vqec_vision_ai_stor_apinv_bind_text(
                mapping_handle, 2, component.component_id_) &&
            vqec_vision_ai_stor_apinv_bind_text(
                mapping_handle, 3, component.component_version_) &&
            vqec_vision_ai_stor_apinv_bind_text(
                mapping_handle, 4, component.target_id_) &&
            vqec_vision_ai_stor_apinv_bind_text(
                mapping_handle, 5, component.artifact_sha256_) &&
            vqec_vision_ai_stor_apinv_bind_text(
                mapping_handle, 6, component.semantic_contract_sha256_);
        if (!mapping_bound ||
            (current = vqec_vision_ai_stor_apinv_step_done(
                database_, mapping_handle)).code_ != status_code::ok) {
            vqec_vision_ai_stor_apinv_rollback(database_);
            return {status_code::io_error, "cannot map updated component"};
        }
        sqlite_statement details;
        current = vqec_vision_ai_stor_apinv_prepare(database_,
            "INSERT INTO app_component_details VALUES(?,?,?,?,?,?,?,?,?,?)", details);
        auto* detail_handle = details.vqec_vision_ai_stor_apinv_get();
        const bool detail_bound = current.code_ == status_code::ok &&
            vqec_vision_ai_stor_apinv_bind_text(
                detail_handle, 1, _request.manifest_.app_id_) &&
            vqec_vision_ai_stor_apinv_bind_text(
                detail_handle, 2, component.component_id_) &&
            vqec_vision_ai_stor_apinv_bind_text(
                detail_handle, 3, component.component_version_) &&
            sqlite3_bind_int(detail_handle, 4,
                static_cast<int>(component.type_)) == SQLITE_OK &&
            vqec_vision_ai_stor_apinv_bind_text(
                detail_handle, 5, component.target_id_) &&
            vqec_vision_ai_stor_apinv_bind_text(
                detail_handle, 6, component.artifact_sha256_) &&
            vqec_vision_ai_stor_apinv_bind_uint64(
                detail_handle, 7, component.artifact_bytes_) &&
            vqec_vision_ai_stor_apinv_bind_text(
                detail_handle, 8, component.semantic_contract_sha256_) &&
            sqlite3_bind_int(detail_handle, 9,
                static_cast<int>(component.model_role_)) == SQLITE_OK &&
            vqec_vision_ai_stor_apinv_bind_text(
                detail_handle, 10, installed_component.immutable_location_);
        if (!detail_bound ||
            (current = vqec_vision_ai_stor_apinv_step_done(
                database_, detail_handle)).code_ != status_code::ok) {
            vqec_vision_ai_stor_apinv_rollback(database_);
            return {status_code::io_error, "cannot describe updated component"};
        }
    }
    sqlite_statement update_sources;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "UPDATE app_sources SET supported=?,compatible=?,admitted=?,reason_code=? "
        "WHERE app_id=?",
        update_sources);
    auto* source_handle = update_sources.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        sqlite3_bind_int(source_handle, 1, _request.supported_ ? 1 : 0) != SQLITE_OK ||
        sqlite3_bind_int(source_handle, 2, _request.compatible_ ? 1 : 0) != SQLITE_OK ||
        sqlite3_bind_int(source_handle, 3, _request.admitted_ ? 1 : 0) != SQLITE_OK ||
        !vqec_vision_ai_stor_apinv_bind_text(source_handle, 4,
            vqec_vision_ai_stor_apinv_effective_reason(_request.supported_,
                _request.compatible_, _request.admitted_)) ||
        !vqec_vision_ai_stor_apinv_bind_text(
            source_handle, 5, _request.manifest_.app_id_) ||
        (current = vqec_vision_ai_stor_apinv_step_done(
            database_, source_handle)).code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
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

status sqlite_app_inventory::vqec_vision_ai_ports_apinv_rollback(
    const std::string& _app_id, std::uint64_t _expected_inventory_revision,
    runtime_control_snapshot& _snapshot) {
    if (database_ == nullptr ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _app_id, app_lifecycle_limits::g_max_identifier_bytes) ||
        _expected_inventory_revision == 0) {
        return {status_code::invalid_argument, "invalid app rollback request"};
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
    sqlite_statement state;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "SELECT a.configuration_revision,"
        "(SELECT COUNT(*) FROM app_sources s WHERE s.app_id=a.app_id AND s.desired=1),"
        "(SELECT COUNT(*) FROM app_rollback_applications r WHERE r.app_id=a.app_id) "
        "FROM applications a WHERE a.app_id=?", state);
    auto* state_handle = state.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(state_handle, 1, _app_id) ||
        sqlite3_step(state_handle) != SQLITE_ROW ||
        sqlite3_column_int64(state_handle, 0) <= 0 ||
        sqlite3_column_int64(state_handle, 1) != 0 ||
        sqlite3_column_int64(state_handle, 2) != 1) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return {status_code::invalid_state,
            "disabled app with one rollback generation is required"};
    }
    const auto next_configuration_revision = static_cast<std::uint64_t>(
        sqlite3_column_int64(state_handle, 0)) + 1U;
    sqlite_statement release_current;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "UPDATE components SET reference_count=reference_count-1 WHERE EXISTS("
        "SELECT 1 FROM app_components m WHERE m.app_id=? AND "
        "m.component_id=components.component_id AND "
        "m.component_version=components.component_version AND "
        "m.target_id=components.target_id AND "
        "m.artifact_sha256=components.artifact_sha256 AND "
        "m.semantic_contract_sha256=components.semantic_contract_sha256)",
        release_current);
    auto* release_handle = release_current.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(release_handle, 1, _app_id) ||
        (current = vqec_vision_ai_stor_apinv_step_done(
            database_, release_handle)).code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    for (const char* sql : {
             "DELETE FROM app_components WHERE app_id=?",
             "DELETE FROM app_component_details WHERE app_id=?"}) {
        sqlite_statement erase;
        current = vqec_vision_ai_stor_apinv_prepare(database_, sql, erase);
        auto* erase_handle = erase.vqec_vision_ai_stor_apinv_get();
        if (current.code_ != status_code::ok ||
            !vqec_vision_ai_stor_apinv_bind_text(erase_handle, 1, _app_id) ||
            (current = vqec_vision_ai_stor_apinv_step_done(
                database_, erase_handle)).code_ != status_code::ok) {
            vqec_vision_ai_stor_apinv_rollback(database_);
            return current;
        }
    }
    sqlite_statement restore_app;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "UPDATE applications SET app_version=(SELECT app_version FROM "
        "app_rollback_applications WHERE app_id=?),usecase_version=(SELECT "
        "usecase_version FROM app_rollback_applications WHERE app_id=?),"
        "release_sequence=(SELECT release_sequence FROM app_rollback_applications "
        "WHERE app_id=?),manifest_sha256=(SELECT manifest_sha256 FROM "
        "app_rollback_applications WHERE app_id=?),configuration_schema_id=(SELECT "
        "configuration_schema_id FROM app_rollback_applications WHERE app_id=?),"
        "configuration_revision=?,configuration_sha256=(SELECT configuration_sha256 "
        "FROM app_rollback_applications WHERE app_id=?),configuration_payload=(SELECT "
        "configuration_payload FROM app_rollback_applications WHERE app_id=?),"
        "requested_output_scopes=(SELECT requested_output_scopes FROM "
        "app_rollback_applications WHERE app_id=?) WHERE app_id=?", restore_app);
    auto* restore_handle = restore_app.vqec_vision_ai_stor_apinv_get();
    bool restore_bound = current.code_ == status_code::ok;
    for (int index = 1; index <= 5 && restore_bound; ++index) {
        restore_bound = vqec_vision_ai_stor_apinv_bind_text(
            restore_handle, index, _app_id);
    }
    restore_bound = restore_bound && vqec_vision_ai_stor_apinv_bind_uint64(
        restore_handle, 6, next_configuration_revision);
    for (int index = 7; index <= 10 && restore_bound; ++index) {
        restore_bound = vqec_vision_ai_stor_apinv_bind_text(
            restore_handle, index, _app_id);
    }
    if (!restore_bound ||
        (current = vqec_vision_ai_stor_apinv_step_done(
            database_, restore_handle)).code_ != status_code::ok ||
        sqlite3_changes(database_) != 1) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return {status_code::io_error, "cannot restore rollback application generation"};
    }
    sqlite_statement restore_mapping;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "INSERT INTO app_components(app_id,component_id,component_version,target_id,"
        "artifact_sha256,semantic_contract_sha256) SELECT app_id,component_id,"
        "component_version,target_id,artifact_sha256,semantic_contract_sha256 FROM "
        "app_rollback_components WHERE app_id=?", restore_mapping);
    auto* mapping_handle = restore_mapping.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(mapping_handle, 1, _app_id) ||
        (current = vqec_vision_ai_stor_apinv_step_done(
            database_, mapping_handle)).code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    sqlite_statement restore_details;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "INSERT INTO app_component_details SELECT * FROM app_rollback_components "
        "WHERE app_id=?", restore_details);
    auto* detail_handle = restore_details.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(detail_handle, 1, _app_id) ||
        (current = vqec_vision_ai_stor_apinv_step_done(
            database_, detail_handle)).code_ != status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    for (const char* sql : {
             "DELETE FROM app_rollback_components WHERE app_id=?",
             "DELETE FROM app_rollback_applications WHERE app_id=?"}) {
        sqlite_statement erase;
        current = vqec_vision_ai_stor_apinv_prepare(database_, sql, erase);
        auto* erase_handle = erase.vqec_vision_ai_stor_apinv_get();
        if (current.code_ != status_code::ok ||
            !vqec_vision_ai_stor_apinv_bind_text(erase_handle, 1, _app_id) ||
            (current = vqec_vision_ai_stor_apinv_step_done(
                database_, erase_handle)).code_ != status_code::ok) {
            vqec_vision_ai_stor_apinv_rollback(database_);
            return current;
        }
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
    bool installed = false;
    sqlite_statement requested_statement;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "SELECT requested_output_scopes FROM applications WHERE app_id=?",
        requested_statement);
    auto* requested_handle = requested_statement.vqec_vision_ai_stor_apinv_get();
    const bool requested_bound = current.code_ == status_code::ok &&
        vqec_vision_ai_stor_apinv_bind_text(requested_handle, 1, _update.app_id_);
    const int requested_result = requested_bound ? sqlite3_step(requested_handle) : SQLITE_ERROR;
    if (!requested_bound ||
        (requested_result != SQLITE_ROW && requested_result != SQLITE_DONE)) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current.code_ != status_code::ok ? current :
            status{status_code::io_error, "cannot inspect installed app scopes"};
    }
    installed = requested_result == SQLITE_ROW;
    if (installed &&
        ((current = vqec_vision_ai_stor_apinv_split_scopes(
              sqlite3_column_text(requested_handle, 0), requested)).code_ !=
             status_code::ok ||
         !vqec_vision_ai_stor_apinv_is_subset(_update.output_scopes_, requested))) {
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
    sqlite_statement grant_statement;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "INSERT INTO app_grants(app_id,source_id,entitled,"
        "entitlement_expires_utc_ns,output_scopes,reason_code) VALUES(?,?,?,?,?,?) "
        "ON CONFLICT(app_id,source_id) DO UPDATE SET entitled=excluded.entitled,"
        "entitlement_expires_utc_ns=excluded.entitlement_expires_utc_ns,"
        "output_scopes=excluded.output_scopes,reason_code=excluded.reason_code",
        grant_statement);
    auto* grant_handle = grant_statement.vqec_vision_ai_stor_apinv_get();
    const auto scopes = vqec_vision_ai_stor_apinv_join_scopes(_update.output_scopes_);
    const auto reason = _update.reason_code_.empty() ? "ok" : _update.reason_code_;
    const bool grant_bound = current.code_ == status_code::ok &&
        vqec_vision_ai_stor_apinv_bind_text(grant_handle, 1, _update.app_id_) &&
        vqec_vision_ai_stor_apinv_bind_text(grant_handle, 2, _update.source_id_) &&
        sqlite3_bind_int(grant_handle, 3, _update.entitled_ ? 1 : 0) == SQLITE_OK &&
        vqec_vision_ai_stor_apinv_bind_uint64(
            grant_handle, 4, _update.entitlement_expires_utc_ns_) &&
        vqec_vision_ai_stor_apinv_bind_text(grant_handle, 5, scopes) &&
        vqec_vision_ai_stor_apinv_bind_text(grant_handle, 6, reason);
    if (!grant_bound ||
        (current = vqec_vision_ai_stor_apinv_step_done(database_, grant_handle)).code_ !=
            status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return !grant_bound ? status{status_code::io_error,
            "app grant binding failed"} : current;
    }
    if (installed) {
        sqlite_statement statement;
        current = vqec_vision_ai_stor_apinv_prepare(database_,
            "UPDATE app_sources SET entitled=?,supported=?,compatible=?,admitted=?,"
            "entitlement_expires_utc_ns=?,output_scopes=?,reason_code=? "
            "WHERE app_id=? AND source_id=?", statement);
        auto* handle = statement.vqec_vision_ai_stor_apinv_get();
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
                status{status_code::invalid_state,
                    "installed app association is unavailable"} : current;
        }
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
    sqlite_statement decrement_rollback;
    current = vqec_vision_ai_stor_apinv_prepare(database_,
        "UPDATE components SET reference_count=reference_count-1 WHERE EXISTS("
        "SELECT 1 FROM app_rollback_components r WHERE r.app_id=? AND "
        "r.component_id=components.component_id AND "
        "r.component_version=components.component_version AND "
        "r.target_id=components.target_id AND "
        "r.artifact_sha256=components.artifact_sha256 AND "
        "r.semantic_contract_sha256=components.semantic_contract_sha256)",
        decrement_rollback);
    handle = decrement_rollback.vqec_vision_ai_stor_apinv_get();
    if (current.code_ != status_code::ok ||
        !vqec_vision_ai_stor_apinv_bind_text(handle, 1, _app_id) ||
        (current = vqec_vision_ai_stor_apinv_step_done(database_, handle)).code_ !=
            status_code::ok) {
        vqec_vision_ai_stor_apinv_rollback(database_);
        return current;
    }
    for (const char* sql : {
             "DELETE FROM app_rollback_components WHERE app_id=?",
             "DELETE FROM app_rollback_applications WHERE app_id=?"}) {
        sqlite_statement erase_rollback;
        current = vqec_vision_ai_stor_apinv_prepare(database_, sql, erase_rollback);
        handle = erase_rollback.vqec_vision_ai_stor_apinv_get();
        if (current.code_ != status_code::ok ||
            !vqec_vision_ai_stor_apinv_bind_text(handle, 1, _app_id) ||
            (current = vqec_vision_ai_stor_apinv_step_done(
                database_, handle)).code_ != status_code::ok) {
            vqec_vision_ai_stor_apinv_rollback(database_);
            return current;
        }
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
