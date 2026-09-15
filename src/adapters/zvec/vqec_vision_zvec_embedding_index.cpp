#include "vqec_vision_zvec_embedding_index.hpp"

#include <algorithm>
#include <cmath>
#include <charconv>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

#include <zvec/c_api.h>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

constexpr char g_collection_name[] = "lacai_face_gallery";
constexpr char g_record_id_field[] = "record_id";
constexpr char g_subject_ref_field[] = "subject_ref";
constexpr char g_embedding_field[] = "embedding";

bool vqec_vision_ai_zvec_zvidx_valid_revision_change(
    std::uint64_t _current, std::uint64_t _expected,
    std::uint64_t _replacement) noexcept {
    return _current == _expected && _expected != UINT64_MAX &&
        _replacement == _expected + 1U;
}

bool vqec_vision_ai_zvec_zvidx_is_normalized(
    const std::vector<float>& _values) noexcept {
    double squared_norm = 0.0;
    for (const float value : _values) {
        if (!std::isfinite(value)) {
            return false;
        }
        squared_norm += static_cast<double>(value) * value;
    }
    return std::fabs(std::sqrt(squared_norm) - 1.0) <=
        embedding_limits::g_normalized_tolerance;
}

std::string vqec_vision_ai_zvec_zvidx_record_key(std::uint64_t _record_id) {
    return std::to_string(_record_id);
}

status vqec_vision_ai_zvec_zvidx_map_error(zvec_error_code_t _error,
    const char* _message) noexcept {
    if (_error == ZVEC_OK) {
        return {};
    }
    if (_error == ZVEC_ERROR_RESOURCE_EXHAUSTED) {
        return {status_code::resource_exhausted, _message};
    }
    if (_error == ZVEC_ERROR_NOT_SUPPORTED) {
        return {status_code::unsupported, _message};
    }
    if (_error == ZVEC_ERROR_UNAVAILABLE) {
        return {status_code::io_error, _message};
    }
    return {status_code::io_error, _message};
}

bool vqec_vision_ai_zvec_zvidx_contains(
    const std::vector<std::uint64_t>& _record_ids, std::uint64_t _record_id) {
    for (const auto value : _record_ids) {
        if (value == _record_id) {
            return true;
        }
    }
    return false;
}

status vqec_vision_ai_zvec_zvidx_create_collection(
    const std::string& _path, std::size_t _dimensions,
    zvec_collection_t*& _collection) noexcept {
    zvec_collection_schema_t* schema =
        zvec_collection_schema_create(g_collection_name);
    if (schema == nullptr) {
        return {status_code::resource_exhausted, "Zvec schema allocation failed"};
    }
    zvec_index_params_t* invert = zvec_index_params_create(ZVEC_INDEX_TYPE_INVERT);
    zvec_index_params_t* flat = zvec_index_params_create(ZVEC_INDEX_TYPE_FLAT);
    zvec_field_schema_t* record_id = zvec_field_schema_create(
        g_record_id_field, ZVEC_DATA_TYPE_STRING, false, 0U);
    zvec_field_schema_t* subject_ref = zvec_field_schema_create(
        g_subject_ref_field, ZVEC_DATA_TYPE_STRING, false, 0U);
    zvec_field_schema_t* embedding = zvec_field_schema_create(
        g_embedding_field, ZVEC_DATA_TYPE_VECTOR_FP32, false,
        static_cast<std::uint32_t>(_dimensions));
    if (invert == nullptr || flat == nullptr || record_id == nullptr ||
        subject_ref == nullptr || embedding == nullptr) {
        zvec_field_schema_destroy(embedding);
        zvec_field_schema_destroy(subject_ref);
        zvec_field_schema_destroy(record_id);
        zvec_index_params_destroy(flat);
        zvec_index_params_destroy(invert);
        zvec_collection_schema_destroy(schema);
        return {status_code::resource_exhausted, "Zvec schema allocation failed"};
    }
    zvec_index_params_set_metric_type(flat, ZVEC_METRIC_TYPE_COSINE);
    auto error = zvec_field_schema_set_index_params(record_id, invert);
    if (error == ZVEC_OK) {
        error = zvec_field_schema_set_index_params(subject_ref, invert);
    }
    if (error == ZVEC_OK) {
        error = zvec_field_schema_set_index_params(embedding, flat);
    }
    if (error == ZVEC_OK) {
        error = zvec_collection_schema_add_field(schema, record_id);
    }
    if (error == ZVEC_OK) {
        error = zvec_collection_schema_add_field(schema, subject_ref);
    }
    if (error == ZVEC_OK) {
        error = zvec_collection_schema_add_field(schema, embedding);
    }
    zvec_field_schema_destroy(embedding);
    zvec_field_schema_destroy(subject_ref);
    zvec_field_schema_destroy(record_id);
    zvec_index_params_destroy(flat);
    zvec_index_params_destroy(invert);
    if (error == ZVEC_OK) {
        error = zvec_collection_create_and_open(_path.c_str(), schema, nullptr,
            &_collection);
    }
    zvec_collection_schema_destroy(schema);
    return vqec_vision_ai_zvec_zvidx_map_error(error,
        "Zvec collection creation failed");
}

