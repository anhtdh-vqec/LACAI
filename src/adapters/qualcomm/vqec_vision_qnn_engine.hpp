#ifndef VQEC_VISION_AI_QCOM_QNN_ENGINE_HPP
#define VQEC_VISION_AI_QCOM_QNN_ENGINE_HPP

#include <memory>
#include <string>

#include "vqec/vision/ai/contracts/vqec_vision_inference_execution.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

// LACAI-owned Qualcomm QNN engine. It owns the loaded SDK libraries, backend and device
// and reports the execution capabilities it can actually satisfy. No QNN type appears in
// this header; the adapter translation units keep the vendor boundary.
//
// Opening the engine creates a backend and, when the interface provides it, a device. It
// does not create a context, compose a graph or execute; those are later steps. All calls
// are serialized by the caller and must run on the backend worker thread.
class qnn_engine final {
public:
    qnn_engine();
    ~qnn_engine() noexcept;
    qnn_engine(const qnn_engine& _other) = delete;
    qnn_engine& operator=(const qnn_engine& _other) = delete;

    // Loads the backend/system libraries, resolves the interface, creates the backend and
    // device. The policy is the validated intent used only for device/affinity selection;
    // it is not silently downgraded. Failure tears down anything already created.
    [[nodiscard]] status vqec_vision_ai_qcom_qneng_open(
        const std::string& _backend_library, const std::string& _system_library,
        const inference_execution_policy& _policy);
    [[nodiscard]] bool vqec_vision_ai_qcom_qneng_is_open() const noexcept;
    // Reports capabilities derived from the resolved interface and created device.
    [[nodiscard]] status vqec_vision_ai_qcom_qneng_probe_capabilities(
        inference_capabilities& _capabilities) const noexcept;
    // Releases device, backend and libraries in dependency order; idempotent.
    void vqec_vision_ai_qcom_qneng_close() noexcept;

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QCOM_QNN_ENGINE_HPP
