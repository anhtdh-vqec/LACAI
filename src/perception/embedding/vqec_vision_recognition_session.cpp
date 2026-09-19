#include "vqec_vision_recognition_session.hpp"

#include <algorithm>
#include <cmath>
#include <new>
#include <utility>

#include "vqec/vision/ai/contracts/base/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_embed_rcses_valid_revision_change(
    std::uint64_t _current, std::uint64_t _expected) noexcept {
    return _current == _expected && _current != UINT64_MAX;
}

status vqec_vision_ai_embed_rcses_validate_config(
    const recognition_session_config& _config) {
    const auto policy_status =
        vqec_vision_ai_embed_rcpol_validate_config(_config.policy_);
    if (policy_status.code_ != status_code::ok ||
        _config.index_.capacity_ == 0 ||
        _config.max_templates_per_subject_ == 0 ||
        _config.max_templates_per_subject_ > _config.index_.capacity_ ||
        _config.search_top_k_ == 0 ||
        _config.search_top_k_ > _config.index_.max_results_ ||
        _config.search_top_k_ > _config.policy_.max_subjects_ ||
        !std::isfinite(_config.search_minimum_similarity_) ||
        _config.search_minimum_similarity_ < -1.0F ||
        _config.search_minimum_similarity_ > _config.policy_.minimum_similarity_) {
        return {status_code::invalid_argument,
            "recognition session configuration is invalid"};
    }
    return {};
}

}  // namespace

status recognition_session::vqec_vision_ai_embed_rcses_configure(
    embedding_index_port& _index, const recognition_session_config& _config) {
    if (is_configured_) {
        return {status_code::invalid_state, "recognition session is already configured"};
    }
    const auto config_status = vqec_vision_ai_embed_rcses_validate_config(_config);
    if (config_status.code_ != status_code::ok) {
        return config_status;
    }
    try {
        std::vector<template_metadata> metadata;
        metadata.reserve(_config.index_.capacity_);
        recognition_session_config candidate_config = _config;
        const auto configured = _index.vqec_vision_ai_ports_emidx_configure(_config.index_);
        if (configured.code_ != status_code::ok) {
            return configured;
        }
        index_ = &_index;
        config_ = std::move(candidate_config);
        templates_ = std::move(metadata);
        next_record_id_ = 1;
        is_configured_ = true;
        return {};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "recognition session metadata allocation failed"};
    }
}

