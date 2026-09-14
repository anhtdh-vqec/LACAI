#include "vqec/vision/ai/contracts/vqec_vision_model_package.hpp"

#include <utility>

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_core_mpkg_is_sha256(const std::string& _value) noexcept {
    if (_value.size() != 64) {
        return false;
    }
    return _value.find_first_not_of("0123456789abcdef") == std::string::npos;
}

bool vqec_vision_ai_core_mpkg_is_identifier(const std::string& _value) noexcept {
    if (_value.empty() || _value.size() > 128) {
        return false;
    }
    for (const char character : _value) {
        const bool valid = (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '_' ||
            character == '-' || character == '.' || character == ':';
        if (!valid) {
            return false;
        }
    }
    return true;
}

}  // namespace

status vqec_vision_ai_core_mpkg_resolve(
    const model_package_inputs& _inputs, resolved_model_package& _package) {
    const auto& model = _inputs.model_;
    if (!vqec_vision_ai_core_mpkg_is_identifier(model.model_id_) ||
        !vqec_vision_ai_core_mpkg_is_identifier(model.model_version_) ||
        !vqec_vision_ai_core_mpkg_is_identifier(model.target_id_) ||
        model.artifact_ref_.empty() || !vqec_vision_ai_core_mpkg_is_sha256(model.artifact_sha256_) ||
        model.decoder_contract_.empty()) {
        return {status_code::invalid_argument, "model package catalog identity is invalid"};
    }
    if (!vqec_vision_ai_core_mpkg_is_identifier(_inputs.graph_name_)) {
        return {status_code::invalid_argument, "model package graph name is invalid"};
    }
    if (!model.graph_name_.empty() && model.graph_name_ != _inputs.graph_name_) {
        return {status_code::unsupported,
            "model graph name differs between the catalog and the package"};
    }
    const auto io = vqec_vision_ai_core_ioman_validate(_inputs.io_);
    if (io.code_ != status_code::ok) {
        return io;
    }
    if (_inputs.io_.inputs_.size() != 1) {
        return {status_code::unsupported,
            "base v1 supports exactly one image input per model"};
    }
    const auto preprocess = vqec_vision_ai_core_ppspc_validate(_inputs.preprocess_);
    if (preprocess.code_ != status_code::ok) {
        return preprocess;
    }
    const auto& paths = _inputs.paths_;
    if (paths.model_id_ != model.model_id_ || paths.target_id_ != model.target_id_ ||
        paths.artifact_ref_ != model.artifact_ref_ || paths.model_path_.empty() ||
        paths.backend_path_.empty() || paths.system_path_.empty()) {
        return {status_code::invalid_argument,
            "resolved model paths do not match the catalog entry"};
    }
    for (const auto& label : _inputs.class_labels_) {
        if (label.empty()) {
            return {status_code::invalid_argument, "model package class label is empty"};
        }
    }

    resolved_model_package candidate;
    candidate.model_id_ = model.model_id_;
    candidate.model_version_ = model.model_version_;
    candidate.target_id_ = model.target_id_;
    candidate.graph_name_ = _inputs.graph_name_;
    candidate.decoder_contract_ = model.decoder_contract_;
    candidate.paths_ = paths;
    candidate.io_ = _inputs.io_;
    candidate.preprocess_ = _inputs.preprocess_;
    candidate.class_labels_ = _inputs.class_labels_;
    _package = std::move(candidate);
    return {};
}

}  // namespace vqec::vision::ai
