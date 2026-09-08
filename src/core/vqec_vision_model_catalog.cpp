#include "vqec/vision/ai/contracts/vqec_vision_model_catalog.hpp"

#include <cmath>
#include <limits>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_core_mdcat_is_identifier(const std::string& _value) noexcept {
    if (_value.empty() || _value.size() > model_catalog_limits::g_max_identifier_bytes) {
        return false;
    }
    for (const unsigned char character : _value) {
        if (!((character >= 'a' && character <= 'z') ||
              (character >= 'A' && character <= 'Z') ||
              (character >= '0' && character <= '9') || character == '_' ||
              character == '-' || character == '.' || character == ':')) {
            return false;
        }
    }
    return true;
}

bool vqec_vision_ai_core_mdcat_is_digest(const std::string& _value) noexcept {
    return _value.size() == 64 &&
        _value.find_first_not_of("0123456789abcdef") == std::string::npos;
}

bool vqec_vision_ai_core_mdcat_add_bytes(
    std::uint64_t _bytes, std::uint64_t& _total) noexcept {
    if (_bytes > std::numeric_limits<std::uint64_t>::max() - _total) {
        return false;
    }
    _total += _bytes;
    return true;
}

bool vqec_vision_ai_core_mdcat_multiply_bytes(
    std::uint64_t _bytes, unsigned _count, std::uint64_t& _total) noexcept {
    if (_count != 0 &&
        _bytes > std::numeric_limits<std::uint64_t>::max() / _count) {
        return false;
    }
    return vqec_vision_ai_core_mdcat_add_bytes(_bytes * _count, _total);
}

bool vqec_vision_ai_core_mdcat_is_rate_at_most(
    std::uint32_t _left_numerator, std::uint32_t _left_denominator,
    std::uint32_t _right_numerator, std::uint32_t _right_denominator) noexcept {
    return static_cast<std::uint64_t>(_left_numerator) * _right_denominator <=
        static_cast<std::uint64_t>(_right_numerator) * _left_denominator;
}