status recognition_session::vqec_vision_ai_embed_rcses_configure_persistent(
    embedding_index_port& _index, face_gallery_store_port& _store,
    const recognition_session_config& _config,
    const face_gallery_config& _gallery_config) {
    if (is_configured_) {
        return {status_code::invalid_state, "recognition session is already configured"};
    }
    const auto config_status = vqec_vision_ai_embed_rcses_validate_config(_config);
    const auto gallery_status =
        vqec_vision_ai_core_fgalr_validate_config(_gallery_config);
    if (config_status.code_ != status_code::ok) {
        return config_status;
    }
    if (gallery_status.code_ != status_code::ok ||
        _gallery_config.model_id_ != _config.index_.model_id_ ||
        _gallery_config.model_version_ != _config.index_.model_version_ ||
        _gallery_config.dimensions_ != _config.index_.dimensions_ ||
        _gallery_config.capacity_ != _config.index_.capacity_ ||
        _gallery_config.max_templates_per_subject_ !=
            _config.max_templates_per_subject_) {
        return {status_code::invalid_argument,
            "recognition gallery and index configuration differ"};
    }

    face_gallery_snapshot gallery;
    const auto loaded = _store.vqec_vision_ai_ports_fgstr_load(
        _gallery_config, gallery);
    if (loaded.code_ != status_code::ok) {
        return loaded;
    }
    const auto validated =
        vqec_vision_ai_core_fgalr_validate_snapshot(_gallery_config, gallery);
    if (validated.code_ != status_code::ok) {
        return validated;
    }
    if (gallery.revision_ <= gallery.templates_.size()) {
        return {status_code::invalid_state,
            "face gallery revision cannot seed an index rebuild"};
    }

    recognition_session_config candidate_config;
    face_gallery_config candidate_gallery_config;
    std::vector<template_metadata> metadata;
    embedding_index_config index_config;
    try {
        candidate_config = _config;
        candidate_gallery_config = _gallery_config;
        metadata.reserve(_config.index_.capacity_);
        for (const auto& item : gallery.templates_) {
            metadata.push_back({item.record_id_, item.subject_ref_});
        }
        index_config = _config.index_;
        index_config.initial_revision_ = gallery.revision_ - gallery.templates_.size();
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "recognition recovery allocation failed"};
    }
    auto rebuilt = _index.vqec_vision_ai_ports_emidx_configure(index_config);
    auto revision = index_config.initial_revision_;
    for (const auto& item : gallery.templates_) {
        if (rebuilt.code_ != status_code::ok) {
            break;
        }
        embedding_gallery_record record{item.record_id_, item.subject_ref_, item.values_};
        rebuilt = _index.vqec_vision_ai_ports_emidx_upsert(
            record, revision, revision + 1U);
        ++revision;
    }
    if (rebuilt.code_ != status_code::ok || revision != gallery.revision_ ||
        _index.vqec_vision_ai_ports_emidx_revision() != gallery.revision_) {
        return rebuilt.code_ == status_code::ok
            ? status{status_code::invalid_state,
                  "rebuilt embedding index revision differs from gallery"}
            : rebuilt;
    }

    index_ = &_index;
    store_ = &_store;
    config_ = std::move(candidate_config);
    gallery_config_ = std::move(candidate_gallery_config);
    gallery_ = std::move(gallery);
    templates_ = std::move(metadata);
    next_record_id_ = gallery_.next_record_id_;
    is_configured_ = true;
    return {};
}

status recognition_session::vqec_vision_ai_embed_rcses_add_template(
    const std::string& _subject_ref, const embedding_result& _embedding,
    std::uint64_t _expected_revision, std::uint64_t& _record_id,
    std::uint64_t& _new_revision) {
    if (!is_configured_ || is_faulted_ || index_ == nullptr) {
        return {status_code::invalid_state, "recognition session is unavailable"};
    }
    const auto revision = index_->vqec_vision_ai_ports_emidx_revision();
    if (!vqec_vision_ai_embed_rcses_valid_revision_change(
            revision, _expected_revision)) {
        return {status_code::invalid_state, "recognition gallery revision conflict"};
    }
    if (revision == 0 || revision >= UINT64_MAX - 1U) {
        return {status_code::invalid_state, "recognition revision space is exhausted"};
    }
    if (vqec_vision_ai_core_embct_validate_result(_embedding, _embedding.frame_).code_ !=
            status_code::ok || !vqec_vision_ai_cntr_ident_is_valid(
            _subject_ref, embedding_index_limits::g_max_subject_ref_bytes) ||
        _embedding.model_id_ != config_.index_.model_id_ ||
        _embedding.model_version_ != config_.index_.model_version_ ||
        _embedding.values_.size() != config_.index_.dimensions_ ||
        !_embedding.is_l2_normalized_ || next_record_id_ == 0 ||
        next_record_id_ == UINT64_MAX) {
        return {status_code::invalid_argument, "recognition template is invalid"};
    }
    const auto subject_templates = std::count_if(templates_.begin(), templates_.end(),
        [&_subject_ref](const template_metadata& _item) {
            return _item.subject_ref_ == _subject_ref;
        });
    if (templates_.size() == config_.index_.capacity_ ||
        static_cast<std::size_t>(subject_templates) ==
            config_.max_templates_per_subject_) {
        return {status_code::resource_exhausted,
            "recognition subject or gallery template limit reached"};
    }
    const std::uint64_t candidate_record_id = next_record_id_;
    const std::uint64_t candidate_revision = revision + 1U;
    embedding_gallery_record record;
    template_metadata metadata;
    face_gallery_snapshot replacement;
    try {
        record = {candidate_record_id, _subject_ref, _embedding.values_};
        metadata = {candidate_record_id, _subject_ref};
        if (store_ != nullptr) {
            replacement = gallery_;
            replacement.revision_ = candidate_revision;
            replacement.next_record_id_ = candidate_record_id + 1U;
            replacement.templates_.push_back(
                {candidate_record_id, _subject_ref, _embedding.values_});
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "recognition template allocation failed"};
    }
    if (store_ != nullptr) {
        const auto validated = vqec_vision_ai_core_fgalr_validate_replacement(
            gallery_config_, revision, replacement);
        if (validated.code_ != status_code::ok) {
            return validated;
        }
        const auto stored = store_->vqec_vision_ai_ports_fgstr_replace(
            gallery_config_, revision, replacement);
        if (stored.code_ != status_code::ok) {
            return stored;
        }
    }
    const auto added = index_->vqec_vision_ai_ports_emidx_upsert(
        record, revision, candidate_revision);
    if (added.code_ != status_code::ok) {
        if (store_ != nullptr ||
            index_->vqec_vision_ai_ports_emidx_revision() != revision) {
            is_faulted_ = true;
        }
        return added;
    }
    // Both string and vector capacity are prepared before mutation; move cannot allocate.
    templates_.push_back(std::move(metadata));
    next_record_id_ = candidate_record_id + 1U;
    if (store_ != nullptr) {
        gallery_ = std::move(replacement);
    }
    _record_id = candidate_record_id;
    _new_revision = candidate_revision;
    return {};
}

