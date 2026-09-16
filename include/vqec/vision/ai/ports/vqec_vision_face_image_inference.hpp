#ifndef VQEC_VISION_AI_PORTS_FACE_IMAGE_INFERENCE_HPP
#define VQEC_VISION_AI_PORTS_FACE_IMAGE_INFERENCE_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_embedding.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_observation.hpp"
#include "vqec/vision/ai/ports/vqec_vision_raw_source.hpp"

namespace vqec::vision::ai {

class face_image_detector_port {
public:
    virtual ~face_image_detector_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_ports_fidet_run(
        const raw_frame& _frame, std::uint64_t _steady_now_ns,
        observation_batch& _detections) = 0;
};

class face_image_cascade_port {
public:
    virtual ~face_image_cascade_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_ports_ficas_run(
        const raw_frame& _frame, const observation_batch& _detections,
        std::uint64_t _steady_now_ns, std::vector<embedding_result>& _embeddings,
        std::size_t& _failed_tasks) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_FACE_IMAGE_INFERENCE_HPP
