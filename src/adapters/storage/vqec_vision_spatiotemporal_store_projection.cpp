#include "vqec_vision_spatiotemporal_store.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <sstream>
#include <utility>

#include "vqec/vision/ai/contracts/base/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

constexpr const char* g_spatiotemporal_projection_outbox_pending = "pending";
constexpr const char* g_spatiotemporal_episode_family = "episode";
constexpr const char* g_spatiotemporal_aggregate_family = "aggregate_contribution";

class projection_statement final {
public:
    projection_statement() = default;
    ~projection_statement() noexcept {
        if (statement_ != nullptr) {
            sqlite3_finalize(statement_);
        }
    }
    projection_statement(const projection_statement&) = delete;
    projection_statement& operator=(const projection_statement&) = delete;

    sqlite3_stmt** vqec_vision_ai_stor_stsql_put() noexcept {
        return &statement_;
    }

    sqlite3_stmt* vqec_vision_ai_stor_stsql_get() const noexcept {
        return statement_;
    }

private:
    sqlite3_stmt* statement_{nullptr};
};

status vqec_vision_ai_stor_stsql_make_projection_error(
    sqlite3* _database, const char* _operation) {
    std::ostringstream message;
    message << _operation << ": "
            << (_database == nullptr ? "sqlite database unavailable" : sqlite3_errmsg(_database));
    return {status_code::io_error, message.str()};
}

status vqec_vision_ai_stor_stsql_execute_projection(
    sqlite3* _database, const char* _sql) {
    char* error_message = nullptr;
    const auto result = sqlite3_exec(_database, _sql, nullptr, nullptr, &error_message);
    if (result == SQLITE_OK) {
        return {};
    }
    std::string message = error_message == nullptr
        ? "sqlite projection execution failed"
        : error_message;
    sqlite3_free(error_message);
    return {status_code::io_error, message};
}

status vqec_vision_ai_stor_stsql_prepare_projection(
    sqlite3* _database, const std::string& _sql, projection_statement& _statement) {
    if (sqlite3_prepare_v2(_database, _sql.c_str(), -1,
            _statement.vqec_vision_ai_stor_stsql_put(), nullptr) != SQLITE_OK) {
        return vqec_vision_ai_stor_stsql_make_projection_error(
            _database, "prepare metadata projection statement");
    }
    return {};
}

bool vqec_vision_ai_stor_stsql_bind_projection_text(
    sqlite3_stmt* _statement, int _index, const std::string& _value) {
    return sqlite3_bind_text(_statement, _index, _value.c_str(),
               static_cast<int>(_value.size()), SQLITE_TRANSIENT) == SQLITE_OK;
}

bool vqec_vision_ai_stor_stsql_bind_projection_blob(
    sqlite3_stmt* _statement, int _index, const std::vector<std::uint8_t>& _value) {
    return sqlite3_bind_blob(_statement, _index, _value.data(),
               static_cast<int>(_value.size()), SQLITE_TRANSIENT) == SQLITE_OK;
}

std::string vqec_vision_ai_stor_stsql_read_projection_text(
    sqlite3_stmt* _statement, int _column) {
    const auto* text = sqlite3_column_text(_statement, _column);
    return text == nullptr ? std::string{} :
                             std::string(reinterpret_cast<const char*>(text));
}

std::vector<std::uint8_t> vqec_vision_ai_stor_stsql_read_projection_blob(
    sqlite3_stmt* _statement, int _column) {
    const auto* data = static_cast<const std::uint8_t*>(
        sqlite3_column_blob(_statement, _column));
    const auto size = sqlite3_column_bytes(_statement, _column);
    if (data == nullptr || size <= 0) {
        return {};
    }
    return {data, data + size};
}

void vqec_vision_ai_stor_stsql_append_projection_u64(
    std::vector<std::uint8_t>& _output, std::uint64_t _value) {
    for (std::size_t index = 0U; index < sizeof(_value); ++index) {
        _output.push_back(static_cast<std::uint8_t>(_value >> (index * 8U)));
    }
}

void vqec_vision_ai_stor_stsql_append_projection_i64(
    std::vector<std::uint8_t>& _output, std::int64_t _value) {
    std::uint64_t bits = 0U;
    static_assert(sizeof(bits) == sizeof(_value), "signed integer size mismatch");
    std::memcpy(&bits, &_value, sizeof(bits));
    vqec_vision_ai_stor_stsql_append_projection_u64(_output, bits);
}

void vqec_vision_ai_stor_stsql_append_projection_string(
    std::vector<std::uint8_t>& _output, const std::string& _value) {
    vqec_vision_ai_stor_stsql_append_projection_u64(_output, _value.size());
    _output.insert(_output.end(), _value.begin(), _value.end());
}

std::vector<std::uint8_t> vqec_vision_ai_stor_stsql_encode_dimensions(
    const std::vector<spatiotemporal_dimension>& _dimensions) {
    std::vector<std::uint8_t> encoded;
    encoded.reserve(16U + _dimensions.size() * 32U);
    vqec_vision_ai_stor_stsql_append_projection_u64(encoded, _dimensions.size());
    for (const auto& dimension : _dimensions) {
        vqec_vision_ai_stor_stsql_append_projection_string(encoded, dimension.key_);
        vqec_vision_ai_stor_stsql_append_projection_string(encoded, dimension.value_);
    }
    return encoded;
}

std::vector<std::uint8_t> vqec_vision_ai_stor_stsql_encode_references(
    const std::vector<std::string>& _references) {
    std::vector<std::uint8_t> encoded;
    encoded.reserve(16U + _references.size() * 24U);
    vqec_vision_ai_stor_stsql_append_projection_u64(encoded, _references.size());
    for (const auto& reference : _references) {
        vqec_vision_ai_stor_stsql_append_projection_string(encoded, reference);
    }
    return encoded;
}

bool vqec_vision_ai_stor_stsql_read_projection_u64(
    const std::vector<std::uint8_t>& _input, std::size_t& _offset, std::uint64_t& _value) {
    if (_offset > _input.size() || sizeof(_value) > _input.size() - _offset) {
        return false;
    }
    _value = 0U;
    for (std::size_t index = 0U; index < sizeof(_value); ++index) {
        _value |= static_cast<std::uint64_t>(_input[_offset++]) << (index * 8U);
    }
    return true;
}

bool vqec_vision_ai_stor_stsql_read_projection_string(
    const std::vector<std::uint8_t>& _input, std::size_t& _offset, std::string& _value) {
    std::uint64_t size = 0U;
    if (!vqec_vision_ai_stor_stsql_read_projection_u64(_input, _offset, size) ||
        size > g_spatiotemporal_max_identifier_bytes ||
        size > _input.size() - std::min(_input.size(), _offset)) {
        return false;
    }
    _value.assign(reinterpret_cast<const char*>(_input.data() + _offset),
        static_cast<std::size_t>(size));
    _offset += static_cast<std::size_t>(size);
    return true;
}

bool vqec_vision_ai_stor_stsql_decode_dimensions(
    const std::vector<std::uint8_t>& _encoded,
    std::vector<spatiotemporal_dimension>& _dimensions) {
    std::size_t offset = 0U;
    std::uint64_t count = 0U;
    if (!vqec_vision_ai_stor_stsql_read_projection_u64(_encoded, offset, count) ||
        count > g_spatiotemporal_max_episode_claims) {
        return false;
    }
    _dimensions.clear();
    _dimensions.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t index = 0U; index < count; ++index) {
        spatiotemporal_dimension dimension;
        if (!vqec_vision_ai_stor_stsql_read_projection_string(
                _encoded, offset, dimension.key_) ||
            !vqec_vision_ai_stor_stsql_read_projection_string(
                _encoded, offset, dimension.value_)) {
            return false;
        }
        _dimensions.push_back(std::move(dimension));
    }
    return offset == _encoded.size();
}

