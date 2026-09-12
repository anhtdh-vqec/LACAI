#ifndef VQEC_VISION_AI_PORTS_IMAGE_PROCESSOR_HPP
#define VQEC_VISION_AI_PORTS_IMAGE_PROCESSOR_HPP

#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_inference_plan.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp"
#include "vqec/vision/ai/ports/vqec_vision_raw_source.hpp"

namespace vqec::vision::ai {

// Neutral pixel preprocessing. Backends that do not preprocess pixels use this stage to
// turn one borrowed RAW frame into the exact model input tensor declared by _target. The
// implementation owns memory mapping/import and cache policy; it performs no inference,
// entitlement or output decision. The caller keeps the frame owner alive for the call.
class image_processor_port {
public:
    virtual ~image_processor_port() = default;

    // Validates that the frame descriptor and plan can be transformed into the target.
    [[nodiscard]] virtual status vqec_vision_ai_ports_imgpr_validate(
        const raw_frame& _frame, const inference_plan& _plan,
        const tensor_spec& _target) const = 0;

    // Writes exactly one tensor blob matching _target. One input tensor is supported in
    // this slice. Failure preserves _outputs.
    [[nodiscard]] virtual status vqec_vision_ai_ports_imgpr_preprocess(
        const raw_frame& _frame, const inference_plan& _plan,
        const tensor_spec& _target, std::vector<tensor_blob>& _outputs) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_IMAGE_PROCESSOR_HPP
