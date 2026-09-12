#include "vqec_vision_qnn_engine.hpp"

#include <cstdint>

#include <QnnInterface.h>  // private QAIRT SDK header, not a project include

#include "vqec_vision_sdk_loader.hpp"

namespace vqec::vision::ai {
namespace {

std::uint32_t vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type _type) noexcept {
    return static_cast<std::uint32_t>(1U) << static_cast<std::uint32_t>(_type);
}

std::uint32_t vqec_vision_ai_qcom_qneng_supported_dtype_mask() noexcept {
    // QNN HTP supports these element types. This is an engine-level advertisement; a
    // specific model may support fewer, and the composed graph tensors are authoritative.
    return vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type::int8) |
        vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type::uint8) |
        vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type::int16) |
        vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type::uint16) |
        vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type::int32) |
        vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type::uint32) |
        vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type::float16) |
        vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type::float32);
}

}  // namespace

struct qnn_engine::implementation {
    qnn_sdk_libraries libraries_;
    const QNN_INTERFACE_VER_TYPE* qnn_{nullptr};
    Qnn_BackendHandle_t backend_{nullptr};
    Qnn_DeviceHandle_t device_{nullptr};
    bool is_open_{false};
};

qnn_engine::qnn_engine() : implementation_(std::make_unique<implementation>()) {}

qnn_engine::~qnn_engine() noexcept {
    vqec_vision_ai_qcom_qneng_close();
}

status qnn_engine::vqec_vision_ai_qcom_qneng_open(
    const std::string& _backend_library, const std::string& _system_library,
    const inference_execution_policy& _policy) {
    if (implementation_ == nullptr) {
        return {status_code::invalid_state, "QNN engine implementation is unavailable"};
    }
    auto& impl = *implementation_;
    if (impl.is_open_) {
        return {status_code::invalid_state, "QNN engine is already open"};
    }
    const auto valid_policy = vqec_vision_ai_core_inexe_validate_policy(_policy);
    if (valid_policy.code_ != status_code::ok) {
        return valid_policy;
    }
    const auto loaded = impl.libraries_.vqec_vision_ai_qcom_sdkld_open(
        _backend_library, _system_library);
    if (loaded.code_ != status_code::ok) {
        return loaded;
    }
    const auto* provider =
        static_cast<const QnnInterface_t*>(impl.libraries_.vqec_vision_ai_qcom_sdkld_get_provider());
    if (provider == nullptr) {
        impl.libraries_.vqec_vision_ai_qcom_sdkld_close();
        return {status_code::unsupported, "QNN interface provider is unavailable"};
    }
    impl.qnn_ = &provider->QNN_INTERFACE_VER_NAME;
    if (impl.qnn_->backendCreate == nullptr) {
        vqec_vision_ai_qcom_qneng_close();
        return {status_code::unsupported, "QNN backend interface is incomplete"};
    }
    if (impl.qnn_->backendCreate(nullptr, nullptr, &impl.backend_) != QNN_SUCCESS ||
        impl.backend_ == nullptr) {
        vqec_vision_ai_qcom_qneng_close();
        return {status_code::io_error, "QNN backend creation failed"};
    }
    if (impl.qnn_->deviceCreate != nullptr) {
        // HTP needs a device for affinity and performance configuration. A failed device
        // creation is a fault for the admitted HTP path, not a silent CPU downgrade.
        if (impl.qnn_->deviceCreate(nullptr, nullptr, &impl.device_) != QNN_SUCCESS) {
            vqec_vision_ai_qcom_qneng_close();
            return {status_code::unsupported, "QNN device creation failed for the selected backend"};
        }
    }
    impl.is_open_ = true;
    return {};
}

bool qnn_engine::vqec_vision_ai_qcom_qneng_is_open() const noexcept {
    return implementation_ != nullptr && implementation_->is_open_;
}

status qnn_engine::vqec_vision_ai_qcom_qneng_probe_capabilities(
    inference_capabilities& _capabilities) const noexcept {
    if (!vqec_vision_ai_qcom_qneng_is_open()) {
        return {status_code::invalid_state, "QNN engine is not open"};
    }
    const auto& qnn = *implementation_->qnn_;
    inference_capabilities capabilities;
    capabilities.supported_dtype_mask_ = vqec_vision_ai_qcom_qneng_supported_dtype_mask();
    capabilities.perf_profile_mask_ = implementation_->device_ != nullptr ?
        static_cast<std::uint8_t>(0x0FU) :  // all profiles require a device/perf infrastructure
        static_cast<std::uint8_t>(1U << static_cast<unsigned>(inference_perf_profile::balanced));
    capabilities.graph_count_ = 1;
    capabilities.supports_async_ = qnn.graphExecuteAsync != nullptr;
    capabilities.max_inflight_jobs_ = capabilities.supports_async_ ? 2 : 1;
    capabilities.supports_shared_memory_ =
        qnn.memRegister != nullptr && qnn.memDeRegister != nullptr;
    capabilities.max_shared_registrations_ = capabilities.supports_shared_memory_ ? 64 : 0;
    capabilities.supports_native_output_ = true;
    capabilities.supports_artifact_update_ = qnn.contextApplyBinarySection != nullptr;
    capabilities.supports_multi_model_domain_ = true;
    const auto valid = vqec_vision_ai_core_inexe_validate_capabilities(capabilities);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    _capabilities = capabilities;
    return {};
}

void qnn_engine::vqec_vision_ai_qcom_qneng_close() noexcept {
    if (implementation_ == nullptr) {
        return;
    }
    auto& impl = *implementation_;
    if (impl.qnn_ != nullptr && impl.device_ != nullptr && impl.qnn_->deviceFree != nullptr) {
        (void)impl.qnn_->deviceFree(impl.device_);
    }
    impl.device_ = nullptr;
    if (impl.qnn_ != nullptr && impl.backend_ != nullptr && impl.qnn_->backendFree != nullptr) {
        (void)impl.qnn_->backendFree(impl.backend_);
    }
    impl.backend_ = nullptr;
    impl.qnn_ = nullptr;
    impl.is_open_ = false;
    impl.libraries_.vqec_vision_ai_qcom_sdkld_close();
}

}  // namespace vqec::vision::ai