bool vqec_vision_ai_stor_stsql_decode_references(
    const std::vector<std::uint8_t>& _encoded, std::vector<std::string>& _references) {
    std::size_t offset = 0U;
    std::uint64_t count = 0U;
    if (!vqec_vision_ai_stor_stsql_read_projection_u64(_encoded, offset, count) ||
        count > g_spatiotemporal_max_episode_references) {
        return false;
    }
    _references.clear();
    _references.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t index = 0U; index < count; ++index) {
        std::string reference;
        if (!vqec_vision_ai_stor_stsql_read_projection_string(_encoded, offset, reference)) {
            return false;
        }
        _references.push_back(std::move(reference));
    }
    return offset == _encoded.size();
}

std::vector<std::uint8_t> vqec_vision_ai_stor_stsql_encode_episode_payload(
    const event_episode_revision& _episode,
    const std::vector<std::uint8_t>& _claims,
    const std::vector<std::uint8_t>& _references) {
    std::vector<std::uint8_t> payload;
    vqec_vision_ai_stor_stsql_append_projection_string(payload, _episode.episode_id_);
    vqec_vision_ai_stor_stsql_append_projection_u64(payload, _episode.revision_);
    vqec_vision_ai_stor_stsql_append_projection_u64(payload, _episode.supersedes_revision_);
    vqec_vision_ai_stor_stsql_append_projection_string(payload, _episode.source_id_);
    vqec_vision_ai_stor_stsql_append_projection_string(payload, _episode.semantic_type_);
    vqec_vision_ai_stor_stsql_append_projection_string(payload, _episode.subject_ref_);
    vqec_vision_ai_stor_stsql_append_projection_string(payload, _episode.scene_revision_);
    vqec_vision_ai_stor_stsql_append_projection_string(payload, _episode.rule_revision_);
    vqec_vision_ai_stor_stsql_append_projection_i64(payload, _episode.begin_ns_);
    vqec_vision_ai_stor_stsql_append_projection_i64(payload, _episode.end_ns_);
    vqec_vision_ai_stor_stsql_append_projection_i64(payload, _episode.recorded_ns_);
    vqec_vision_ai_stor_stsql_append_projection_u64(
        payload, static_cast<std::uint64_t>(_episode.lifecycle_));
    vqec_vision_ai_stor_stsql_append_projection_u64(payload, _episode.severity_ppm_);
    vqec_vision_ai_stor_stsql_append_projection_u64(
        payload, _episode.required_access_domain_mask_);
    payload.insert(payload.end(), _claims.begin(), _claims.end());
    payload.insert(payload.end(), _references.begin(), _references.end());
    return payload;
}

std::vector<std::uint8_t> vqec_vision_ai_stor_stsql_encode_contribution_payload(
    const aggregate_contribution_revision& _contribution,
    const std::vector<std::uint8_t>& _dimensions) {
    std::vector<std::uint8_t> payload;
    vqec_vision_ai_stor_stsql_append_projection_string(
        payload, _contribution.contribution_id_);
    vqec_vision_ai_stor_stsql_append_projection_u64(payload, _contribution.revision_);
    vqec_vision_ai_stor_stsql_append_projection_u64(
        payload, _contribution.supersedes_revision_);
    vqec_vision_ai_stor_stsql_append_projection_string(payload, _contribution.episode_id_);
    vqec_vision_ai_stor_stsql_append_projection_string(payload, _contribution.source_id_);
    vqec_vision_ai_stor_stsql_append_projection_string(
        payload, _contribution.aggregate_definition_id_);
    vqec_vision_ai_stor_stsql_append_projection_string(payload, _contribution.scene_revision_);
    vqec_vision_ai_stor_stsql_append_projection_string(
        payload, _contribution.definition_revision_);
    vqec_vision_ai_stor_stsql_append_projection_i64(payload, _contribution.bucket_begin_ns_);
    vqec_vision_ai_stor_stsql_append_projection_i64(payload, _contribution.bucket_end_ns_);
    vqec_vision_ai_stor_stsql_append_projection_i64(payload, _contribution.recorded_ns_);
    vqec_vision_ai_stor_stsql_append_projection_u64(
        payload, static_cast<std::uint64_t>(_contribution.operation_));
    vqec_vision_ai_stor_stsql_append_projection_i64(
        payload, _contribution.numerator_microunits_);
    vqec_vision_ai_stor_stsql_append_projection_i64(
        payload, _contribution.denominator_microunits_);
    vqec_vision_ai_stor_stsql_append_projection_u64(
        payload, _contribution.observed_duration_ns_);
    vqec_vision_ai_stor_stsql_append_projection_u64(
        payload, _contribution.expected_duration_ns_);
    vqec_vision_ai_stor_stsql_append_projection_u64(
        payload, _contribution.required_access_domain_mask_);
    payload.insert(payload.end(), _dimensions.begin(), _dimensions.end());
    return payload;
}

