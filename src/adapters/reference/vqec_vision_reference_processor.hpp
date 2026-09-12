#ifndef VQEC_VISION_AI_REFER_REFERENCE_PROCESSOR_HPP
#define VQEC_VISION_AI_REFER_REFERENCE_PROCESSOR_HPP

#include "vqec/vision/ai/ports/vqec_vision_image_processor.hpp"

namespace vqec::vision::ai {

// Device-free CPU preprocessor: linear NV12 -> letterboxed RGB -> normalize -> quantize
// into the target tensor dtype/quantization. It is a correctness baseline for pipelines
// and tests, not a performance path; the production adapter uses the accelerator. Nearest
// sampling is used so the transform stays deterministic and dependency-free.
class reference_image_processor final : public image_processor_port {
public:
    reference_image_processor() = default;
    reference_image_processor(const reference_image_processor& _other) = delete;
    reference_image_processor& operator=(const reference_image_processor& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_ports_imgpr_validate(
        const nv12_frame_view& _view, const inference_plan& _plan,
        const tensor_spec& _target) const override;
    [[nodiscard]] status vqec_vision_ai_ports_imgpr_preprocess(
        const nv12_frame_view& _view, const inference_plan& _plan,
        const tensor_spec& _target, std::vector<tensor_blob>& _outputs) override;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_REFER_REFERENCE_PROCESSOR_HPP
