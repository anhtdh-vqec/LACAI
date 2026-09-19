#ifndef VQEC_VISION_AI_APPL_SINGLE_IMAGE_INFERENCE_HPP
#define VQEC_VISION_AI_APPL_SINGLE_IMAGE_INFERENCE_HPP

#include <cstdint>
#include <vector>

#include "vqec/vision/ai/contracts/inference/vqec_vision_model_decoder.hpp"
#include "vqec/vision/ai/ports/inference/vqec_vision_image_processor.hpp"
#include "vqec/vision/ai/ports/inference/vqec_vision_inference_graph.hpp"
#include "vqec/vision/ai/ports/perception/vqec_vision_face_image_inference.hpp"

namespace vqec::vision::ai {

struct single_image_inference_config {
    image_processor_port* processor_{nullptr};
    inference_graph_port* graph_{nullptr};
    model_decoder_port* decoder_{nullptr};
    const inference_plan* plan_{nullptr};
    preview_geometry geometry_;
    std::uint32_t camera_id_{0};
    std::uint32_t channel_id_{0};
    std::uint64_t cycle_id_{0};
    std::uint64_t job_timeout_ns_{0};
};

// Serialized one-image execution on a graph whose configure/load/bind/start lifecycle is
// owned by the caller. No retry, queue or hidden graph fallback is performed.
class single_image_inference final : public face_image_detector_port {
public:
    [[nodiscard]] status vqec_vision_ai_appl_siinf_configure(
        const single_image_inference_config& _config);
    [[nodiscard]] status vqec_vision_ai_appl_siinf_run(
        const raw_frame& _frame, std::uint64_t _steady_now_ns,
        observation_batch& _observations);
    [[nodiscard]] status vqec_vision_ai_ports_fidet_run(
        const raw_frame& _frame, std::uint64_t _steady_now_ns,
        observation_batch& _detections) override {
        return vqec_vision_ai_appl_siinf_run(_frame, _steady_now_ns, _detections);
    }

private:
    [[nodiscard]] status vqec_vision_ai_appl_siinf_prepare_input();
    single_image_inference_config config_{};
    tensor_spec input_spec_{};
    std::vector<tensor_blob> input_;
    observation_batch scratch_;
    std::uint64_t armed_source_epoch_{0};
    bool is_configured_{false};
    bool is_input_prepared_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_SINGLE_IMAGE_INFERENCE_HPP