bool vqec_vision_ai_stor_stsql_validate_projection_sinks(
    const std::vector<std::string>& _sinks) {
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

status vqec_vision_ai_stor_stsql_begin_projection(
    sqlite3* _catalog, std::uint64_t& _sequence) {
    status result;
    if (sqlite3_get_autocommit(_catalog) != 0) {
        result = vqec_vision_ai_stor_stsql_execute_projection(
            _catalog, "BEGIN IMMEDIATE;");
        if (result.code_ != status_code::ok) {
            return result;
        }
    }
    projection_statement statement;
    result = vqec_vision_ai_stor_stsql_prepare_projection(_catalog,
        "UPDATE store_state SET value=value+1 WHERE key='global_sequence' RETURNING value;",
        statement);
    auto* update = statement.vqec_vision_ai_stor_stsql_get();
    if (result.code_ != status_code::ok || sqlite3_step(update) != SQLITE_ROW) {
        (void)vqec_vision_ai_stor_stsql_execute_projection(_catalog, "ROLLBACK;");
        return result.code_ == status_code::ok
            ? vqec_vision_ai_stor_stsql_make_projection_error(
                  _catalog, "reserve projection sequence")
            : result;
    }
    const auto value = sqlite3_column_int64(update, 0);
    if (value <= 0 || sqlite3_step(update) != SQLITE_DONE) {
        (void)vqec_vision_ai_stor_stsql_execute_projection(_catalog, "ROLLBACK;");
        return {status_code::resource_exhausted, "metadata projection sequence overflow"};
    }
    _sequence = static_cast<std::uint64_t>(value);
    return {};
}

status vqec_vision_ai_stor_stsql_insert_projection_outbox(
    sqlite3* _catalog, const std::vector<std::string>& _sinks,
    const char* _family, const std::string& _record_id, std::uint64_t _revision) {
    for (const auto& sink : _sinks) {
        projection_statement statement;
        auto result = vqec_vision_ai_stor_stsql_prepare_projection(_catalog,
            "INSERT INTO metadata_outbox(sink_id,record_family,record_id,revision,state,"
            "attempt_revision) VALUES(?1,?2,?3,?4,?5,0);", statement);
        auto* insert = statement.vqec_vision_ai_stor_stsql_get();
        if (result.code_ != status_code::ok ||
            !vqec_vision_ai_stor_stsql_bind_projection_text(insert, 1, sink) ||
            !vqec_vision_ai_stor_stsql_bind_projection_text(insert, 2, _family) ||
            !vqec_vision_ai_stor_stsql_bind_projection_text(insert, 3, _record_id) ||
            sqlite3_bind_int64(insert, 4, static_cast<sqlite3_int64>(_revision)) != SQLITE_OK ||
            !vqec_vision_ai_stor_stsql_bind_projection_text(
                insert, 5, g_spatiotemporal_projection_outbox_pending) ||
            sqlite3_step(insert) != SQLITE_DONE) {
            return result.code_ == status_code::ok
                ? vqec_vision_ai_stor_stsql_make_projection_error(
                      _catalog, "insert metadata projection outbox")
                : result;
        }
    }
    return {};
}

status vqec_vision_ai_stor_stsql_check_projection_revision(
    sqlite3* _catalog, const char* _table, const char* _id_column,
    const std::string& _record_id, std::uint64_t _revision,
    std::uint64_t _supersedes_revision, const std::vector<std::uint8_t>& _payload,
    bool& _is_idempotent) {
    projection_statement statement;
    const std::string sql = std::string("SELECT revision,canonical_payload FROM ") + _table +
        " WHERE " + _id_column + "=?1 ORDER BY revision DESC LIMIT 1;";
    auto result = vqec_vision_ai_stor_stsql_prepare_projection(_catalog, sql, statement);
    auto* query = statement.vqec_vision_ai_stor_stsql_get();
    if (result.code_ != status_code::ok ||
        !vqec_vision_ai_stor_stsql_bind_projection_text(query, 1, _record_id)) {
        return result.code_ == status_code::ok
            ? vqec_vision_ai_stor_stsql_make_projection_error(
                  _catalog, "bind metadata projection revision")
            : result;
    }
    const auto step = sqlite3_step(query);
    const auto latest_revision = step == SQLITE_ROW
        ? static_cast<std::uint64_t>(sqlite3_column_int64(query, 0))
        : 0U;
    if (latest_revision >= _revision) {
        _is_idempotent = latest_revision == _revision &&
            vqec_vision_ai_stor_stsql_read_projection_blob(query, 1) == _payload;
        return _is_idempotent
            ? status{}
            : status{status_code::invalid_argument,
                  "metadata projection revision is stale or conflicts"};
    }
    if (latest_revision != _supersedes_revision) {
        return {status_code::invalid_argument,
            "metadata projection supersedes revision is not current"};
    }
    _is_idempotent = false;
    return {};
}

std::uint64_t vqec_vision_ai_stor_stsql_get_projection_now_ns() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

bool vqec_vision_ai_stor_stsql_checked_add_i64(
    std::int64_t _left, std::int64_t _right, std::int64_t& _result) {
    if ((_right > 0 && _left > std::numeric_limits<std::int64_t>::max() - _right) ||
        (_right < 0 && _left < std::numeric_limits<std::int64_t>::min() - _right)) {
        return false;
    }
    _result = _left + _right;
    return true;
}

bool vqec_vision_ai_stor_stsql_checked_add_u64(
    std::uint64_t _left, std::uint64_t _right, std::uint64_t& _result) {
    if (_left > std::numeric_limits<std::uint64_t>::max() - _right) {
        return false;
    }
    _result = _left + _right;
    return true;
}

status vqec_vision_ai_stor_stsql_load_contribution_revision(
    sqlite3* _catalog, const std::string& _contribution_id, std::uint64_t _revision,
    aggregate_contribution_revision& _contribution,
    std::vector<std::uint8_t>& _dimensions) {
    projection_statement statement;
    auto result = vqec_vision_ai_stor_stsql_prepare_projection(_catalog,
        "SELECT episode_id,source_id,aggregate_definition_id,scene_revision,"
        "definition_revision,bucket_begin_ns,bucket_end_ns,recorded_ns,operation,"
        "numerator_microunits,denominator_microunits,observed_duration_ns,"
        "expected_duration_ns,required_access_mask,dimensions FROM "
        "aggregate_contribution_revisions WHERE contribution_id=?1 AND revision=?2;",
        statement);
    auto* query = statement.vqec_vision_ai_stor_stsql_get();
    if (result.code_ != status_code::ok ||
        !vqec_vision_ai_stor_stsql_bind_projection_text(query, 1, _contribution_id) ||
        sqlite3_bind_int64(query, 2, static_cast<sqlite3_int64>(_revision)) != SQLITE_OK ||
        sqlite3_step(query) != SQLITE_ROW) {
        return result.code_ == status_code::ok
            ? status{status_code::io_error,
                  "aggregate superseded contribution is unavailable"}
            : result;
    }
    _contribution = {};
    _contribution.contribution_id_ = _contribution_id;
    _contribution.revision_ = _revision;
    _contribution.episode_id_ = vqec_vision_ai_stor_stsql_read_projection_text(query, 0);
    _contribution.source_id_ = vqec_vision_ai_stor_stsql_read_projection_text(query, 1);
    _contribution.aggregate_definition_id_ =
        vqec_vision_ai_stor_stsql_read_projection_text(query, 2);
    _contribution.scene_revision_ = vqec_vision_ai_stor_stsql_read_projection_text(query, 3);
    _contribution.definition_revision_ =
        vqec_vision_ai_stor_stsql_read_projection_text(query, 4);
    _contribution.bucket_begin_ns_ = sqlite3_column_int64(query, 5);
    _contribution.bucket_end_ns_ = sqlite3_column_int64(query, 6);
    _contribution.recorded_ns_ = sqlite3_column_int64(query, 7);
    _contribution.operation_ = static_cast<aggregate_contribution_operation>(
        sqlite3_column_int(query, 8));
    _contribution.numerator_microunits_ = sqlite3_column_int64(query, 9);
    _contribution.denominator_microunits_ = sqlite3_column_int64(query, 10);
    _contribution.observed_duration_ns_ =
        static_cast<std::uint64_t>(sqlite3_column_int64(query, 11));
    _contribution.expected_duration_ns_ =
        static_cast<std::uint64_t>(sqlite3_column_int64(query, 12));
    _contribution.required_access_domain_mask_ =
        static_cast<std::uint32_t>(sqlite3_column_int64(query, 13));
    _dimensions = vqec_vision_ai_stor_stsql_read_projection_blob(query, 14);
    return {};
}

status vqec_vision_ai_stor_stsql_apply_rollup_delta(
    sqlite3* _catalog, const aggregate_contribution_revision& _contribution,
    const std::vector<std::uint8_t>& _dimensions, std::int32_t _sign,
    std::uint64_t _sequence) {
    if (_sign != 1 && _sign != -1) {
        return {status_code::invalid_argument, "aggregate rollup delta sign is invalid"};
    }
    projection_statement select_statement;
    auto result = vqec_vision_ai_stor_stsql_prepare_projection(_catalog,
        "SELECT numerator_microunits,denominator_microunits,observed_duration_ns,"
        "expected_duration_ns,contribution_count FROM aggregate_rollups WHERE "
        "aggregate_definition_id=?1 AND source_id=?2 AND scene_revision=?3 AND "
        "definition_revision=?4 AND bucket_begin_ns=?5 AND bucket_end_ns=?6 AND "
        "required_access_mask=?7 AND dimensions=?8;", select_statement);
    auto* select = select_statement.vqec_vision_ai_stor_stsql_get();
    const bool select_bound = result.code_ == status_code::ok &&
        vqec_vision_ai_stor_stsql_bind_projection_text(
            select, 1, _contribution.aggregate_definition_id_) &&
        vqec_vision_ai_stor_stsql_bind_projection_text(select, 2, _contribution.source_id_) &&
        vqec_vision_ai_stor_stsql_bind_projection_text(select, 3, _contribution.scene_revision_) &&
        vqec_vision_ai_stor_stsql_bind_projection_text(
            select, 4, _contribution.definition_revision_) &&
        sqlite3_bind_int64(select, 5, _contribution.bucket_begin_ns_) == SQLITE_OK &&
        sqlite3_bind_int64(select, 6, _contribution.bucket_end_ns_) == SQLITE_OK &&
        sqlite3_bind_int64(select, 7, _contribution.required_access_domain_mask_) == SQLITE_OK &&
        vqec_vision_ai_stor_stsql_bind_projection_blob(select, 8, _dimensions);
    if (!select_bound) {
        return result.code_ == status_code::ok
            ? vqec_vision_ai_stor_stsql_make_projection_error(
                  _catalog, "bind aggregate rollup lookup")
            : result;
    }
    const auto step = sqlite3_step(select);
    if (step != SQLITE_ROW) {
        if (_sign < 0) {
            return {status_code::io_error, "aggregate rollup source bucket is unavailable"};
        }
        projection_statement insert_statement;
        result = vqec_vision_ai_stor_stsql_prepare_projection(_catalog,
            "INSERT INTO aggregate_rollups(aggregate_definition_id,source_id,"
            "scene_revision,definition_revision,bucket_begin_ns,bucket_end_ns,"
            "required_access_mask,dimensions,numerator_microunits,denominator_microunits,"
            "observed_duration_ns,expected_duration_ns,contribution_count,updated_sequence) "
            "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,1,?13);",
            insert_statement);
        auto* insert = insert_statement.vqec_vision_ai_stor_stsql_get();
        const bool insert_bound = result.code_ == status_code::ok &&
            vqec_vision_ai_stor_stsql_bind_projection_text(
                insert, 1, _contribution.aggregate_definition_id_) &&
            vqec_vision_ai_stor_stsql_bind_projection_text(
                insert, 2, _contribution.source_id_) &&
            vqec_vision_ai_stor_stsql_bind_projection_text(
                insert, 3, _contribution.scene_revision_) &&
            vqec_vision_ai_stor_stsql_bind_projection_text(
                insert, 4, _contribution.definition_revision_) &&
            sqlite3_bind_int64(insert, 5, _contribution.bucket_begin_ns_) == SQLITE_OK &&
            sqlite3_bind_int64(insert, 6, _contribution.bucket_end_ns_) == SQLITE_OK &&
            sqlite3_bind_int64(insert, 7, _contribution.required_access_domain_mask_) == SQLITE_OK &&
            vqec_vision_ai_stor_stsql_bind_projection_blob(insert, 8, _dimensions) &&
            sqlite3_bind_int64(insert, 9, _contribution.numerator_microunits_) == SQLITE_OK &&
            sqlite3_bind_int64(insert, 10, _contribution.denominator_microunits_) == SQLITE_OK &&
            sqlite3_bind_int64(insert, 11, static_cast<sqlite3_int64>(
                _contribution.observed_duration_ns_)) == SQLITE_OK &&
            sqlite3_bind_int64(insert, 12, static_cast<sqlite3_int64>(
                _contribution.expected_duration_ns_)) == SQLITE_OK &&
            sqlite3_bind_int64(insert, 13, static_cast<sqlite3_int64>(_sequence)) == SQLITE_OK;
        if (!insert_bound || sqlite3_step(insert) != SQLITE_DONE) {
            return result.code_ == status_code::ok
                ? vqec_vision_ai_stor_stsql_make_projection_error(
                      _catalog, "insert aggregate rollup")
                : result;
        }
        return {};
    }
    const auto old_count = sqlite3_column_int64(select, 4);
    if (old_count <= 0 || (_sign < 0 && old_count == 1)) {
        if (_sign > 0 && old_count > 0) {
            // Continue to the checked update below.
        } else if (_sign < 0 && old_count == 1) {
            projection_statement delete_statement;
            result = vqec_vision_ai_stor_stsql_prepare_projection(_catalog,
                "DELETE FROM aggregate_rollups WHERE aggregate_definition_id=?1 AND "
                "source_id=?2 AND scene_revision=?3 AND definition_revision=?4 AND "
                "bucket_begin_ns=?5 AND bucket_end_ns=?6 AND required_access_mask=?7 AND "
                "dimensions=?8;", delete_statement);
            auto* remove = delete_statement.vqec_vision_ai_stor_stsql_get();
            const bool delete_bound = result.code_ == status_code::ok &&
                vqec_vision_ai_stor_stsql_bind_projection_text(
                    remove, 1, _contribution.aggregate_definition_id_) &&
                vqec_vision_ai_stor_stsql_bind_projection_text(
                    remove, 2, _contribution.source_id_) &&
                vqec_vision_ai_stor_stsql_bind_projection_text(
                    remove, 3, _contribution.scene_revision_) &&
                vqec_vision_ai_stor_stsql_bind_projection_text(
                    remove, 4, _contribution.definition_revision_) &&
                sqlite3_bind_int64(remove, 5, _contribution.bucket_begin_ns_) == SQLITE_OK &&
                sqlite3_bind_int64(remove, 6, _contribution.bucket_end_ns_) == SQLITE_OK &&
                sqlite3_bind_int64(remove, 7, _contribution.required_access_domain_mask_) == SQLITE_OK &&
                vqec_vision_ai_stor_stsql_bind_projection_blob(remove, 8, _dimensions);
            if (!delete_bound || sqlite3_step(remove) != SQLITE_DONE) {
                return result.code_ == status_code::ok
                    ? vqec_vision_ai_stor_stsql_make_projection_error(
                          _catalog, "delete empty aggregate rollup")
                    : result;
            }
            return {};
        } else {
            return {status_code::io_error, "aggregate rollup count is corrupt"};
        }
    }
    const auto sign_i64 = static_cast<std::int64_t>(_sign);
    if (_sign < 0 && (_contribution.numerator_microunits_ ==
            std::numeric_limits<std::int64_t>::min() ||
            _contribution.denominator_microunits_ ==
                std::numeric_limits<std::int64_t>::min())) {
        return {status_code::resource_exhausted, "aggregate rollup delta overflows"};
    }
    std::int64_t numerator = 0;
    std::int64_t denominator = 0;
    std::int64_t observed = 0;
    std::int64_t expected = 0;
    const auto observed_delta = static_cast<std::int64_t>(
        _contribution.observed_duration_ns_) * sign_i64;
    const auto expected_delta = static_cast<std::int64_t>(
        _contribution.expected_duration_ns_) * sign_i64;
    if (!vqec_vision_ai_stor_stsql_checked_add_i64(sqlite3_column_int64(select, 0),
            _contribution.numerator_microunits_ * sign_i64, numerator) ||
        !vqec_vision_ai_stor_stsql_checked_add_i64(sqlite3_column_int64(select, 1),
            _contribution.denominator_microunits_ * sign_i64, denominator) ||
        !vqec_vision_ai_stor_stsql_checked_add_i64(
            sqlite3_column_int64(select, 2), observed_delta, observed) ||
        !vqec_vision_ai_stor_stsql_checked_add_i64(
            sqlite3_column_int64(select, 3), expected_delta, expected)) {
        return {status_code::resource_exhausted, "aggregate rollup value overflows"};
    }
    projection_statement update_statement;
    result = vqec_vision_ai_stor_stsql_prepare_projection(_catalog,
        "UPDATE aggregate_rollups SET numerator_microunits=?1,denominator_microunits=?2,"
        "observed_duration_ns=?3,expected_duration_ns=?4,contribution_count=?5,"
        "updated_sequence=?6 WHERE aggregate_definition_id=?7 AND source_id=?8 AND "
        "scene_revision=?9 AND definition_revision=?10 AND bucket_begin_ns=?11 AND "
        "bucket_end_ns=?12 AND required_access_mask=?13 AND dimensions=?14;",
        update_statement);
    auto* update = update_statement.vqec_vision_ai_stor_stsql_get();
    const bool update_bound = result.code_ == status_code::ok &&
        sqlite3_bind_int64(update, 1, numerator) == SQLITE_OK &&
        sqlite3_bind_int64(update, 2, denominator) == SQLITE_OK &&
        sqlite3_bind_int64(update, 3, observed) == SQLITE_OK &&
        sqlite3_bind_int64(update, 4, expected) == SQLITE_OK &&
        sqlite3_bind_int64(update, 5, old_count + _sign) == SQLITE_OK &&
        sqlite3_bind_int64(update, 6, static_cast<sqlite3_int64>(_sequence)) == SQLITE_OK &&
        vqec_vision_ai_stor_stsql_bind_projection_text(
            update, 7, _contribution.aggregate_definition_id_) &&
        vqec_vision_ai_stor_stsql_bind_projection_text(update, 8, _contribution.source_id_) &&
        vqec_vision_ai_stor_stsql_bind_projection_text(update, 9, _contribution.scene_revision_) &&
        vqec_vision_ai_stor_stsql_bind_projection_text(
            update, 10, _contribution.definition_revision_) &&
        sqlite3_bind_int64(update, 11, _contribution.bucket_begin_ns_) == SQLITE_OK &&
        sqlite3_bind_int64(update, 12, _contribution.bucket_end_ns_) == SQLITE_OK &&
        sqlite3_bind_int64(update, 13, _contribution.required_access_domain_mask_) == SQLITE_OK &&
        vqec_vision_ai_stor_stsql_bind_projection_blob(update, 14, _dimensions);
    if (!update_bound || sqlite3_step(update) != SQLITE_DONE) {
        return result.code_ == status_code::ok
            ? vqec_vision_ai_stor_stsql_make_projection_error(
                  _catalog, "update aggregate rollup")
            : result;
    }
    return {};
}

status vqec_vision_ai_stor_stsql_query_materialized_rollups(
    sqlite3* _catalog, const spatiotemporal_query& _query,
    std::uint64_t _snapshot_sequence, std::size_t _maximum_store_results,
    std::atomic<bool>& _cancel_requested, spatiotemporal_query_page& _page) {
    std::ostringstream sql;
    sql << "SELECT aggregate_definition_id,source_id,scene_revision,definition_revision,"
           "bucket_begin_ns,bucket_end_ns,numerator_microunits,denominator_microunits,"
           "observed_duration_ns,expected_duration_ns,contribution_count,dimensions,"
           "updated_sequence FROM aggregate_rollups WHERE bucket_begin_ns<?1 AND "
           "bucket_end_ns>?2 AND updated_sequence<=?3 AND "
           "(required_access_mask & ?4)=required_access_mask AND source_id IN (";
    int bind_index = 5;
    for (std::size_t index = 0U; index < _query.source_ids_.size(); ++index) {
        if (index != 0U) {
            sql << ',';
        }
        sql << '?' << bind_index++;
    }
    sql << ')';
    const auto semantic_index = _query.semantic_type_.empty() ? 0 : bind_index++;
    if (semantic_index != 0) {
        sql << " AND aggregate_definition_id=?" << semantic_index;
    }
    sql << " ORDER BY aggregate_definition_id,source_id,scene_revision,definition_revision,"
           "bucket_begin_ns,bucket_end_ns,dimensions;";
    projection_statement statement;
    auto result = vqec_vision_ai_stor_stsql_prepare_projection(
        _catalog, sql.str(), statement);
    auto* query = statement.vqec_vision_ai_stor_stsql_get();
    if (result.code_ != status_code::ok ||
        sqlite3_bind_int64(query, 1, _query.end_ns_) != SQLITE_OK ||
        sqlite3_bind_int64(query, 2, _query.begin_ns_) != SQLITE_OK ||
        sqlite3_bind_int64(query, 3, static_cast<sqlite3_int64>(_snapshot_sequence)) != SQLITE_OK ||
        sqlite3_bind_int64(query, 4, _query.allowed_access_domain_mask_) != SQLITE_OK) {
        return result.code_ == status_code::ok
            ? vqec_vision_ai_stor_stsql_make_projection_error(
                  _catalog, "bind materialized rollup query")
            : result;
    }
    int value_index = 5;
    for (const auto& source : _query.source_ids_) {
        if (!vqec_vision_ai_stor_stsql_bind_projection_text(query, value_index++, source)) {
            return vqec_vision_ai_stor_stsql_make_projection_error(
                _catalog, "bind materialized rollup source");
        }
    }
    if (semantic_index != 0 &&
        !vqec_vision_ai_stor_stsql_bind_projection_text(
            query, value_index, _query.semantic_type_)) {
        return vqec_vision_ai_stor_stsql_make_projection_error(
            _catalog, "bind materialized rollup semantic type");
    }
    const auto result_limit = std::min(
        _query.budget_.maximum_results_, _maximum_store_results);
    while (sqlite3_step(query) == SQLITE_ROW) {
        if (_cancel_requested.exchange(false, std::memory_order_acq_rel)) {
            _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
            return {status_code::timeout, "metadata projection query was cancelled"};
        }
        if (vqec_vision_ai_stor_stsql_get_projection_now_ns() >=
                _query.budget_.deadline_ns_) {
            _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
            return {};
        }
        if (_page.aggregate_buckets_.size() == result_limit) {
            _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
            return {};
        }
        const auto dimensions =
            vqec_vision_ai_stor_stsql_read_projection_blob(query, 11);
        const auto row_bytes = static_cast<std::uint64_t>(dimensions.size() + 128U);
        if (row_bytes > _query.budget_.maximum_scan_bytes_ -
                std::min(_query.budget_.maximum_scan_bytes_, _page.scanned_bytes_) ||
            row_bytes > _query.budget_.maximum_result_bytes_ -
                std::min(_query.budget_.maximum_result_bytes_, _page.result_bytes_)) {
            _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
            return {};
        }
        aggregate_bucket bucket;
        bucket.aggregate_definition_id_ =
            vqec_vision_ai_stor_stsql_read_projection_text(query, 0);
        bucket.source_id_ = vqec_vision_ai_stor_stsql_read_projection_text(query, 1);
        bucket.scene_revision_ = vqec_vision_ai_stor_stsql_read_projection_text(query, 2);
        bucket.definition_revision_ =
            vqec_vision_ai_stor_stsql_read_projection_text(query, 3);
        bucket.bucket_begin_ns_ = sqlite3_column_int64(query, 4);
        bucket.bucket_end_ns_ = sqlite3_column_int64(query, 5);
        bucket.numerator_microunits_ = sqlite3_column_int64(query, 6);
        bucket.denominator_microunits_ = sqlite3_column_int64(query, 7);
        bucket.observed_duration_ns_ =
            static_cast<std::uint64_t>(sqlite3_column_int64(query, 8));
        bucket.expected_duration_ns_ =
            static_cast<std::uint64_t>(sqlite3_column_int64(query, 9));
        bucket.contribution_count_ =
            static_cast<std::uint64_t>(sqlite3_column_int64(query, 10));
        if (!vqec_vision_ai_stor_stsql_decode_dimensions(
                dimensions, bucket.dimensions_)) {
            return {status_code::io_error, "materialized rollup dimensions are corrupt"};
        }
        _page.next_cursor_sequence_ = std::max(_page.next_cursor_sequence_,
            static_cast<std::uint64_t>(sqlite3_column_int64(query, 12)));
        _page.scanned_bytes_ += row_bytes;
        _page.result_bytes_ += row_bytes;
        _page.aggregate_buckets_.push_back(std::move(bucket));
    }
    _page.delivered_resolution_ = trajectory_resolution::aggregate;
    _page.completeness_ = spatiotemporal_result_completeness::complete;
    return {};
}

}  // namespace