status recognition_session::vqec_vision_ai_embed_rcses_remove_subject(
    const std::string& _subject_ref, std::uint64_t _expected_revision,
    std::uint64_t& _new_revision) {
    if (!is_configured_ || is_faulted_ || index_ == nullptr ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _subject_ref, embedding_index_limits::g_max_subject_ref_bytes)) {
        return {status_code::invalid_state, "recognition session or subject is invalid"};
    }
    auto revision = index_->vqec_vision_ai_ports_emidx_revision();
    if (!vqec_vision_ai_embed_rcses_valid_revision_change(
            revision, _expected_revision)) {
        return {status_code::invalid_state, "recognition gallery revision conflict"};
    }
    const auto count = static_cast<std::uint64_t>(std::count_if(
        templates_.begin(), templates_.end(), [&_subject_ref](const template_metadata& _item) {
            return _item.subject_ref_ == _subject_ref;
        }));
    if (revision == 0 || count >= UINT64_MAX - revision) {
        return {status_code::invalid_state, "recognition revision space is exhausted"};
    }
    if (count == 0) {
        return {status_code::invalid_argument, "recognition subject is unknown"};
    }
    face_gallery_snapshot replacement;
    if (store_ != nullptr) {
        try {
            replacement = gallery_;
            replacement.revision_ = revision + count;
            replacement.templates_.erase(std::remove_if(
                replacement.templates_.begin(), replacement.templates_.end(),
                [&_subject_ref](const face_gallery_template& _item) {
                    return _item.subject_ref_ == _subject_ref;
                }), replacement.templates_.end());
        } catch (const std::bad_alloc&) {
            return {status_code::resource_exhausted,
                "recognition removal allocation failed"};
        }
        const auto validated = vqec_vision_ai_core_fgalr_validate_replacement(
            gallery_config_, revision, replacement);
        if (validated.code_ != status_code::ok) {
            return validated;
        }
        const auto stored = store_->vqec_vision_ai_ports_fgstr_replace(
            gallery_config_, revision, replacement);
        if (stored.code_ != status_code::ok) {
            return stored;
        }
    }
    bool removed_any = false;
    for (auto item = templates_.begin(); item != templates_.end();) {
        if (item->subject_ref_ != _subject_ref) {
            ++item;
            continue;
        }
        const auto removed = index_->vqec_vision_ai_ports_emidx_remove(
            item->record_id_, revision, revision + 1U);
        if (removed.code_ != status_code::ok) {
            is_faulted_ = true;
            return removed;
        }
        ++revision;
        item = templates_.erase(item);
        removed_any = true;
    }
    if (!removed_any) {
        return {status_code::invalid_argument, "recognition subject is unknown"};
    }
    if (store_ != nullptr) {
        gallery_ = std::move(replacement);
    }
    _new_revision = revision;
    return {};
}

