#ifndef VQEC_VISION_AI_PORTS_IMAGE_PROCESSOR_HPP
#define VQEC_VISION_AI_PORTS_IMAGE_PROCESSOR_HPP

#include <cstdint>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_frame_descriptor.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_inference_plan.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp"

namespace vqec::vision::ai {

// Borrowed linear NV12 planes. The producer owns the pixels and keeps them alive for the
// whole call. Pointers are CPU-visible; importing and cache/fence policy belong to the
// adapter that builds this view.
struct nv12_frame_view {
    const std::uint8_t* y_data_{nullptr};
    const std::uint8_t* uv_data_{nullptr};
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::int32_t y_stride_{0};
    std::int32_t uv_stride_{0};
};

// Neutral pixel preprocessing. Backends that do not preprocess pixels use this stage to
// turn one NV12 frame into the exact model input tensor declared by _target. It is a pure
// CPU/accelerator transform: no inference, entitlement or output decision. The caller
// supplies the target tensor spec (name/dtype/shape/quantization) from the composed graph.
class image_processor_port {
public:
    virtual ~image_processor_port() = default;

    // Validates that the frame geometry and plan can be transformed into the target.
    [[nodiscard]] virtual status vqec_vision_ai_ports_imgpr_validate(
        const nv12_frame_view& _view, const inference_plan& _plan,
        const tensor_spec& _target) const = 0;

    // Writes exactly one tensor blob matching _target. One input tensor is supported in
    // this slice. Failure preserves _outputs.
    [[nodiscard]] virtual status vqec_vision_ai_ports_imgpr_preprocess(
        const nv12_frame_view& _view, const inference_plan& _plan,
        const tensor_spec& _target, std::vector<tensor_blob>& _outputs) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_IMAGE_PROCESSOR_HPP