status sqlite_spatiotemporal_store::vqec_vision_ai_stor_stsql_ingest_episode(
    const event_episode_revision& _episode,
    const std::vector<std::string>& _outbox_sinks) {
    if (catalog_ == nullptr) {
        return {status_code::invalid_state, "spatiotemporal store is not open"};
    }
    auto result = vqec_vision_ai_cntr_stmet_validate_episode_revision(_episode);
    if (result.code_ != status_code::ok) {
        return result;
    }
    if (!vqec_vision_ai_stor_stsql_validate_projection_sinks(_outbox_sinks)) {
        return {status_code::invalid_argument, "episode outbox sinks are invalid"};
    }
    const auto claims = vqec_vision_ai_stor_stsql_encode_dimensions(_episode.claims_);
    const auto references =
        vqec_vision_ai_stor_stsql_encode_references(_episode.evidence_references_);
    const auto payload = vqec_vision_ai_stor_stsql_encode_episode_payload(
        _episode, claims, references);
    bool is_idempotent = false;
    result = vqec_vision_ai_stor_stsql_check_projection_revision(catalog_,
        "episode_revisions", "episode_id", _episode.episode_id_, _episode.revision_,
        _episode.supersedes_revision_, payload, is_idempotent);
    if (result.code_ != status_code::ok || is_idempotent) {
        return result;
    }
    const bool owns_transaction = sqlite3_get_autocommit(catalog_) != 0;
    std::uint64_t sequence = 0U;
    result = vqec_vision_ai_stor_stsql_begin_projection(catalog_, sequence);
    if (result.code_ != status_code::ok) {
        return result;
    }
    projection_statement statement;
    result = vqec_vision_ai_stor_stsql_prepare_projection(catalog_,
        "INSERT INTO episode_revisions(global_sequence,episode_id,revision,"
        "supersedes_revision,source_id,semantic_type,subject_ref,scene_revision,"
        "rule_revision,begin_ns,end_ns,recorded_ns,lifecycle,severity_ppm,"
        "required_access_mask,claims,evidence_references,canonical_payload) VALUES("
        "?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18);",
        statement);
    auto* insert = statement.vqec_vision_ai_stor_stsql_get();
    const bool is_bound = result.code_ == status_code::ok &&
        sqlite3_bind_int64(insert, 1, static_cast<sqlite3_int64>(sequence)) == SQLITE_OK &&
        vqec_vision_ai_stor_stsql_bind_projection_text(insert, 2, _episode.episode_id_) &&
        sqlite3_bind_int64(insert, 3, static_cast<sqlite3_int64>(_episode.revision_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 4, static_cast<sqlite3_int64>(_episode.supersedes_revision_)) == SQLITE_OK &&
        vqec_vision_ai_stor_stsql_bind_projection_text(insert, 5, _episode.source_id_) &&
        vqec_vision_ai_stor_stsql_bind_projection_text(insert, 6, _episode.semantic_type_) &&
        vqec_vision_ai_stor_stsql_bind_projection_text(insert, 7, _episode.subject_ref_) &&
        vqec_vision_ai_stor_stsql_bind_projection_text(insert, 8, _episode.scene_revision_) &&
        vqec_vision_ai_stor_stsql_bind_projection_text(insert, 9, _episode.rule_revision_) &&
        sqlite3_bind_int64(insert, 10, _episode.begin_ns_) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 11, _episode.end_ns_) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 12, _episode.recorded_ns_) == SQLITE_OK &&
        sqlite3_bind_int(insert, 13, static_cast<int>(_episode.lifecycle_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 14, _episode.severity_ppm_) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 15, _episode.required_access_domain_mask_) == SQLITE_OK &&
        vqec_vision_ai_stor_stsql_bind_projection_blob(insert, 16, claims) &&
        vqec_vision_ai_stor_stsql_bind_projection_blob(insert, 17, references) &&
        vqec_vision_ai_stor_stsql_bind_projection_blob(insert, 18, payload);
    if (!is_bound || sqlite3_step(insert) != SQLITE_DONE) {
        (void)vqec_vision_ai_stor_stsql_execute_projection(catalog_, "ROLLBACK;");
        return result.code_ == status_code::ok
            ? vqec_vision_ai_stor_stsql_make_projection_error(catalog_, "insert episode")
            : result;
    }
    result = vqec_vision_ai_stor_stsql_insert_projection_outbox(catalog_, _outbox_sinks,
        g_spatiotemporal_episode_family, _episode.episode_id_, _episode.revision_);
    if (result.code_ == status_code::ok) {
        if (owns_transaction) {
            result = vqec_vision_ai_stor_stsql_execute_projection(catalog_, "COMMIT;");
        }
    } else {
        (void)vqec_vision_ai_stor_stsql_execute_projection(catalog_, "ROLLBACK;");
    }
    if (result.code_ == status_code::ok) {
        ++stats_.committed_episode_revisions_;
    }
    return result;
}