status recognition_session::vqec_vision_ai_embed_rcses_recognize(
    const embedding_result& _embedding, recognition_match_result& _result) const {
    if (!is_configured_ || is_faulted_ || index_ == nullptr) {
        return {status_code::invalid_state, "recognition session is unavailable"};
    }
    const auto revision = index_->vqec_vision_ai_ports_emidx_revision();
    if (revision == 0) {
        return {status_code::invalid_state, "recognition gallery revision is unavailable"};
    }
    embedding_search_result search;
    try {
        search.matches_.reserve(config_.search_top_k_);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "recognition result allocation failed"};
    }
    const auto searched = index_->vqec_vision_ai_ports_emidx_search(_embedding,
        revision, config_.search_top_k_, config_.search_minimum_similarity_, search);
    if (searched.code_ != status_code::ok) {
        return searched;
    }
    return vqec_vision_ai_embed_rcpol_evaluate(
        search, revision, config_.policy_, _result);
}

status recognition_session::vqec_vision_ai_embed_rcses_recognize_batch(
    const std::vector<embedding_result>& _embeddings,
    std::vector<recognition_match_result>& _results) const {
    if (_embeddings.size() > observation_limits::g_max_observations ||
        _results.capacity() < _embeddings.size()) {
        return {status_code::resource_exhausted,
            "recognition batch exceeds its bounded result capacity"};
    }
    std::vector<recognition_match_result> candidate;
    try {
        candidate.reserve(_embeddings.size());
        for (const auto& embedding : _embeddings) {
            recognition_match_result result;
            const auto recognized = vqec_vision_ai_embed_rcses_recognize(
                embedding, result);
            if (recognized.code_ != status_code::ok) {
                return recognized;
            }
            candidate.push_back(std::move(result));
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "recognition batch allocation failed"};
    }
    _results = std::move(candidate);
    return {};
}

status recognition_session::vqec_vision_ai_embed_rcses_apply_labels(
    const std::vector<recognition_match_result>& _results,
    observation_batch& _observations) const {
    for (const auto& result : _results) {
        if (result.decision_ != recognition_decision::known) {
            continue;
        }
        if (result.gallery_revision_ !=
                vqec_vision_ai_embed_rcses_get_snapshot().gallery_revision_ ||
            result.frame_.camera_id_ != _observations.frame_.camera_id_ ||
            result.frame_.channel_id_ != _observations.frame_.channel_id_ ||
            result.frame_.source_epoch_ != _observations.frame_.source_epoch_ ||
            result.frame_.frame_id_ != _observations.frame_.frame_id_ ||
            result.frame_.source_pts_ns_ != _observations.frame_.source_pts_ns_) {
            return {status_code::invalid_state,
                "recognition label result is stale or belongs to another frame"};
        }
        const auto found = std::find_if(_observations.observations_.begin(),
            _observations.observations_.end(), [&result](const observation& _item) {
                return _item.track_id_ == result.track_id_;
            });
        if (found == _observations.observations_.end()) {
            return {status_code::invalid_argument,
                "recognition label has no correlated observation"};
        }
        found->box_.label_ = result.subject_ref_;
    }
    return {};
}

recognition_session_snapshot
recognition_session::vqec_vision_ai_embed_rcses_get_snapshot() const noexcept {
    recognition_session_snapshot snapshot;
    snapshot.gallery_revision_ = index_ == nullptr ? 0 :
        index_->vqec_vision_ai_ports_emidx_revision();
    snapshot.template_count_ = templates_.size();
    for (std::size_t index = 0; index < templates_.size(); ++index) {
        const bool first = std::none_of(templates_.begin(), templates_.begin() + index,
            [this, index](const template_metadata& _item) {
                return _item.subject_ref_ == templates_[index].subject_ref_;
            });
        snapshot.subject_count_ += first ? 1U : 0U;
    }
    snapshot.is_configured_ = is_configured_;
    snapshot.is_faulted_ = is_faulted_;
    return snapshot;
}

}  // namespace vqec::vision::ai
