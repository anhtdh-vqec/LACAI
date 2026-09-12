#include "vqec_vision_backend_factory.hpp"

#include <new>
#include <utility>

namespace vqec::vision::ai {

qnn_engine* qnn_backend_bundle::vqec_vision_ai_qcom_bfact_get_engine() noexcept {
    return engine_.get();
}

inference_graph_port* qnn_backend_bundle::vqec_vision_ai_qcom_bfact_get_graph() noexcept {
    return graph_.get();
}

const inference_capabilities& qnn_backend_bundle::
vqec_vision_ai_qcom_bfact_get_capabilities() const noexcept {
    return capabilities_;
}

status vqec_vision_ai_qcom_bfact_create(
    const resolved_model_paths& _paths,
    const inference_execution_policy& _policy,
    std::unique_ptr<qnn_backend_bundle>& _bundle) {
    if (_paths.model_id_.empty() || _paths.model_path_.empty() ||
        _paths.backend_path_.empty() || _paths.system_path_.empty()) {
        return {status_code::invalid_argument,
            "resolved QNN paths require model, backend and system references"};
    }
    const auto valid_policy = vqec_vision_ai_core_inexe_validate_policy(_policy);
    if (valid_policy.code_ != status_code::ok) {
        return valid_policy;
    }
    try {
        auto candidate = std::unique_ptr<qnn_backend_bundle>(new qnn_backend_bundle());
        candidate->engine_ = std::make_unique<qnn_engine>();
        const auto opened = candidate->engine_->vqec_vision_ai_qcom_qneng_open(
            _paths.backend_path_, _paths.system_path_, _policy);
        if (opened.code_ != status_code::ok) {
            return opened;
        }
        const auto probed = candidate->engine_->vqec_vision_ai_qcom_qneng_probe_capabilities(
            candidate->capabilities_);
        if (probed.code_ != status_code::ok) {
            return probed;
        }
        const auto supported = vqec_vision_ai_core_inexe_policy_is_supported(
            _policy, candidate->capabilities_);
        if (supported.code_ != status_code::ok) {
            return supported;
        }
        candidate->graph_ = std::make_unique<qnn_inference_graph>(*candidate->engine_);
        _bundle = std::move(candidate);
        return {};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "QNN backend bundle allocation failed"};
    } catch (...) {
        return {status_code::io_error, "QNN backend bundle construction raised an exception"};
    }
}

}  // namespace vqec::vision::ai