status sqlite_spatiotemporal_store::
vqec_vision_ai_stor_stsql_ingest_aggregate_contribution(
    const aggregate_contribution_revision& _contribution,
    const std::vector<std::string>& _outbox_sinks) {
    if (catalog_ == nullptr) {
        return {status_code::invalid_state, "spatiotemporal store is not open"};
    }
    auto result =
        vqec_vision_ai_cntr_stmet_validate_aggregate_contribution(_contribution);
    if (result.code_ != status_code::ok) {
        return result;
    }
    if (!vqec_vision_ai_stor_stsql_validate_projection_sinks(_outbox_sinks)) {
        return {status_code::invalid_argument, "aggregate outbox sinks are invalid"};
    }
    const auto dimensions =
        vqec_vision_ai_stor_stsql_encode_dimensions(_contribution.dimensions_);
    const auto payload = vqec_vision_ai_stor_stsql_encode_contribution_payload(
        _contribution, dimensions);
    bool is_idempotent = false;
    result = vqec_vision_ai_stor_stsql_check_projection_revision(catalog_,
        "aggregate_contribution_revisions", "contribution_id",
        _contribution.contribution_id_, _contribution.revision_,
        _contribution.supersedes_revision_, payload, is_idempotent);
    if (result.code_ != status_code::ok || is_idempotent) {
        return result;
    }
    const bool owns_transaction = sqlite3_get_autocommit(catalog_) != 0;
    std::uint64_t sequence = 0U;
    result = vqec_vision_ai_stor_stsql_begin_projection(catalog_, sequence);
    if (result.code_ != status_code::ok) {
        return result;
    }
    projection_statement statement;
    result = vqec_vision_ai_stor_stsql_prepare_projection(catalog_,
        "INSERT INTO aggregate_contribution_revisions(global_sequence,contribution_id,"
        "revision,supersedes_revision,episode_id,source_id,aggregate_definition_id,"
        "scene_revision,definition_revision,bucket_begin_ns,bucket_end_ns,recorded_ns,"
        "operation,numerator_microunits,denominator_microunits,observed_duration_ns,"
        "expected_duration_ns,required_access_mask,dimensions,canonical_payload) VALUES("
        "?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20);",
        statement);
    auto* insert = statement.vqec_vision_ai_stor_stsql_get();
    const bool is_bound = result.code_ == status_code::ok &&
        sqlite3_bind_int64(insert, 1, static_cast<sqlite3_int64>(sequence)) == SQLITE_OK &&
        vqec_vision_ai_stor_stsql_bind_projection_text(insert, 2, _contribution.contribution_id_) &&
        sqlite3_bind_int64(insert, 3, static_cast<sqlite3_int64>(_contribution.revision_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 4, static_cast<sqlite3_int64>(_contribution.supersedes_revision_)) == SQLITE_OK &&
        vqec_vision_ai_stor_stsql_bind_projection_text(insert, 5, _contribution.episode_id_) &&
        vqec_vision_ai_stor_stsql_bind_projection_text(insert, 6, _contribution.source_id_) &&
        vqec_vision_ai_stor_stsql_bind_projection_text(insert, 7, _contribution.aggregate_definition_id_) &&
        vqec_vision_ai_stor_stsql_bind_projection_text(insert, 8, _contribution.scene_revision_) &&
        vqec_vision_ai_stor_stsql_bind_projection_text(insert, 9, _contribution.definition_revision_) &&
        sqlite3_bind_int64(insert, 10, _contribution.bucket_begin_ns_) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 11, _contribution.bucket_end_ns_) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 12, _contribution.recorded_ns_) == SQLITE_OK &&
        sqlite3_bind_int(insert, 13, static_cast<int>(_contribution.operation_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 14, _contribution.numerator_microunits_) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 15, _contribution.denominator_microunits_) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 16, static_cast<sqlite3_int64>(_contribution.observed_duration_ns_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 17, static_cast<sqlite3_int64>(_contribution.expected_duration_ns_)) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 18, _contribution.required_access_domain_mask_) == SQLITE_OK &&
        vqec_vision_ai_stor_stsql_bind_projection_blob(insert, 19, dimensions) &&
        vqec_vision_ai_stor_stsql_bind_projection_blob(insert, 20, payload);
    if (!is_bound || sqlite3_step(insert) != SQLITE_DONE) {
        (void)vqec_vision_ai_stor_stsql_execute_projection(catalog_, "ROLLBACK;");
        return result.code_ == status_code::ok
            ? vqec_vision_ai_stor_stsql_make_projection_error(
                  catalog_, "insert aggregate contribution")
            : result;
    }
    result = vqec_vision_ai_stor_stsql_insert_projection_outbox(catalog_, _outbox_sinks,
        g_spatiotemporal_aggregate_family, _contribution.contribution_id_,
        _contribution.revision_);
    if (result.code_ == status_code::ok && _contribution.supersedes_revision_ != 0U) {
        aggregate_contribution_revision previous;
        std::vector<std::uint8_t> previous_dimensions;
        result = vqec_vision_ai_stor_stsql_load_contribution_revision(catalog_,
            _contribution.contribution_id_, _contribution.supersedes_revision_,
            previous, previous_dimensions);
        if (result.code_ == status_code::ok &&
            previous.operation_ == aggregate_contribution_operation::add) {
            result = vqec_vision_ai_stor_stsql_apply_rollup_delta(
                catalog_, previous, previous_dimensions, -1, sequence);
        }
    }
    if (result.code_ == status_code::ok &&
        _contribution.operation_ == aggregate_contribution_operation::add) {
        result = vqec_vision_ai_stor_stsql_apply_rollup_delta(
            catalog_, _contribution, dimensions, 1, sequence);
    }
    if (result.code_ == status_code::ok) {
        if (owns_transaction) {
            result = vqec_vision_ai_stor_stsql_execute_projection(catalog_, "COMMIT;");
        }
    } else {
        (void)vqec_vision_ai_stor_stsql_execute_projection(catalog_, "ROLLBACK;");
    }
    if (result.code_ == status_code::ok) {
        ++stats_.committed_aggregate_revisions_;
    }
    return result;
}