status vqec_vision_ai_zvec_zvidx_validate_query(
    const embedding_index_config& _config, const embedding_result& _query,
    std::size_t _top_k, float _minimum_similarity) {
    if (_query.model_id_ != _config.model_id_ ||
        _query.model_version_ != _config.model_version_ ||
        _query.values_.size() != _config.dimensions_ || !_query.is_l2_normalized_ ||
        _top_k == 0 || _top_k > _config.max_results_ ||
        !std::isfinite(_minimum_similarity) || _minimum_similarity < -1.0F ||
        _minimum_similarity > 1.0F ||
        !vqec_vision_ai_zvec_zvidx_is_normalized(_query.values_)) {
        return {status_code::invalid_argument, "Zvec embedding query is invalid"};
    }
    return {};
}

}  // namespace

zvec_embedding_index::zvec_embedding_index(std::string _collection_path)
    : collection_path_(std::move(_collection_path)) {}

zvec_embedding_index::~zvec_embedding_index() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (collection_ != nullptr) {
        zvec_collection_close(collection_);
        collection_ = nullptr;
    }
}

status zvec_embedding_index::vqec_vision_ai_ports_emidx_configure(
    const embedding_index_config& _config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_configured_ || is_faulted_) {
        return {status_code::invalid_state, "Zvec index is already configured"};
    }
    if (collection_path_.empty() ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _config.model_id_, embedding_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _config.model_version_, embedding_limits::g_max_identifier_bytes) ||
        _config.dimensions_ == 0 || _config.dimensions_ > embedding_limits::g_max_dimensions ||
        _config.capacity_ == 0 || _config.max_results_ == 0 ||
        _config.max_results_ > embedding_index_limits::g_max_results ||
        _config.initial_revision_ == 0 || _config.initial_revision_ == UINT64_MAX ||
        _config.metric_ != embedding_metric::cosine_similarity ||
        _config.dimensions_ > std::numeric_limits<std::uint32_t>::max()) {
        return {status_code::invalid_argument, "Zvec index configuration is invalid"};
    }
    // Allocate bookkeeping before creating persistent state.
    config_ = _config;
    record_ids_.reserve(_config.capacity_);
    const auto create_status = vqec_vision_ai_zvec_zvidx_create_collection(
        collection_path_, _config.dimensions_, collection_);
    if (create_status.code_ != status_code::ok) {
        // The revision is not persisted by this adapter. Reusing an old collection
        // without an authoritative revision handshake would permit stale matches.
        return create_status;
    }
    revision_ = _config.initial_revision_;
    is_configured_ = true;
    return {};
}

status zvec_embedding_index::vqec_vision_ai_ports_emidx_upsert(
    const embedding_gallery_record& _record, std::uint64_t _expected_revision,
    std::uint64_t _new_revision) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_configured_ || is_faulted_) {
        return {status_code::invalid_state, "Zvec index is not configured"};
    }
    if (!vqec_vision_ai_zvec_zvidx_valid_revision_change(
            revision_, _expected_revision, _new_revision)) {
        return {status_code::invalid_state, "embedding gallery revision conflict"};
    }
    if (_record.record_id_ == 0 ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _record.subject_ref_, embedding_index_limits::g_max_subject_ref_bytes) ||
        _record.values_.size() != config_.dimensions_ ||
        !vqec_vision_ai_zvec_zvidx_is_normalized(_record.values_)) {
        return {status_code::invalid_argument, "embedding gallery record is invalid"};
    }
    const bool exists = vqec_vision_ai_zvec_zvidx_contains(record_ids_, _record.record_id_);
    if (!exists && record_ids_.size() == config_.capacity_) {
        return {status_code::resource_exhausted, "embedding gallery capacity is full"};
    }
    const auto key = vqec_vision_ai_zvec_zvidx_record_key(_record.record_id_);
    zvec_doc_t* document = zvec_doc_create();
    if (document == nullptr) {
        return {status_code::resource_exhausted, "Zvec document allocation failed"};
    }
    zvec_doc_set_pk(document, key.c_str());
    auto error = zvec_doc_add_field_by_value(document, g_record_id_field,
        ZVEC_DATA_TYPE_STRING, key.c_str(), key.size());
    if (error == ZVEC_OK) {
        error = zvec_doc_add_field_by_value(document, g_subject_ref_field,
            ZVEC_DATA_TYPE_STRING, _record.subject_ref_.data(), _record.subject_ref_.size());
    }
    if (error == ZVEC_OK) {
        error = zvec_doc_add_field_by_value(document, g_embedding_field,
            ZVEC_DATA_TYPE_VECTOR_FP32, _record.values_.data(),
            _record.values_.size() * sizeof(float));
    }
    const zvec_doc_t* documents[] = {document};
    std::size_t success_count = 0;
    std::size_t error_count = 0;
    if (error == ZVEC_OK) {
        error = zvec_collection_upsert(collection_, documents, 1U,
            &success_count, &error_count);
    }
    zvec_doc_destroy(document);
    if (error != ZVEC_OK || success_count != 1U || error_count != 0U) {
        is_faulted_ = true;
        return vqec_vision_ai_zvec_zvidx_map_error(
            error == ZVEC_OK ? ZVEC_ERROR_INTERNAL_ERROR : error,
            "Zvec gallery upsert failed");
    }
    if (!exists) {
        record_ids_.push_back(_record.record_id_);
    }
    revision_ = _new_revision;
    return {};
}