status vqec_vision_ai_core_mdcat_validate_entry(const model_catalog_entry& _model) {
    const auto& constraints = _model.source_constraints_;
    const auto& resources = _model.resources_;
    if (!vqec_vision_ai_core_mdcat_is_identifier(_model.model_id_) ||
        !vqec_vision_ai_core_mdcat_is_identifier(_model.model_version_) ||
        !vqec_vision_ai_core_mdcat_is_identifier(_model.target_id_) ||
        !vqec_vision_ai_core_mdcat_is_identifier(_model.artifact_ref_) ||
        !vqec_vision_ai_core_mdcat_is_digest(_model.artifact_sha256_) ||
        !vqec_vision_ai_core_mdcat_is_identifier(_model.output_manifest_ref_) ||
        !vqec_vision_ai_core_mdcat_is_identifier(_model.decoder_contract_) ||
        !vqec_vision_ai_core_mdcat_is_identifier(_model.preprocess_contract_) ||
        !vqec_vision_ai_core_mdcat_is_identifier(_model.graph_name_)) {
        return {status_code::invalid_argument, "invalid model catalog identity"};
    }
    if (_model.tensor_width_ < inference_limits::g_min_tensor_dimension ||
        _model.tensor_height_ < inference_limits::g_min_tensor_dimension ||
        _model.tensor_width_ > inference_limits::g_max_tensor_dimension ||
        _model.tensor_height_ > inference_limits::g_max_tensor_dimension ||
        (_model.input_type_ != tensor_type::uint8 &&
         _model.input_type_ != tensor_type::float32) ||
        (_model.channel_order_ != channel_order::rgb &&
         _model.channel_order_ != channel_order::bgr) ||
        (_model.placement_ != image_placement::top_left &&
         _model.placement_ != image_placement::centre &&
         _model.placement_ != image_placement::stretch)) {
        return {status_code::unsupported, "unsupported model input/preprocess profile"};
    }
    for (std::size_t channel = 0; channel < _model.mean_.size(); ++channel) {
        if (!std::isfinite(_model.mean_[channel]) || _model.mean_[channel] < 0.0 ||
            _model.mean_[channel] > 255.0 || !std::isfinite(_model.sigma_[channel]) ||
            _model.sigma_[channel] <= 0.0 || _model.sigma_[channel] > 255.0) {
            return {status_code::invalid_argument, "invalid model normalization"};
        }
    }
    if (_model.input_type_ == tensor_type::uint8 &&
        (_model.mean_ != std::array<double, 3>{0.0, 0.0, 0.0} ||
         _model.sigma_ != std::array<double, 3>{1.0, 1.0, 1.0})) {
        return {status_code::unsupported, "UINT8 custom normalization is not qualified"};
    }
    if (_model.inference_fps_numerator_ == 0 ||
        _model.inference_fps_denominator_ == 0 ||
        _model.inference_fps_numerator_ > inference_limits::g_max_fps_numerator ||
        _model.inference_fps_denominator_ > inference_limits::g_max_fps_denominator ||
        !vqec_vision_ai_core_mdcat_is_rate_at_most(
            _model.inference_fps_numerator_, _model.inference_fps_denominator_,
            inference_limits::g_max_frames_per_second, 1)) {
        return {status_code::invalid_argument, "invalid model inference cadence"};
    }
    if (constraints.min_width_ == 0 || constraints.min_height_ == 0 ||
        constraints.max_width_ < constraints.min_width_ ||
        constraints.max_height_ < constraints.min_height_ ||
        constraints.max_width_ > deployment_limits::g_max_dimension_pixels ||
        constraints.max_height_ > deployment_limits::g_max_dimension_pixels ||
        constraints.min_width_ % 2 != 0 || constraints.min_height_ % 2 != 0 ||
        constraints.max_width_ % 2 != 0 || constraints.max_height_ % 2 != 0 ||
        constraints.min_fps_numerator_ == 0 || constraints.min_fps_denominator_ == 0 ||
        constraints.min_fps_numerator_ > inference_limits::g_max_fps_numerator ||
        constraints.min_fps_denominator_ > inference_limits::g_max_fps_denominator ||
        !vqec_vision_ai_core_mdcat_is_rate_at_most(
            constraints.min_fps_numerator_, constraints.min_fps_denominator_,
            inference_limits::g_max_frames_per_second, 1)) {
        return {status_code::invalid_argument, "invalid supported source envelope"};
    }
    if (resources.resident_bytes_ == 0 ||
        resources.resident_bytes_ > model_catalog_limits::g_max_model_resident_bytes ||
        resources.max_tensor_bytes_per_source_ == 0 ||
        resources.max_tensor_bytes_per_source_ >
            deployment_limits::g_max_tensor_bytes_per_source ||
        resources.output_queue_buffers_ == 0 ||
        resources.output_queue_buffers_ > inference_limits::g_max_output_queue_buffers ||
        resources.max_concurrent_sources_ == 0 ||
        resources.max_concurrent_sources_ > deployment_limits::g_max_sources) {
        return {status_code::invalid_argument, "invalid model resource profile"};
    }
    const std::uint64_t element_bytes =
        _model.input_type_ == tensor_type::float32 ? sizeof(float) : sizeof(std::uint8_t);
    const auto minimum_input_bytes = static_cast<std::uint64_t>(_model.tensor_width_) *
        _model.tensor_height_ * 3 * element_bytes;
    if (resources.max_tensor_bytes_per_source_ < minimum_input_bytes) {
        return {status_code::resource_exhausted,
                "model tensor budget cannot hold the declared input"};
    }
    return {};
}

const model_catalog_entry* vqec_vision_ai_core_mdcat_find_model(
    const model_catalog& _catalog, const std::string& _model_id) noexcept {
    for (const auto& model : _catalog.models_) {
        if (model.model_id_ == _model_id) {
            return &model;
        }
    }
    return nullptr;
}

bool vqec_vision_ai_core_mdcat_source_supports_model(
    const source_deployment_config& _source, const model_catalog_entry& _model) noexcept {
    const auto& profile = _source.profile_;
    const auto& constraints = _model.source_constraints_;
    return profile.width_ >= constraints.min_width_ &&
        profile.width_ <= constraints.max_width_ &&
        profile.height_ >= constraints.min_height_ &&
        profile.height_ <= constraints.max_height_ &&
        vqec_vision_ai_core_mdcat_is_rate_at_most(
            constraints.min_fps_numerator_, constraints.min_fps_denominator_,
            profile.fps_numerator_, profile.fps_denominator_) &&
        vqec_vision_ai_core_mdcat_is_rate_at_most(
            _model.inference_fps_numerator_, _model.inference_fps_denominator_,
            profile.fps_numerator_, profile.fps_denominator_);
}

}  // namespace

