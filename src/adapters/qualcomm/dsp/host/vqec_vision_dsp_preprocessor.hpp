#ifndef VQEC_VISION_AI_QUALCOMM_DSP_PREPROCESSOR_HPP
#define VQEC_VISION_AI_QUALCOMM_DSP_PREPROCESSOR_HPP

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_inference_plan.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp"
#include "vqec/vision/ai/ports/vqec_vision_image_processor.hpp"
#include "vqec/vision/ai/ports/vqec_vision_raw_source.hpp"

#include "vqec_vision_dsp_buffer_cache.hpp"
#include "vqec_vision_dsp_session.hpp"

namespace vqec::vision::ai {

enum class dsp_preprocessor_kind {
    yolov8,
    scrfd
};

struct dsp_preprocessor_config {
    dsp_preprocessor_kind kind_{dsp_preprocessor_kind::yolov8};
    std::shared_ptr<dsp_session> session_;
    std::shared_ptr<dsp_buffer_cache> buffer_cache_;
};

// Hexagon cDSP image preprocessor adapting raw NV12 camera DMA-BUFs into quantized
// uint16 NHWC model input tensors via FastRPC and cDSP FastCV HVX.
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

    [[nodiscard]] static std::array<std::int32_t, 12> vqec_vision_ai_qcom_dsppr_compute_geom(
        dsp_preprocessor_kind _kind,
        std::uint32_t _src_w, std::uint32_t _src_h,
        std::int32_t _y_stride, std::uint32_t _uv_offset, std::int32_t _uv_stride,
        std::uint32_t _tensor_side);

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QUALCOMM_DSP_PREPROCESSOR_HPP