status zvec_embedding_index::vqec_vision_ai_ports_emidx_remove(
    std::uint64_t _record_id, std::uint64_t _expected_revision,
    std::uint64_t _new_revision) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_configured_ || is_faulted_ || _record_id == 0) {
        return {status_code::invalid_state, "Zvec index or record identity is invalid"};
    }
    if (!vqec_vision_ai_zvec_zvidx_valid_revision_change(
            revision_, _expected_revision, _new_revision)) {
        return {status_code::invalid_state, "embedding gallery revision conflict"};
    }
    const auto found = std::find(record_ids_.begin(), record_ids_.end(), _record_id);
    if (found == record_ids_.end()) {
        return {status_code::invalid_argument, "embedding gallery record is unknown"};
    }
    const auto key = vqec_vision_ai_zvec_zvidx_record_key(_record_id);
    const char* keys[] = {key.c_str()};
    std::size_t success_count = 0;
    std::size_t error_count = 0;
    const auto error = zvec_collection_delete(collection_, keys, 1U,
        &success_count, &error_count);
    if (error != ZVEC_OK || success_count != 1U || error_count != 0U) {
        is_faulted_ = true;
        return vqec_vision_ai_zvec_zvidx_map_error(
            error == ZVEC_OK ? ZVEC_ERROR_INTERNAL_ERROR : error,
            "Zvec gallery remove failed");
    }
    record_ids_.erase(found);
    revision_ = _new_revision;
    return {};
}

status zvec_embedding_index::vqec_vision_ai_ports_emidx_search(
    const embedding_result& _query, std::uint64_t _required_revision,
    std::size_t _top_k, float _minimum_similarity,
    embedding_search_result& _result) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_configured_ || is_faulted_ || _required_revision != revision_) {
        return {status_code::invalid_state, "Zvec index revision is unavailable"};
    }
    const auto query_status = vqec_vision_ai_zvec_zvidx_validate_query(
        config_, _query, _top_k, _minimum_similarity);
    if (query_status.code_ != status_code::ok) {
        return query_status;
    }
    if (_result.matches_.capacity() < _top_k) {
        return {status_code::resource_exhausted,
            "embedding search result capacity is below requested top-k"};
    }
    zvec_vector_query_t* query = zvec_vector_query_create();
    if (query == nullptr) {
        return {status_code::resource_exhausted, "Zvec query allocation failed"};
    }
    auto error = zvec_vector_query_set_field_name(query, g_embedding_field);
    if (error == ZVEC_OK) {
        error = zvec_vector_query_set_query_vector(query, _query.values_.data(),
            _query.values_.size() * sizeof(float));
    }
    if (error == ZVEC_OK) {
        error = zvec_vector_query_set_topk(query, static_cast<int>(_top_k));
    }
    if (error == ZVEC_OK) {
        error = zvec_vector_query_set_include_doc_id(query, true);
    }
    zvec_doc_t** documents = nullptr;
    std::size_t document_count = 0;
    if (error == ZVEC_OK) {
        error = zvec_collection_query(collection_, query, &documents, &document_count);
    }
    zvec_vector_query_destroy(query);
    if (error != ZVEC_OK) {
        zvec_docs_free(documents, document_count);
        return vqec_vision_ai_zvec_zvidx_map_error(error, "Zvec gallery search failed");
    }
    _result.frame_ = _query.frame_;
    _result.track_id_ = _query.track_id_;
    _result.gallery_revision_ = revision_;
    _result.matches_.clear();
    for (std::size_t index = 0; index < document_count && index < _top_k; ++index) {
        // Zvec v0.7.0 COSINE returns distance (1 - cosine similarity).
        const float score = 1.0F - zvec_doc_get_score(documents[index]);
        const char* key = zvec_doc_get_pk_pointer(documents[index]);
        if (key == nullptr || !std::isfinite(score) || score < _minimum_similarity) {
            continue;
        }
        std::uint64_t record_id = 0;
        const auto key_end = key + std::strlen(key);
        const auto parsed = std::from_chars(key, key_end, record_id);
        if (parsed.ec != std::errc{} || parsed.ptr != key_end || record_id == 0 ||
            !vqec_vision_ai_zvec_zvidx_contains(record_ids_, record_id)) {
            zvec_docs_free(documents, document_count);
            _result.matches_.clear();
            return {status_code::protocol_error,
                "Zvec returned an invalid record identity"};
        }
        _result.matches_.push_back({record_id, score});
    }
    zvec_docs_free(documents, document_count);
    return {};
}

std::uint64_t zvec_embedding_index::vqec_vision_ai_ports_emidx_revision()
    const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return is_faulted_ ? 0 : revision_;
}

}  // namespace vqec::vision::ai
