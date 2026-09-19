#ifndef VQEC_VISION_AI_QCOM_FASTCV_ALIGNER_HPP
#define VQEC_VISION_AI_QCOM_FASTCV_ALIGNER_HPP

#include <array>
#include <cstdint>
#include <vector>

#include "vqec/vision/ai/contracts/media/vqec_vision_image_enums.hpp"
#include "vqec/vision/ai/contracts/inference/vqec_vision_preprocess_spec.hpp"
#include "vqec/vision/ai/ports/inference/vqec_vision_image_alignment.hpp"

namespace vqec::vision::ai {

class dsp_buffer_cache;

struct fastcv_aligner_config {
    // When true the destination is packed RGB/BGR 8-bit; otherwise a single-channel luma
    // patch. RGB requires an explicit matrix, range and channel order.
    bool output_rgb_{false};
    color_matrix matrix_{color_matrix::unspecified};
    color_range range_{color_range::unspecified};
    channel_order order_{channel_order::rgb};
    std::shared_ptr<dsp_buffer_cache> buffer_cache_{nullptr};
};

// Owned FastCV landmark-alignment adapter behind image_alignment_port. It computes the
// 4-DOF similarity in neutral code and warps the source patch with the FastCV affine warp.
// The source NV12 is mapped read-only from the borrowed FD; RGB output uses the reviewed
// neutral NV12->RGB conversion and a per-channel warp (a CPU copy plus three warps, not the
// final optimized path). Color conversion, crop/tensor pooling and DSP offload are not
// claimed as optimized.
//
// The FastCV affine convention was established on QCS6490:
// docs/architecture/image_alignment_port.md.
class fastcv_aligner final : public image_alignment_port {
public:
    fastcv_aligner() = default;
    explicit fastcv_aligner(fastcv_aligner_config _config) noexcept;

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

private:
    fastcv_aligner_config config_{};
    std::vector<std::uint8_t> rgb_scratch_;
    std::array<std::vector<std::uint8_t>, 3> planes_scratch_;
    std::array<std::vector<std::uint8_t>, 3> patches_scratch_;
    std::vector<std::uint8_t> luma_scratch_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QCOM_FASTCV_ALIGNER_HPP
