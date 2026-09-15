#ifndef VQEC_VISION_AI_QCOM_FASTCV_ALIGNER_HPP
#define VQEC_VISION_AI_QCOM_FASTCV_ALIGNER_HPP

#include <cstdint>

#include "vqec/vision/ai/ports/vqec_vision_image_alignment.hpp"

namespace vqec::vision::ai {

// Owned FastCV landmark-alignment adapter behind image_alignment_port. It computes the
// 4-DOF similarity in neutral code and warps the source luma patch with the FastCV affine
// warp. This slice maps the source NV12 luma read-only from the borrowed FD into a
// contiguous single-channel buffer (a CPU copy) and returns a single-channel destination;
// color conversion, crop/tensor pooling and DSP offload are not claimed.
//
// The FastCV affine convention was established on QCS6490:
// docs/architecture/image_alignment_port.md.
class fastcv_aligner final : public image_alignment_port {
public:
    fastcv_aligner() = default;

    [[nodiscard]] status vqec_vision_ai_ports_imaln_probe_capabilities(
        alignment_capabilities& _capabilities) const override;
    [[nodiscard]] status vqec_vision_ai_ports_imaln_validate_template(
        const alignment_template& _template,
        const alignment_capabilities& _capabilities) const override;
    [[nodiscard]] status vqec_vision_ai_ports_imaln_align(
        const alignment_request& _request, const raw_frame& _source,
        const alignment_template& _template, alignment_result& _result,
        std::uint64_t& _ticket) override;
    [[nodiscard]] status vqec_vision_ai_ports_imaln_poll_completion(
        std::uint64_t _ticket, bool& _complete) override;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QCOM_FASTCV_ALIGNER_HPP
