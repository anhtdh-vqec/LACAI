#ifndef VQEC_VISION_AI_QUALCOMM_FASTCV_PROCESSOR_HPP
#define VQEC_VISION_AI_QUALCOMM_FASTCV_PROCESSOR_HPP

#include <cstdint>
#include <memory>

#include "vqec/vision/ai/ports/inference/vqec_vision_image_processor.hpp"

namespace vqec::vision::ai {

struct fastcv_processor_config {
    std::uint64_t max_frame_allocation_bytes_{0};
    std::uint64_t output_timeout_ns_{0};
};

// Qualcomm preprocessing adapter. Vendor and GStreamer types stay behind the pimpl;
// orchestration continues to depend only on image_processor_port.
class fastcv_processor final : public image_processor_port {
public:
    explicit fastcv_processor(fastcv_processor_config _config);
    ~fastcv_processor() noexcept override;

    fastcv_processor(const fastcv_processor& _other) = delete;
    fastcv_processor& operator=(const fastcv_processor& _other) = delete;

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

#endif  // VQEC_VISION_AI_QUALCOMM_FASTCV_PROCESSOR_HPP