status sqlite_spatiotemporal_store::vqec_vision_ai_stor_stsql_ingest_projection_batch(
    const std::vector<event_episode_revision>& _episodes,
    const std::vector<aggregate_contribution_revision>& _contributions,
    const std::vector<std::string>& _outbox_sinks) {
    if (catalog_ == nullptr) {
        return {status_code::invalid_state, "spatiotemporal store is not open"};
    }
    if (_episodes.empty() && _contributions.empty()) {
        return {status_code::invalid_argument, "metadata projection batch is empty"};
    }
    if (_episodes.size() > std::numeric_limits<std::size_t>::max() -
            _contributions.size()) {
        return {status_code::invalid_argument, "metadata projection batch size overflows"};
    }
    const auto stats_before = stats_;
    auto result = vqec_vision_ai_stor_stsql_execute_projection(catalog_, "BEGIN IMMEDIATE;");
    if (result.code_ != status_code::ok) {
        return result;
    }
    for (const auto& episode : _episodes) {
        result = vqec_vision_ai_stor_stsql_ingest_episode(episode, _outbox_sinks);
        if (result.code_ != status_code::ok) {
            break;
        }
    }
    if (result.code_ == status_code::ok) {
        for (const auto& contribution : _contributions) {
            result = vqec_vision_ai_stor_stsql_ingest_aggregate_contribution(
                contribution, _outbox_sinks);
            if (result.code_ != status_code::ok) {
                break;
            }
        }
    }
    if (result.code_ == status_code::ok) {
        result = vqec_vision_ai_stor_stsql_execute_projection(catalog_, "COMMIT;");
    }
    if (result.code_ != status_code::ok) {
        if (sqlite3_get_autocommit(catalog_) == 0) {
            (void)vqec_vision_ai_stor_stsql_execute_projection(catalog_, "ROLLBACK;");
        }
        stats_ = stats_before;
    }
    return result;
}

