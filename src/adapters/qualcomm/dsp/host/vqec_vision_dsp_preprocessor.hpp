#ifndef VQEC_VISION_AI_QUALCOMM_DSP_PREPROCESSOR_HPP
#define VQEC_VISION_AI_QUALCOMM_DSP_PREPROCESSOR_HPP

#include <memory>

#include "vqec/vision/ai/ports/inference/vqec_vision_image_processor.hpp"

#include "vqec_vision_dsp_buffer_cache.hpp"
#include "vqec_vision_dsp_v1_client.hpp"

namespace vqec::vision::ai {

struct dsp_preprocessor_config {
    std::shared_ptr<dsp_v1_client> client_;
    std::shared_ptr<dsp_buffer_cache> buffer_cache_;
};

// Descriptor-driven cDSP image transform. Model identity never crosses this boundary;
// exact pixel/tensor semantics are derived from the admitted inference plan.
class dsp_preprocessor final : public image_processor_port {
public:
    explicit dsp_preprocessor(dsp_preprocessor_config _config);
    ~dsp_preprocessor() noexcept override;

    dsp_preprocessor(const dsp_preprocessor&) = delete;
    dsp_preprocessor& operator=(const dsp_preprocessor&) = delete;
    dsp_preprocessor(dsp_preprocessor&&) noexcept;
    dsp_preprocessor& operator=(dsp_preprocessor&&) noexcept;

    [[nodiscard]] status vqec_vision_ai_ports_imgpr_validate(
        const raw_frame& _frame, const inference_plan& _plan,
        const tensor_spec& _target) const override;

    [[nodiscard]] status vqec_vision_ai_ports_imgpr_preprocess(
        const raw_frame& _frame, const inference_plan& _plan,
        const tensor_spec& _target, std::vector<tensor_blob>& _outputs) override;

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QUALCOMM_DSP_PREPROCESSOR_HPP