status vqec_vision_ai_core_mdcat_validate_catalog(
    const model_catalog& _catalog, std::uint64_t& _declared_resident_bytes) {
    if (_catalog.schema_version_ != model_catalog_limits::g_schema_version) {
        return {status_code::unsupported, "unsupported model catalog schema"};
    }
    if (_catalog.revision_ == 0 ||
        !vqec_vision_ai_core_mdcat_is_identifier(_catalog.catalog_id_) ||
        _catalog.models_.empty() ||
        _catalog.models_.size() > model_catalog_limits::g_max_models) {
        return {status_code::invalid_argument, "invalid model catalog root"};
    }
    std::uint64_t resident_bytes = 0;
    for (std::size_t index = 0; index < _catalog.models_.size(); ++index) {
        const auto& model = _catalog.models_[index];
        const auto valid = vqec_vision_ai_core_mdcat_validate_entry(model);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (_catalog.models_[previous].model_id_ == model.model_id_ ||
                _catalog.models_[previous].artifact_ref_ == model.artifact_ref_ ||
                _catalog.models_[previous].output_manifest_ref_ ==
                    model.output_manifest_ref_) {
                return {status_code::invalid_argument, "duplicate model catalog identity"};
            }
        }
        if (!vqec_vision_ai_core_mdcat_add_bytes(
                model.resources_.resident_bytes_, resident_bytes) ||
            resident_bytes > model_catalog_limits::g_max_catalog_resident_bytes) {
            return {status_code::resource_exhausted, "catalog resident declaration too large"};
        }
    }
    _declared_resident_bytes = resident_bytes;
    return {};
}

status vqec_vision_ai_core_mdcat_validate_deployment_models(
    const deployment_config& _deployment, const model_catalog& _catalog,
    std::uint64_t& _required_model_resident_bytes) {
    std::uint64_t deployment_bytes = 0;
    const auto valid_deployment =
        vqec_vision_ai_core_dpval_validate_deployment(_deployment, deployment_bytes);
    if (valid_deployment.code_ != status_code::ok) {
        return valid_deployment;
    }
    std::uint64_t catalog_bytes = 0;
    const auto valid_catalog =
        vqec_vision_ai_core_mdcat_validate_catalog(_catalog, catalog_bytes);
    if (valid_catalog.code_ != status_code::ok) {
        return valid_catalog;
    }
    if (_deployment.model_catalog_ref_ != _catalog.catalog_id_) {
        return {status_code::invalid_argument, "deployment references another model catalog"};
    }
    std::uint64_t required_resident = 0;
    for (const auto& model : _catalog.models_) {
        unsigned assignments = 0;
        for (const auto& source : _deployment.sources_) {
            for (const auto& model_id : source.model_ids_) {
                if (model_id == model.model_id_) {
                    ++assignments;
                }
            }
        }
        if (assignments > model.resources_.max_concurrent_sources_) {
            return {status_code::resource_exhausted, "model source concurrency exceeded"};
        }
        const unsigned resident_instances =
            model.resources_.can_share_context_across_sources_ && assignments != 0
                ? 1U
                : assignments;
        if (!vqec_vision_ai_core_mdcat_multiply_bytes(
                model.resources_.resident_bytes_, resident_instances,
                required_resident)) {
            return {status_code::resource_exhausted, "model resident arithmetic overflow"};
        }
    }
    for (const auto& source : _deployment.sources_) {
        std::uint64_t tensor_bytes = 0;
        for (const auto& model_id : source.model_ids_) {
            const auto* model = vqec_vision_ai_core_mdcat_find_model(_catalog, model_id);
            if (model == nullptr) {
                return {status_code::invalid_argument, "deployment references unknown model"};
            }
            if (!vqec_vision_ai_core_mdcat_source_supports_model(source, *model)) {
                return {status_code::unsupported, "source profile is incompatible with model"};
            }
            if (!vqec_vision_ai_core_mdcat_add_bytes(
                    model->resources_.max_tensor_bytes_per_source_, tensor_bytes)) {
                return {status_code::resource_exhausted, "source tensor arithmetic overflow"};
            }
        }
        if (tensor_bytes > source.memory_.max_tensor_bytes_) {
            return {status_code::resource_exhausted, "source model tensors exceed budget"};
        }
    }
    if (required_resident > _deployment.max_model_resident_bytes_) {
        return {status_code::resource_exhausted, "model contexts exceed deployment budget"};
    }
    _required_model_resident_bytes = required_resident;
    return {};
}