status sqlite_spatiotemporal_store::vqec_vision_ai_stor_stsql_query_projection(
    const spatiotemporal_query& _query, std::uint64_t _snapshot_sequence,
    spatiotemporal_query_page& _page) {
    if (_query.spatial_relation_ != spatiotemporal_spatial_relation::none) {
        _page.completeness_ = spatiotemporal_result_completeness::unsupported;
        return {status_code::unsupported,
            "projection queries do not accept trajectory spatial predicates"};
    }
    const bool is_episode = _query.collection_ == spatiotemporal_collection::episodes;
    const auto required_domain = vqec_vision_ai_cntr_stmet_get_access_domain_mask(
        is_episode ? spatiotemporal_access_domain::object
                   : spatiotemporal_access_domain::aggregate);
    if ((_query.allowed_access_domain_mask_ & required_domain) == 0U) {
        return {status_code::unauthorized, "metadata projection access is denied"};
    }
    if (!is_episode &&
        _query.revision_view_ == spatiotemporal_revision_view::latest_corrected &&
        _query.cursor_sequence_ == 0U) {
        return vqec_vision_ai_stor_stsql_query_materialized_rollups(
            catalog_, _query, _snapshot_sequence, config_.maximum_query_results_,
            query_cancel_requested_, _page);
    }
    std::ostringstream sql;
    if (is_episode) {
        sql << "SELECT global_sequence,episode_id,revision,supersedes_revision,source_id,"
               "semantic_type,subject_ref,scene_revision,rule_revision,begin_ns,end_ns,"
               "recorded_ns,lifecycle,severity_ppm,required_access_mask,claims,"
               "evidence_references FROM episode_revisions e WHERE global_sequence>?1 "
               "AND global_sequence<=?2 AND begin_ns<?3 AND end_ns>?4 "
               "AND (required_access_mask & ?5)=required_access_mask";
    } else {
        sql << "SELECT global_sequence,contribution_id,revision,supersedes_revision,episode_id,"
               "source_id,aggregate_definition_id,scene_revision,definition_revision,"
               "bucket_begin_ns,bucket_end_ns,recorded_ns,operation,numerator_microunits,"
               "denominator_microunits,observed_duration_ns,expected_duration_ns,"
               "required_access_mask,dimensions FROM aggregate_contribution_revisions a "
               "WHERE global_sequence>?1 AND global_sequence<=?2 AND bucket_begin_ns<?3 "
               "AND bucket_end_ns>?4 AND (required_access_mask & ?5)=required_access_mask";
    }
    int bind_index = 6;
    sql << " AND source_id IN (";
    for (std::size_t index = 0U; index < _query.source_ids_.size(); ++index) {
        if (index != 0U) {
            sql << ',';
        }
        sql << '?' << bind_index++;
    }
    sql << ')';
    const auto semantic_index = _query.semantic_type_.empty() ? 0 : bind_index++;
    if (semantic_index != 0) {
        sql << (is_episode ? " AND semantic_type=?" : " AND aggregate_definition_id=?")
            << semantic_index;
    }
    if (is_episode && !_query.subject_ref_.empty()) {
        sql << " AND subject_ref=?" << bind_index++;
    }
    const auto alias = is_episode ? "e" : "a";
    const auto table = is_episode ? "episode_revisions" : "aggregate_contribution_revisions";
    const auto id_column = is_episode ? "episode_id" : "contribution_id";
    if (_query.revision_view_ == spatiotemporal_revision_view::as_observed) {
        sql << " AND revision=1";
    } else {
        sql << " AND revision=(SELECT MAX(p2.revision) FROM " << table
            << " p2 WHERE p2." << id_column << '=' << alias << '.' << id_column
            << " AND p2.global_sequence<=?2";
        if (_query.revision_view_ == spatiotemporal_revision_view::as_known_at) {
            sql << " AND p2.recorded_ns<=" << _query.known_at_ns_;
        }
        sql << ')';
    }
    sql << " ORDER BY global_sequence;";
    projection_statement statement;
    auto result = vqec_vision_ai_stor_stsql_prepare_projection(catalog_, sql.str(), statement);
    auto* query = statement.vqec_vision_ai_stor_stsql_get();
    if (result.code_ != status_code::ok ||
        sqlite3_bind_int64(query, 1, static_cast<sqlite3_int64>(_query.cursor_sequence_)) != SQLITE_OK ||
        sqlite3_bind_int64(query, 2, static_cast<sqlite3_int64>(_snapshot_sequence)) != SQLITE_OK ||
        sqlite3_bind_int64(query, 3, _query.end_ns_) != SQLITE_OK ||
        sqlite3_bind_int64(query, 4, _query.begin_ns_) != SQLITE_OK ||
        sqlite3_bind_int64(query, 5, _query.allowed_access_domain_mask_) != SQLITE_OK) {
        return result.code_ == status_code::ok
            ? vqec_vision_ai_stor_stsql_make_projection_error(
                  catalog_, "bind metadata projection query")
            : result;
    }
    int value_index = 6;
    for (const auto& source : _query.source_ids_) {
        if (!vqec_vision_ai_stor_stsql_bind_projection_text(query, value_index++, source)) {
            return vqec_vision_ai_stor_stsql_make_projection_error(
                catalog_, "bind metadata projection source");
        }
    }
    if (semantic_index != 0 &&
        !vqec_vision_ai_stor_stsql_bind_projection_text(
            query, value_index++, _query.semantic_type_)) {
        return vqec_vision_ai_stor_stsql_make_projection_error(
            catalog_, "bind metadata projection semantic type");
    }
    if (is_episode && !_query.subject_ref_.empty() &&
        !vqec_vision_ai_stor_stsql_bind_projection_text(
            query, value_index++, _query.subject_ref_)) {
        return vqec_vision_ai_stor_stsql_make_projection_error(
            catalog_, "bind metadata projection subject");
    }
    const auto result_limit = std::min(
        _query.budget_.maximum_results_, config_.maximum_query_results_);
    std::map<std::vector<std::uint8_t>, aggregate_bucket> aggregate_map;
    while (sqlite3_step(query) == SQLITE_ROW) {
        if (query_cancel_requested_.exchange(false, std::memory_order_acq_rel)) {
            _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
            return {status_code::timeout, "metadata projection query was cancelled"};
        }
        if (vqec_vision_ai_stor_stsql_get_projection_now_ns() >= _query.budget_.deadline_ns_) {
            _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
            return {};
        }
        const auto claims_column = is_episode ? 15 : 18;
        const auto encoded = vqec_vision_ai_stor_stsql_read_projection_blob(
            query, claims_column);
        const auto row_bytes = static_cast<std::uint64_t>(encoded.size() + 256U);
        if (row_bytes > _query.budget_.maximum_scan_bytes_ -
                std::min(_query.budget_.maximum_scan_bytes_, _page.scanned_bytes_)) {
            _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
            return {};
        }
        _page.scanned_bytes_ += row_bytes;
        _page.next_cursor_sequence_ =
            static_cast<std::uint64_t>(sqlite3_column_int64(query, 0));
        if (is_episode) {
            if (_page.episodes_.size() == result_limit ||
                row_bytes > _query.budget_.maximum_result_bytes_ -
                    std::min(_query.budget_.maximum_result_bytes_, _page.result_bytes_)) {
                _page.has_more_ = true;
                break;
            }
            event_episode_revision episode;
            episode.episode_id_ = vqec_vision_ai_stor_stsql_read_projection_text(query, 1);
            episode.revision_ = static_cast<std::uint64_t>(sqlite3_column_int64(query, 2));
            episode.supersedes_revision_ =
                static_cast<std::uint64_t>(sqlite3_column_int64(query, 3));
            episode.source_id_ = vqec_vision_ai_stor_stsql_read_projection_text(query, 4);
            episode.semantic_type_ = vqec_vision_ai_stor_stsql_read_projection_text(query, 5);
            episode.subject_ref_ = vqec_vision_ai_stor_stsql_read_projection_text(query, 6);
            episode.scene_revision_ = vqec_vision_ai_stor_stsql_read_projection_text(query, 7);
            episode.rule_revision_ = vqec_vision_ai_stor_stsql_read_projection_text(query, 8);
            episode.begin_ns_ = sqlite3_column_int64(query, 9);
            episode.end_ns_ = sqlite3_column_int64(query, 10);
            episode.recorded_ns_ = sqlite3_column_int64(query, 11);
            episode.lifecycle_ = static_cast<episode_lifecycle>(sqlite3_column_int(query, 12));
            episode.severity_ppm_ = static_cast<std::uint32_t>(sqlite3_column_int64(query, 13));
            episode.required_access_domain_mask_ =
                static_cast<std::uint32_t>(sqlite3_column_int64(query, 14));
            if (!vqec_vision_ai_stor_stsql_decode_dimensions(encoded, episode.claims_) ||
                !vqec_vision_ai_stor_stsql_decode_references(
                    vqec_vision_ai_stor_stsql_read_projection_blob(query, 16),
                    episode.evidence_references_)) {
                return {status_code::io_error, "stored episode projection is corrupt"};
            }
            _page.result_bytes_ += row_bytes;
            _page.episodes_.push_back(std::move(episode));
            continue;
        }
        const auto operation = static_cast<aggregate_contribution_operation>(
            sqlite3_column_int(query, 12));
        if (operation == aggregate_contribution_operation::retract) {
            continue;
        }
        std::vector<spatiotemporal_dimension> dimensions;
        if (!vqec_vision_ai_stor_stsql_decode_dimensions(encoded, dimensions)) {
            return {status_code::io_error, "stored aggregate dimensions are corrupt"};
        }
        std::vector<std::uint8_t> key;
        const auto definition = vqec_vision_ai_stor_stsql_read_projection_text(query, 6);
        const auto source = vqec_vision_ai_stor_stsql_read_projection_text(query, 5);
        const auto scene = vqec_vision_ai_stor_stsql_read_projection_text(query, 7);
        const auto revision = vqec_vision_ai_stor_stsql_read_projection_text(query, 8);
        vqec_vision_ai_stor_stsql_append_projection_string(key, definition);
        vqec_vision_ai_stor_stsql_append_projection_string(key, source);
        vqec_vision_ai_stor_stsql_append_projection_string(key, scene);
        vqec_vision_ai_stor_stsql_append_projection_string(key, revision);
        vqec_vision_ai_stor_stsql_append_projection_i64(key, sqlite3_column_int64(query, 9));
        vqec_vision_ai_stor_stsql_append_projection_i64(key, sqlite3_column_int64(query, 10));
        key.insert(key.end(), encoded.begin(), encoded.end());
        auto& bucket = aggregate_map[key];
        if (bucket.contribution_count_ == 0U) {
            bucket.aggregate_definition_id_ = definition;
            bucket.source_id_ = source;
            bucket.scene_revision_ = scene;
            bucket.definition_revision_ = revision;
            bucket.bucket_begin_ns_ = sqlite3_column_int64(query, 9);
            bucket.bucket_end_ns_ = sqlite3_column_int64(query, 10);
            bucket.dimensions_ = std::move(dimensions);
        }
        std::int64_t numerator = 0;
        std::int64_t denominator = 0;
        std::uint64_t observed = 0U;
        std::uint64_t expected = 0U;
        if (!vqec_vision_ai_stor_stsql_checked_add_i64(bucket.numerator_microunits_,
                sqlite3_column_int64(query, 13), numerator) ||
            !vqec_vision_ai_stor_stsql_checked_add_i64(bucket.denominator_microunits_,
                sqlite3_column_int64(query, 14), denominator) ||
            !vqec_vision_ai_stor_stsql_checked_add_u64(bucket.observed_duration_ns_,
                static_cast<std::uint64_t>(sqlite3_column_int64(query, 15)), observed) ||
            !vqec_vision_ai_stor_stsql_checked_add_u64(bucket.expected_duration_ns_,
                static_cast<std::uint64_t>(sqlite3_column_int64(query, 16)), expected) ||
            bucket.contribution_count_ == std::numeric_limits<std::uint64_t>::max()) {
            return {status_code::resource_exhausted, "aggregate bucket overflow"};
        }
        bucket.numerator_microunits_ = numerator;
        bucket.denominator_microunits_ = denominator;
        bucket.observed_duration_ns_ = observed;
        bucket.expected_duration_ns_ = expected;
        ++bucket.contribution_count_;
    }
    if (query_cancel_requested_.exchange(false, std::memory_order_acq_rel)) {
        _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
        return {status_code::timeout, "metadata projection query was cancelled"};
    }
    if (!is_episode) {
        if (aggregate_map.size() > result_limit) {
            _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
            return {};
        }
        for (auto& entry : aggregate_map) {
            if (query_cancel_requested_.exchange(false, std::memory_order_acq_rel)) {
                _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
                return {status_code::timeout, "metadata projection query was cancelled"};
            }
            const auto row_bytes = static_cast<std::uint64_t>(entry.first.size() + 128U);
            if (row_bytes > _query.budget_.maximum_result_bytes_ -
                    std::min(_query.budget_.maximum_result_bytes_, _page.result_bytes_)) {
                _page.completeness_ = spatiotemporal_result_completeness::budget_exceeded;
                return {};
            }
            _page.result_bytes_ += row_bytes;
            _page.aggregate_buckets_.push_back(std::move(entry.second));
        }
    }
    _page.delivered_resolution_ = is_episode
        ? trajectory_resolution::episode_fact
        : trajectory_resolution::aggregate;
    _page.completeness_ = spatiotemporal_result_completeness::complete;
    return {};
}

}  // namespace vqec::vision::ai
