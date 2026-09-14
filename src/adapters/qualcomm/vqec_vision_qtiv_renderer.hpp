#ifndef VQEC_VISION_AI_QCOM_QTIV_RENDERER_HPP
#define VQEC_VISION_AI_QCOM_QTIV_RENDERER_HPP

#include <cstdint>
#include <memory>
#include <string>

#include "vqec/vision/ai/contracts/vqec_vision_observation.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/ports/vqec_vision_raw_source.hpp"

namespace vqec::vision::ai {

// AI-owned encoded output adapter for the Qualcomm target.
//
// Renders detection boxes with the hardware `qtivoverlay` element (it reads
// GstVideoRegionOfInterestMeta with an "ObjectDetection" param), encodes H.264 with
// v4l2h264enc and writes each access unit into the released FW shared-memory ring that the
// FW RTSP service reads. It is a private adapter: no GStreamer or vendor type crosses this
// boundary. Pixel repacking currently uses the CPU, while box drawing remains on qtivoverlay.
struct qtiv_renderer_config {
    std::string ring_id_;
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::uint32_t fps_{0};
    std::uint32_t bitrate_bps_{0};
    std::uint32_t keyframe_interval_frames_{0};
    std::uint32_t output_surface_count_{0};
    // qtivoverlay uses 0xRRGGBBAA; alpha is the least-significant byte.
    std::uint32_t box_color_rgba_{0};
    // Required deployment values. The adapter passes them to GStreamer caps and never
    // guesses camera colorimetry or scan mode.
    std::string colorimetry_;
    std::string interlace_mode_;
};

class qtiv_renderer final {
public:
    qtiv_renderer();
    ~qtiv_renderer() noexcept;
    qtiv_renderer(const qtiv_renderer& _other) = delete;
    qtiv_renderer& operator=(const qtiv_renderer& _other) = delete;

    // Builds the encode pipeline and opens the FW ring. Cold path; no frame is submitted.
    [[nodiscard]] status vqec_vision_ai_qcom_qtvr_init(const qtiv_renderer_config& _config);
    // Attaches one ROI meta per observation, encodes the frame and writes one AU to the
    // ring. The frame owner must stay valid for the call.
    [[nodiscard]] status vqec_vision_ai_qcom_qtvr_render(
        const raw_frame& _frame, const observation_batch& _observations);
    [[nodiscard]] std::uint64_t vqec_vision_ai_qcom_qtvr_get_written() const noexcept;
    void vqec_vision_ai_qcom_qtvr_close() noexcept;

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QCOM_QTIV_RENDERER_HPP
