#ifndef VQEC_VISION_AI_QCOM_SDK_LOADER_HPP
#define VQEC_VISION_AI_QCOM_SDK_LOADER_HPP

#include <memory>
#include <string>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

// Private Qualcomm boundary: dynamically loads the QNN backend and system libraries,
// resolves the interface provider and keeps both handles. No QNN type appears in this
// header so it can be included only by the Qualcomm adapter translation units.
//
// This does not create a backend/device/context and proves neither accelerator
// availability nor execution. It only establishes the interface provider.
class qnn_sdk_libraries final {
public:
    qnn_sdk_libraries();
    ~qnn_sdk_libraries() noexcept;
    qnn_sdk_libraries(const qnn_sdk_libraries& _other) = delete;
    qnn_sdk_libraries& operator=(const qnn_sdk_libraries& _other) = delete;

    // Bare sonames are resolved through the target loader path; explicit paths are used
    // as given. Failure closes any partially opened library and preserves nothing.
    [[nodiscard]] status vqec_vision_ai_qcom_sdkld_open(
        const std::string& _backend_library, const std::string& _system_library);
    [[nodiscard]] bool vqec_vision_ai_qcom_sdkld_is_open() const noexcept;
    // Borrowed QnnInterface_t* for the selected provider, or null when not open. The
    // concrete type stays in the implementation translation unit.
    [[nodiscard]] const void* vqec_vision_ai_qcom_sdkld_get_provider() const noexcept;
    [[nodiscard]] const char* vqec_vision_ai_qcom_sdkld_get_provider_name() const noexcept;
    void vqec_vision_ai_qcom_sdkld_close() noexcept;

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QCOM_SDK_LOADER_HPP