status vqec_vision_ai_core_mdcat_validate_model_outputs(
    const model_catalog_entry& _model, const std::string& _resolved_manifest_ref,
    const model_outputs& _outputs, std::uint64_t& _required_output_bytes) {
    const auto valid_model = vqec_vision_ai_core_mdcat_validate_entry(_model);
    if (valid_model.code_ != status_code::ok) {
        return valid_model;
    }
    if (_resolved_manifest_ref != _model.output_manifest_ref_ ||
        _outputs.model_id_ != _model.model_id_ ||
        _outputs.model_version_ != _model.model_version_ ||
        _outputs.artifact_sha256_ != _model.artifact_sha256_ ||
        _outputs.decoder_contract_ != _model.decoder_contract_) {
        return {status_code::invalid_argument, "model output manifest binding mismatch"};
    }
    std::uint64_t required_bytes = 0;
    const auto valid_outputs = vqec_vision_ai_core_tnctr_validate_outputs(
        _outputs.outputs_, _outputs.max_output_bytes_, required_bytes);
    if (valid_outputs.code_ != status_code::ok) {
        return valid_outputs;
    }
    if (required_bytes > _model.resources_.max_tensor_bytes_per_source_) {
        return {status_code::resource_exhausted,
                "model outputs exceed declared per-source tensor budget"};
    }
    _required_output_bytes = required_bytes;
    return {};
}

status vqec_vision_ai_core_mdcat_compose_inference_plan(
    const source_deployment_config& _source, const model_catalog_entry& _model,
    const resolved_model_paths& _paths, inference_plan& _plan) {
    bool is_assigned = false;
    for (const auto& model_id : _source.model_ids_) {
        if (model_id == _model.model_id_) {
            is_assigned = true;
            break;
        }
    }
    const auto valid_model = vqec_vision_ai_core_mdcat_validate_entry(_model);
    if (valid_model.code_ != status_code::ok) {
        return valid_model;
    }
    if (!vqec_vision_ai_core_mdcat_source_supports_model(_source, _model)) {
        return {status_code::unsupported, "source profile is incompatible with model"};
    }
    if (!is_assigned || _source.memory_.max_tensor_bytes_ <
            _model.resources_.max_tensor_bytes_per_source_ ||
        _paths.model_id_ != _model.model_id_ ||
        _paths.target_id_ != _model.target_id_ ||
        _paths.artifact_ref_ != _model.artifact_ref_) {
        return {status_code::invalid_argument,
                "model assignment, budget or path binding mismatch"};
    }
    inference_plan candidate;
    candidate.source_width_ = _source.profile_.width_;
    candidate.source_height_ = _source.profile_.height_;
    candidate.fps_numerator_ = _source.profile_.fps_numerator_;
    candidate.fps_denominator_ = _source.profile_.fps_denominator_;
    candidate.tensor_width_ = _model.tensor_width_;
    candidate.tensor_height_ = _model.tensor_height_;
    candidate.input_type_ = _model.input_type_;
    candidate.channel_order_ = _model.channel_order_;
    candidate.placement_ = _model.placement_;
    candidate.mean_ = _model.mean_;
    candidate.sigma_ = _model.sigma_;
    candidate.model_path_ = _paths.model_path_;
    candidate.backend_path_ = _paths.backend_path_;
    candidate.system_path_ = _paths.system_path_;
    candidate.input_queue_bytes_ = _source.memory_.max_frame_allocation_bytes_;
    candidate.output_queue_buffers_ = _model.resources_.output_queue_buffers_;
    const auto valid_plan = vqec_vision_ai_core_infpl_validate_plan(candidate);
    if (valid_plan.code_ != status_code::ok) {
        return valid_plan;
    }
    _plan = std::move(candidate);
    return {};
}

}  // namespace vqec::vision::ai
