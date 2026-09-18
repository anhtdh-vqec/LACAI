#include "vqec/vision/ai/contracts/vqec_vision_model_package_registry.hpp"

#include <algorithm>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_core_mprgy_is_valid_path(const std::string& _path) noexcept {
    return !_path.empty() && _path.size() <= model_package_registry_limits::g_max_path_bytes &&
        _path.find('\0') == std::string::npos;
}

}  // namespace

const model_package_binding* vqec_vision_ai_core_mprgy_find_binding(
    const model_package_registry& _registry, const std::string& _model_id) noexcept {
    const auto found = std::find_if(
        _registry.bindings_.begin(), _registry.bindings_.end(),
        [&_model_id](const model_package_binding& _binding) {
            return _binding.model_id_ == _model_id;
        });
    return found != _registry.bindings_.end() ? &*found : nullptr;
}

status vqec_vision_ai_core_mprgy_validate_registry(
    const model_package_registry& _registry, const model_catalog& _catalog) {
    if (_registry.schema_version_ != model_package_registry_limits::g_schema_version ||
        _registry.bindings_.empty() ||
        _registry.bindings_.size() > model_package_registry_limits::g_max_bindings ||
        _registry.bindings_.size() != _catalog.models_.size()) {
        return {status_code::invalid_argument, "model package registry envelope is invalid"};
    }
    for (std::size_t index = 0; index < _registry.bindings_.size(); ++index) {
        const auto& binding = _registry.bindings_[index];
        if (!vqec_vision_ai_cntr_ident_is_valid(
                binding.model_id_, model_package_registry_limits::g_max_identifier_bytes) ||
            !vqec_vision_ai_cntr_ident_is_valid(
                binding.model_version_, model_package_registry_limits::g_max_identifier_bytes) ||
            !vqec_vision_ai_cntr_ident_is_valid(
                binding.target_id_, model_package_registry_limits::g_max_identifier_bytes) ||
            !vqec_vision_ai_cntr_ident_is_valid(
                binding.artifact_ref_, model_package_registry_limits::g_max_identifier_bytes) ||
            !vqec_vision_ai_core_mprgy_is_valid_path(binding.package_dir_) ||
            !vqec_vision_ai_core_mprgy_is_valid_path(binding.model_library_)) {
            return {status_code::invalid_argument, "model package binding is invalid"};
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (_registry.bindings_[previous].model_id_ == binding.model_id_) {
                return {status_code::invalid_argument, "duplicate model package binding"};
            }
        }
    }
    for (const auto& model : _catalog.models_) {
        const auto* binding =
            vqec_vision_ai_core_mprgy_find_binding(_registry, model.model_id_);
        if (binding == nullptr || binding->model_version_ != model.model_version_ ||
            binding->target_id_ != model.target_id_ ||
            binding->artifact_ref_ != model.artifact_ref_) {
            return {status_code::invalid_argument,
                "model package binding differs from catalog identity"};
        }
    }
    return {};
}

}  // namespace vqec::vision::ai
