#include "vqec/vision/ai/contracts/vqec_vision_deployment_config.hpp"

#include <limits>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_inference_plan.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_core_dpval_is_identifier(const std::string& _value) noexcept {
    return vqec_vision_ai_cntr_ident_is_valid(
        _value, deployment_limits::g_max_identifier_bytes);
}

bool vqec_vision_ai_core_dpval_add_bytes(
    std::uint64_t _bytes, std::uint64_t& _total) noexcept {
    if (_bytes > std::numeric_limits<std::uint64_t>::max() - _total) {
        return false;
    }
    _total += _bytes;
    return true;
}

bool vqec_vision_ai_core_dpval_multiply_bytes(
    std::uint64_t _bytes, unsigned _count, std::uint64_t& _product) noexcept {
    if (_count != 0 &&
        _bytes > std::numeric_limits<std::uint64_t>::max() / _count) {
        return false;
    }
    _product = _bytes * _count;
    return true;
}

}  // namespace

status vqec_vision_ai_core_dpval_validate_deployment(
    const deployment_config& _config, std::uint64_t& _declared_resident_bytes) {
    if (_config.schema_version_ != deployment_limits::g_schema_version) {
        return {status_code::unsupported, "unsupported deployment schema version"};
    }
    if (_config.revision_ == 0 ||
        !vqec_vision_ai_core_dpval_is_identifier(_config.model_catalog_ref_) ||
        _config.sources_.empty() ||
        _config.sources_.size() > deployment_limits::g_max_sources ||
        _config.max_total_resident_bytes_ == 0 ||
        _config.max_total_resident_bytes_ > deployment_limits::g_max_total_resident_bytes ||
        _config.max_model_resident_bytes_ == 0 ||
        _config.max_model_resident_bytes_ > _config.max_total_resident_bytes_) {
        return {status_code::invalid_argument, "invalid deployment identity or global budget"};
    }
    std::uint64_t total = _config.max_model_resident_bytes_;
    for (std::size_t index = 0; index < _config.sources_.size(); ++index) {
        const auto& source = _config.sources_[index];
        const auto& profile = source.profile_;
        const auto& memory = source.memory_;
        if (!vqec_vision_ai_core_dpval_is_identifier(source.source_id_) ||
            !vqec_vision_ai_core_dpval_is_identifier(source.raw_source_ref_) ||
            profile.width_ == 0 || profile.height_ == 0 ||
            profile.width_ > deployment_limits::g_max_dimension_pixels ||
            profile.height_ > deployment_limits::g_max_dimension_pixels ||
            profile.width_ % 2 != 0 || profile.height_ % 2 != 0 ||
            profile.fps_numerator_ == 0 || profile.fps_denominator_ == 0 ||
            static_cast<std::uint64_t>(profile.fps_numerator_) >
                static_cast<std::uint64_t>(deployment_limits::g_max_frames_per_second) *
                    profile.fps_denominator_ ||
            memory.max_frame_allocation_bytes_ == 0 ||
            memory.max_frame_allocation_bytes_ >
                deployment_limits::g_max_frame_allocation_bytes ||
            memory.max_inflight_frames_ == 0 ||
            memory.max_inflight_frames_ >
                deployment_limits::g_max_inflight_frames_per_source ||
            memory.preview_surface_count_ >
                deployment_limits::g_max_preview_surfaces_per_source ||
            memory.max_tensor_bytes_ == 0 ||
            memory.max_tensor_bytes_ > deployment_limits::g_max_tensor_bytes_per_source ||
            memory.max_temporal_bytes_ > deployment_limits::g_max_temporal_bytes_per_source ||
            source.model_ids_.empty() ||
            source.model_ids_.size() > deployment_limits::g_max_models_per_source) {
            return {status_code::invalid_argument, "invalid source profile or resource budget"};
        }
        const auto packed_pixels = static_cast<std::uint64_t>(profile.width_) * profile.height_;
        const auto packed_nv12_bytes = packed_pixels + packed_pixels / 2;
        const bool has_valid_preview =
            vqec_vision_ai_core_dpval_is_identifier(source.preview_output_ref_);
        if (memory.max_frame_allocation_bytes_ < packed_nv12_bytes ||
            (memory.preview_surface_count_ != 0
                 ? !has_valid_preview
                 : !source.preview_output_ref_.empty())) {
            return {status_code::invalid_argument,
                    "source preview output or frame budget mismatch"};
        }
        for (std::size_t other = 0; other < index; ++other) {
            const auto& previous = _config.sources_[other];
            if (previous.source_id_ == source.source_id_ ||
                previous.raw_source_ref_ == source.raw_source_ref_ ||
                (previous.camera_id_ == source.camera_id_ &&
                 previous.channel_id_ == source.channel_id_)) {
                return {status_code::invalid_argument, "duplicate source identity"};
            }
            if (!source.preview_output_ref_.empty() &&
                previous.preview_output_ref_ == source.preview_output_ref_) {
                return {status_code::invalid_argument,
                        "duplicate preview output"};
            }
        }
        for (std::size_t model = 0; model < source.model_ids_.size(); ++model) {
            if (!vqec_vision_ai_core_dpval_is_identifier(source.model_ids_[model])) {
                return {status_code::invalid_argument, "invalid model reference"};
            }
            for (std::size_t previous = 0; previous < model; ++previous) {
                if (source.model_ids_[previous] == source.model_ids_[model]) {
                    return {status_code::invalid_argument, "duplicate per-source model reference"};
                }
            }
        }
        std::uint64_t camera_bytes = 0;
        std::uint64_t preview_bytes = 0;
        if (!vqec_vision_ai_core_dpval_multiply_bytes(
                memory.max_frame_allocation_bytes_, memory.max_inflight_frames_,
                camera_bytes) ||
            !vqec_vision_ai_core_dpval_multiply_bytes(
                packed_nv12_bytes, memory.preview_surface_count_, preview_bytes) ||
            !vqec_vision_ai_core_dpval_add_bytes(camera_bytes, total) ||
            !vqec_vision_ai_core_dpval_add_bytes(preview_bytes, total) ||
            !vqec_vision_ai_core_dpval_add_bytes(memory.max_tensor_bytes_, total) ||
            !vqec_vision_ai_core_dpval_add_bytes(memory.max_temporal_bytes_, total) ||
            total > _config.max_total_resident_bytes_) {
            return {status_code::resource_exhausted, "declared multi-source memory exceeds budget"};
        }
    }
    _declared_resident_bytes = total;
    return {};
}

status vqec_vision_ai_core_dpval_bind_source_profile(
    const source_deployment_config& _source, inference_plan& _plan) {
    inference_plan candidate = _plan;
    candidate.source_width_ = _source.profile_.width_;
    candidate.source_height_ = _source.profile_.height_;
    candidate.fps_numerator_ = _source.profile_.fps_numerator_;
    candidate.fps_denominator_ = _source.profile_.fps_denominator_;
    const auto valid = vqec_vision_ai_core_infpl_validate_plan(candidate);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    _plan = std::move(candidate);
    return {};
}

}  // namespace vqec::vision::ai
