#include "vqec/vision/ai/contracts/vqec_vision_face_gallery.hpp"

#include <cmath>
#include <map>
#include <new>
#include <set>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_core_fgalr_is_normalized(const std::vector<float>& _values) noexcept {
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

}  // namespace

status vqec_vision_ai_core_fgalr_validate_config(const face_gallery_config& _config) {
    if (!vqec_vision_ai_cntr_ident_is_valid(
            _config.gallery_id_, embedding_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _config.model_id_, embedding_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _config.model_version_, embedding_limits::g_max_identifier_bytes) ||
        _config.preprocess_revision_ == 0 || _config.dimensions_ == 0 ||
        _config.dimensions_ > embedding_limits::g_max_dimensions ||
        _config.capacity_ == 0 || _config.capacity_ > face_gallery_limits::g_max_records ||
        _config.max_templates_per_subject_ == 0 ||
        _config.max_templates_per_subject_ > _config.capacity_) {
        return {status_code::invalid_argument, "face gallery configuration is invalid"};
    }
    return {};
}

status vqec_vision_ai_core_fgalr_validate_snapshot(
    const face_gallery_config& _config, const face_gallery_snapshot& _snapshot) {
    const auto config_status = vqec_vision_ai_core_fgalr_validate_config(_config);
    if (config_status.code_ != status_code::ok) {
        return config_status;
    }
    if (_snapshot.schema_version_ != face_gallery_limits::g_schema_version ||
        _snapshot.revision_ == 0 || _snapshot.revision_ == UINT64_MAX ||
        _snapshot.next_record_id_ == 0 ||
        _snapshot.gallery_id_ != _config.gallery_id_ ||
        _snapshot.model_id_ != _config.model_id_ ||
        _snapshot.model_version_ != _config.model_version_ ||
        _snapshot.preprocess_revision_ != _config.preprocess_revision_ ||
        _snapshot.dimensions_ != _config.dimensions_ ||
        _snapshot.templates_.size() > _config.capacity_) {
        return {status_code::invalid_argument, "face gallery snapshot identity is invalid"};
    }
    try {
        std::set<std::uint64_t> record_ids;
        std::map<std::string, std::size_t> subject_counts;
        for (const auto& item : _snapshot.templates_) {
            if (item.record_id_ == 0 || item.record_id_ >= _snapshot.next_record_id_ ||
                !record_ids.insert(item.record_id_).second ||
                !vqec_vision_ai_cntr_ident_is_valid(
                    item.subject_ref_, embedding_limits::g_max_identifier_bytes) ||
                item.values_.size() != _config.dimensions_ ||
                !vqec_vision_ai_core_fgalr_is_normalized(item.values_) ||
                ++subject_counts[item.subject_ref_] >
                    _config.max_templates_per_subject_) {
                return {status_code::invalid_argument, "face gallery template is invalid"};
            }
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "face gallery validation allocation failed"};
    }
    return {};
}

status vqec_vision_ai_core_fgalr_validate_replacement(
    const face_gallery_config& _config, std::uint64_t _expected_revision,
    const face_gallery_snapshot& _replacement) {
    if (_expected_revision == 0 || _expected_revision == UINT64_MAX ||
        _replacement.revision_ <= _expected_revision) {
        return {status_code::invalid_state, "face gallery replacement revision is stale"};
    }
    return vqec_vision_ai_core_fgalr_validate_snapshot(_config, _replacement);
}

}  // namespace vqec::vision::ai
