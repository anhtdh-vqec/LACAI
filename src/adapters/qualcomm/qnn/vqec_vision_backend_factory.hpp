#ifndef VQEC_VISION_AI_QCOM_BACKEND_FACTORY_HPP
#define VQEC_VISION_AI_QCOM_BACKEND_FACTORY_HPP

#include <memory>

#include "vqec_vision_qnn_engine.hpp"
#include "vqec_vision_qnn_inference_graph.hpp"
#include "vqec/vision/ai/contracts/inference/vqec_vision_inference_execution.hpp"
#include "vqec/vision/ai/contracts/inference/vqec_vision_model_catalog.hpp"
#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"

namespace vqec::vision::ai {

// Owns one QNN engine and its graph-port binding. The engine outlives the graph; both are
// destroyed in dependency order. Borrowed paths/policy are not retained after creation.
class qnn_backend_bundle final {
public:
    qnn_backend_bundle(const qnn_backend_bundle& _other) = delete;
    qnn_backend_bundle& operator=(const qnn_backend_bundle& _other) = delete;

    [[nodiscard]] qnn_engine* vqec_vision_ai_qcom_bfact_get_engine() noexcept;
    [[nodiscard]] inference_graph_port* vqec_vision_ai_qcom_bfact_get_graph() noexcept;
    [[nodiscard]] const inference_capabilities&
    vqec_vision_ai_qcom_bfact_get_capabilities() const noexcept;

private:
    qnn_backend_bundle() = default;

    std::unique_ptr<qnn_engine> engine_;
    std::unique_ptr<qnn_inference_graph> graph_;
    inference_capabilities capabilities_;

    friend status vqec_vision_ai_qcom_bfact_create(
        const resolved_model_paths& _paths,
        const inference_execution_policy& _policy,
        std::unique_ptr<qnn_backend_bundle>& _bundle);
};

// Configures paths/policy and constructs the graph binding without loading vendor libraries or
// touching HTP. The graph opens and probes the real backend only from load(), after the neutral
// source session's first-frame gate. Failure preserves _bundle.
[[nodiscard]] status vqec_vision_ai_qcom_bfact_create(
    const resolved_model_paths& _paths,
    const inference_execution_policy& _policy,
    std::unique_ptr<qnn_backend_bundle>& _bundle);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QCOM_BACKEND_FACTORY_HPP
