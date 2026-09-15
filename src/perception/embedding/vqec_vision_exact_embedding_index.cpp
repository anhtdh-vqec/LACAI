#include "vqec_vision_exact_embedding_index.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <new>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_embed_exidx_is_normalized(
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

bool vqec_vision_ai_embed_exidx_valid_revision_change(
    std::uint64_t _current, std::uint64_t _expected,
    std::uint64_t _replacement) noexcept {
    return _current == _expected && _expected != UINT64_MAX &&
        _replacement == _expected + 1U;
}

}  // namespace

status exact_embedding_index::vqec_vision_ai_ports_emidx_configure(
    const embedding_index_config& _config) {
    if (is_configured_) {
        return {status_code::invalid_state, "embedding index is already configured"};
    }
    if (!vqec_vision_ai_cntr_ident_is_valid(
            _config.model_id_, embedding_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _config.model_version_, embedding_limits::g_max_identifier_bytes) ||
        _config.dimensions_ == 0 ||
        _config.dimensions_ > embedding_limits::g_max_dimensions ||
        _config.capacity_ == 0 || _config.max_results_ == 0 ||
        _config.max_results_ > embedding_index_limits::g_max_results ||
        _config.initial_revision_ == 0 ||
        _config.initial_revision_ == UINT64_MAX ||
        _config.metric_ != embedding_metric::cosine_similarity) {
        return {status_code::invalid_argument, "embedding index configuration is invalid"};
    }
    try {
        std::vector<embedding_gallery_record> candidate;
        candidate.reserve(_config.capacity_);
        records_ = std::move(candidate);
        config_ = _config;
        revision_ = _config.initial_revision_;
        is_configured_ = true;
        return {};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "embedding index allocation failed"};
    }
}

status exact_embedding_index::vqec_vision_ai_ports_emidx_upsert(
    const embedding_gallery_record& _record, std::uint64_t _expected_revision,
    std::uint64_t _new_revision) {
    if (!is_configured_) {
        return {status_code::invalid_state, "embedding index is not configured"};
    }
    if (!vqec_vision_ai_embed_exidx_valid_revision_change(
            revision_, _expected_revision, _new_revision)) {
        return {status_code::invalid_state, "embedding gallery revision conflict"};
    }
    if (_record.record_id_ == 0 ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _record.subject_ref_, embedding_index_limits::g_max_subject_ref_bytes) ||
        _record.values_.size() != config_.dimensions_ ||
        !vqec_vision_ai_embed_exidx_is_normalized(_record.values_)) {
        return {status_code::invalid_argument, "embedding gallery record is invalid"};
    }
    auto found = std::find_if(records_.begin(), records_.end(),
        [&_record](const embedding_gallery_record& _item) {
            return _item.record_id_ == _record.record_id_;
        });
    if (found == records_.end() && records_.size() == config_.capacity_) {
        return {status_code::resource_exhausted, "embedding gallery capacity is full"};
    }
    try {
        if (found == records_.end()) {
            records_.push_back(_record);
        } else {
            *found = _record;
        }
        revision_ = _new_revision;
        return {};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "embedding record allocation failed"};
    }
}

status exact_embedding_index::vqec_vision_ai_ports_emidx_remove(
    std::uint64_t _record_id, std::uint64_t _expected_revision,
    std::uint64_t _new_revision) {
    if (!is_configured_ || _record_id == 0) {
        return {status_code::invalid_state, "embedding index or record identity is invalid"};
    }
    if (!vqec_vision_ai_embed_exidx_valid_revision_change(
            revision_, _expected_revision, _new_revision)) {
        return {status_code::invalid_state, "embedding gallery revision conflict"};
    }
    const auto found = std::find_if(records_.begin(), records_.end(),
        [_record_id](const embedding_gallery_record& _item) {
            return _item.record_id_ == _record_id;
        });
    if (found == records_.end()) {
        return {status_code::invalid_argument, "embedding gallery record is unknown"};
    }
    records_.erase(found);
    revision_ = _new_revision;
    return {};
}

status exact_embedding_index::vqec_vision_ai_ports_emidx_search(
    const embedding_result& _query, std::uint64_t _required_revision,
    std::size_t _top_k, float _minimum_similarity,
    embedding_search_result& _result) const {
    if (!is_configured_ || _required_revision != revision_) {
        return {status_code::invalid_state, "embedding index revision is unavailable"};
    }
    if (_query.model_id_ != config_.model_id_ ||
        _query.model_version_ != config_.model_version_ ||
        _query.values_.size() != config_.dimensions_ || !_query.is_l2_normalized_ ||
        _top_k == 0 || _top_k > config_.max_results_ ||
        !std::isfinite(_minimum_similarity) || _minimum_similarity < -1.0F ||
        _minimum_similarity > 1.0F ||
        vqec_vision_ai_core_embct_validate_result(_query, _query.frame_).code_ !=
            status_code::ok) {
        return {status_code::invalid_argument, "embedding search query is invalid"};
    }
    if (_result.matches_.capacity() < _top_k) {
        return {status_code::resource_exhausted,
            "embedding search result capacity is below requested top-k"};
    }
    std::array<embedding_match, embedding_index_limits::g_max_results> matches{};
    std::size_t match_count = 0;
    try {
        for (const auto& record : records_) {
            double similarity = 0.0;
            for (std::size_t dimension = 0; dimension < config_.dimensions_; ++dimension) {
                similarity += static_cast<double>(_query.values_[dimension]) *
                    record.values_[dimension];
            }
            const float score = std::clamp(static_cast<float>(similarity), -1.0F, 1.0F);
            if (score >= _minimum_similarity) {
                const embedding_match current{
                    record.record_id_, record.subject_ref_, score};
                if (match_count < _top_k) {
                    matches[match_count++] = current;
                } else {
                    const auto worst = std::min_element(
                        matches.begin(), matches.begin() + match_count,
                        [](const embedding_match& _left, const embedding_match& _right) {
                            return _left.similarity_ < _right.similarity_ ||
                                (_left.similarity_ == _right.similarity_ &&
                                    _left.record_id_ > _right.record_id_);
                        });
                    if (current.similarity_ > worst->similarity_ ||
                        (current.similarity_ == worst->similarity_ &&
                            current.record_id_ < worst->record_id_)) {
                        *worst = current;
                    }
                }
            }
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "embedding search candidate allocation failed"};
    }
    std::sort(matches.begin(), matches.begin() + match_count,
            [](const embedding_match& _left, const embedding_match& _right) {
                return _left.similarity_ > _right.similarity_ ||
                    (_left.similarity_ == _right.similarity_ &&
                        _left.record_id_ < _right.record_id_);
            });
    _result.frame_ = _query.frame_;
    _result.track_id_ = _query.track_id_;
    _result.gallery_revision_ = revision_;
    _result.matches_.clear();
    const auto result_count = std::min(match_count, _top_k);
    for (std::size_t index = 0; index < result_count; ++index) {
        _result.matches_.push_back(std::move(matches[index]));
    }
    return {};
}

std::uint64_t exact_embedding_index::vqec_vision_ai_ports_emidx_revision()
    const noexcept {
    return revision_;
}

}  // namespace vqec::vision::ai
