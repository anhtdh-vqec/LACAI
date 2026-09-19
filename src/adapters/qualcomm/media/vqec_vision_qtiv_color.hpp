#ifndef VQEC_VISION_AI_QCOM_QTIV_COLOR_HPP
#define VQEC_VISION_AI_QCOM_QTIV_COLOR_HPP

#include <cstdint>
#include <memory>
#include <vector>

#include "vqec/vision/ai/contracts/inference/vqec_vision_preprocess_spec.hpp"
#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"

namespace vqec::vision::ai {

enum class qtiv_color_engine { fcv, gles };

// Private Qualcomm color-conversion offload. A persistent appsrc -> qtivtransform ->
// appsink pipeline converts a tightly packed NV12 image to tightly packed RGB8 on the
// FastCV/GLES backend instead of a CPU loop. No GStreamer or vendor type appears in the
// neutral layer; this adapter is used by the FastCV aligner.
class qtiv_color_converter {
public:
    qtiv_color_converter();
    ~qtiv_color_converter() noexcept;
    qtiv_color_converter(const qtiv_color_converter&) = delete;
    qtiv_color_converter& operator=(const qtiv_color_converter&) = delete;

    // Builds a persistent pipeline with fixed NV12 width/height caps so caps negotiation
    // happens before PLAYING; the caller sizes one converter per distinct ROI size.
    [[nodiscard]] status vqec_vision_ai_qcom_qtcol_open(
        qtiv_color_engine _engine, std::uint32_t _width, std::uint32_t _height,
        color_matrix _matrix, std::uint64_t _timeout_ns);
    [[nodiscard]] status vqec_vision_ai_qcom_qtcol_convert(
        const std::uint8_t* _nv12, std::vector<std::uint8_t>& _rgb);
    void vqec_vision_ai_qcom_qtcol_close() noexcept;

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QCOM_QTIV_COLOR_HPP
