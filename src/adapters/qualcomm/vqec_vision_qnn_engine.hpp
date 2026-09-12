#ifndef VQEC_VISION_AI_QCOM_QNN_ENGINE_HPP
#define VQEC_VISION_AI_QCOM_QNN_ENGINE_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_inference_execution.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp"

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

    // Creates a context and loads one QNN model library (the `.so` produced by
    // qnn-model-lib-generator), composing its graphs. Exactly one graph is supported in
    // this step; a multi-graph library is rejected rather than partially used.
    [[nodiscard]] status vqec_vision_ai_qcom_qneng_prepare(const std::string& _model_library);

    // Reports the composed graph input/output tensor identity (name, shape, dtype,
    // quantization). Order is the vendor graph order and must be bound by the caller.
    [[nodiscard]] status vqec_vision_ai_qcom_qneng_get_tensors(
        std::vector<tensor_spec>& _inputs, std::vector<tensor_spec>& _outputs) const;

    // Synchronous client-buffer execution. Every input blob must match the graph input
    // dtype and packed byte count; outputs are allocated and returned in graph order.
    [[nodiscard]] status vqec_vision_ai_qcom_qneng_execute(
        const std::vector<tensor_blob>& _inputs, std::vector<tensor_blob>& _outputs);

    // Releases graphs, context, device, backend and libraries in dependency order.
    void vqec_vision_ai_qcom_qneng_close() noexcept;

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QCOM_QNN_ENGINE_HPP
