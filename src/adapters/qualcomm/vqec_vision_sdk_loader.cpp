#include "vqec_vision_sdk_loader.hpp"

#include <dlfcn.h>

#include <QnnInterface.h>  // private QAIRT SDK header, not a project include

namespace vqec::vision::ai {
namespace {

using get_providers_fn = Qnn_ErrorHandle_t (*)(const QnnInterface_t***, uint32_t*);

}  // namespace

struct qnn_sdk_libraries::implementation {
    void* backend_handle_{nullptr};
    void* system_handle_{nullptr};
    const QnnInterface_t* provider_{nullptr};
};

qnn_sdk_libraries::qnn_sdk_libraries()
    : implementation_(std::make_unique<implementation>()) {}

qnn_sdk_libraries::~qnn_sdk_libraries() noexcept {
    vqec_vision_ai_qcom_sdkld_close();
}

status qnn_sdk_libraries::vqec_vision_ai_qcom_sdkld_open(
    const std::string& _backend_library, const std::string& _system_library) {
    if (_backend_library.empty() || _system_library.empty()) {
        return {status_code::invalid_argument, "backend and system library are required"};
    }
    if (vqec_vision_ai_qcom_sdkld_is_open()) {
        return {status_code::invalid_state, "QNN libraries are already open"};
    }
    implementation_->backend_handle_ = ::dlopen(_backend_library.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (implementation_->backend_handle_ == nullptr) {
        return {status_code::io_error, "cannot load QNN backend library"};
    }
    implementation_->system_handle_ = ::dlopen(_system_library.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (implementation_->system_handle_ == nullptr) {
        vqec_vision_ai_qcom_sdkld_close();
        return {status_code::io_error, "cannot load QNN system library"};
    }
    ::dlerror();
    auto* symbol = ::dlsym(implementation_->backend_handle_, "QnnInterface_getProviders");
    if (symbol == nullptr) {
        vqec_vision_ai_qcom_sdkld_close();
        return {status_code::unsupported, "QNN backend does not expose interface providers"};
    }
    const auto get_providers = reinterpret_cast<get_providers_fn>(symbol);
    const QnnInterface_t** providers = nullptr;
    std::uint32_t provider_count = 0;
    if (get_providers(&providers, &provider_count) != QNN_SUCCESS || providers == nullptr ||
        provider_count == 0) {
        vqec_vision_ai_qcom_sdkld_close();
        return {status_code::unsupported, "QNN backend returned no interface provider"};
    }
    // Select the first provider whose core API version is compatible with the headers this
    // adapter was compiled against. providers[0] is not assumed compatible: a provider from
    // a different QNN version can expose a different interface layout that would be
    // misinterpreted when its function pointers are called.
    const QnnInterface_t* selected = nullptr;
    for (std::uint32_t index = 0; index < provider_count; ++index) {
        const QnnInterface_t* candidate = providers[index];
        if (candidate == nullptr) {
            continue;
        }
        const auto& core = candidate->apiVersion.coreApiVersion;
        if (core.major == QNN_API_VERSION_MAJOR && core.minor >= QNN_API_VERSION_MINOR) {
            selected = candidate;
            break;
        }
    }
    if (selected == nullptr) {
        vqec_vision_ai_qcom_sdkld_close();
        return {status_code::unsupported,
            "QNN backend exposes no provider compatible with the compiled API version"};
    }
    implementation_->provider_ = selected;
    return {};
}

bool qnn_sdk_libraries::vqec_vision_ai_qcom_sdkld_is_open() const noexcept {
    return implementation_ != nullptr && implementation_->backend_handle_ != nullptr &&
        implementation_->provider_ != nullptr;
}

const void* qnn_sdk_libraries::vqec_vision_ai_qcom_sdkld_get_provider() const noexcept {
    return implementation_ != nullptr ? implementation_->provider_ : nullptr;
}

const char* qnn_sdk_libraries::vqec_vision_ai_qcom_sdkld_get_provider_name() const noexcept {
    return implementation_ != nullptr && implementation_->provider_ != nullptr ?
        implementation_->provider_->providerName : nullptr;
}

void qnn_sdk_libraries::vqec_vision_ai_qcom_sdkld_close() noexcept {
    if (implementation_ == nullptr) {
        return;
    }
    implementation_->provider_ = nullptr;
    if (implementation_->system_handle_ != nullptr) {
        ::dlclose(implementation_->system_handle_);
        implementation_->system_handle_ = nullptr;
    }
    if (implementation_->backend_handle_ != nullptr) {
        ::dlclose(implementation_->backend_handle_);
        implementation_->backend_handle_ = nullptr;
    }
}

}  // namespace vqec::vision::ai
