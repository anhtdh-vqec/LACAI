#include "vqec/vision/ai/contracts/vqec_vision_recognition.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <new>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

struct subject_candidate {
    std::string subject_ref_;
    float similarity_{0.0F};
};

bool vqec_vision_ai_embed_rcpol_is_finite_score(float _score) noexcept {
    return std::isfinite(_score) && _score >= -1.0F && _score <= 1.0F;
}

}  // namespace

status vqec_vision_ai_embed_rcpol_validate_config(
    const recognition_policy_config& _config) {
    if (!std::isfinite(_config.minimum_similarity_) ||
        _config.minimum_similarity_ < -1.0F || _config.minimum_similarity_ > 1.0F ||
        !std::isfinite(_config.minimum_subject_margin_) ||
        _config.minimum_subject_margin_ < 0.0F || _config.minimum_subject_margin_ > 2.0F ||
        _config.max_subjects_ == 0 ||
        _config.max_subjects_ > recognition_limits::g_max_subjects) {
        return {status_code::invalid_argument, "recognition policy configuration is invalid"};
    }
    return {};
}

status vqec_vision_ai_embed_rcpol_evaluate(
    const embedding_search_result& _search, std::uint64_t _required_revision,
    const recognition_policy_config& _config, recognition_match_result& _result) {
    const auto config_status = vqec_vision_ai_embed_rcpol_validate_config(_config);
    if (config_status.code_ != status_code::ok) {
        return config_status;
    }
    if (_required_revision == 0 || _required_revision == UINT64_MAX ||
        _search.gallery_revision_ != _required_revision ||
        _search.matches_.size() > embedding_index_limits::g_max_results) {
        return {status_code::invalid_state, "recognition search revision is unavailable"};
    }

    std::array<subject_candidate, recognition_limits::g_max_subjects> candidates{};
    std::size_t candidate_count = 0;
    try {
        for (std::size_t match_index = 0; match_index < _search.matches_.size();
             ++match_index) {
            const auto& match = _search.matches_[match_index];
            if (match.record_id_ == 0 ||
                !vqec_vision_ai_cntr_ident_is_valid(
                    match.subject_ref_, embedding_index_limits::g_max_subject_ref_bytes) ||
                !vqec_vision_ai_embed_rcpol_is_finite_score(match.similarity_)) {
                return {status_code::protocol_error, "recognition candidate is invalid"};
            }
            bool duplicate_record = false;
            for (std::size_t previous_index = 0; previous_index < match_index;
                 ++previous_index) {
                duplicate_record = duplicate_record ||
                    _search.matches_[previous_index].record_id_ == match.record_id_;
            }
            if (duplicate_record) {
                return {status_code::protocol_error,
                    "recognition search returned a duplicate record"};
            }
            auto found = std::find_if(candidates.begin(), candidates.begin() + candidate_count,
                [&match](const subject_candidate& _candidate) {
                    return _candidate.subject_ref_ == match.subject_ref_;
                });
            if (found == candidates.begin() + candidate_count) {
                if (candidate_count == _config.max_subjects_) {
                    return {status_code::resource_exhausted,
                        "recognition subject candidate limit exceeded"};
                }
                found = candidates.begin() + candidate_count++;
                found->subject_ref_ = match.subject_ref_;
                found->similarity_ = match.similarity_;
            } else if (match.similarity_ > found->similarity_) {
                found->similarity_ = match.similarity_;
            }
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "recognition candidate allocation failed"};
    }

    recognition_match_result evaluated;
    evaluated.frame_ = _search.frame_;
    evaluated.track_id_ = _search.track_id_;
    evaluated.gallery_revision_ = _search.gallery_revision_;
    if (candidate_count == 0) {
        evaluated.decision_ = recognition_decision::unknown;
        _result = std::move(evaluated);
        return {};
    }
    std::sort(candidates.begin(), candidates.begin() + candidate_count,
        [](const subject_candidate& _left, const subject_candidate& _right) {
            return _left.similarity_ > _right.similarity_ ||
                (_left.similarity_ == _right.similarity_ &&
                    _left.subject_ref_ < _right.subject_ref_);
        });
    evaluated.best_similarity_ = candidates[0].similarity_;
    if (candidate_count > 1) {
        evaluated.subject_margin_ = candidates[0].similarity_ - candidates[1].similarity_;
    }
    if (evaluated.best_similarity_ < _config.minimum_similarity_) {
        evaluated.decision_ = recognition_decision::unknown;
    } else if (candidate_count > 1 &&
               evaluated.subject_margin_ < _config.minimum_subject_margin_) {
        evaluated.decision_ = recognition_decision::ambiguous;
    } else {
        evaluated.decision_ = recognition_decision::known;
        evaluated.subject_ref_ = std::move(candidates[0].subject_ref_);
    }
    _result = std::move(evaluated);
    return {};
}

}  // namespace vqec::vision::ai
