#ifndef VQEC_VISION_AI_APPL_CASCADE_COORDINATOR_HPP
#define VQEC_VISION_AI_APPL_CASCADE_COORDINATOR_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_embedding.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_image_alignment.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_observation.hpp"
#include "vqec/vision/ai/ports/vqec_vision_cascade_frame_lease.hpp"
#include "vqec/vision/ai/ports/vqec_vision_embedding_decoder.hpp"
#include "vqec/vision/ai/ports/vqec_vision_image_alignment.hpp"
#include "vqec/vision/ai/ports/vqec_vision_inference_graph.hpp"

namespace vqec::vision::ai {

struct cascade_coordinator_config {
    image_alignment_port* aligner_{nullptr};
    cascade_frame_lease_port* lease_{nullptr};
    // Optional secondary embedding graph and decoder. When both are set, each aligned face is
    // quantized to the model input, submitted, polled and decoded; when both are null the
    // coordinator only aligns. Setting one without the other is invalid.
    inference_graph_port* embedding_graph_{nullptr};
    embedding_decoder_port* embedding_decoder_{nullptr};
    alignment_template template_;
    std::array<float, 3> normalize_offset_{0.0F, 0.0F, 0.0F};
    std::array<float, 3> normalize_scale_{1.0F, 1.0F, 1.0F};
    float quant_scale_{0.0F};
    std::int32_t quant_zero_point_{0};
    // Maximum faces aligned per primary frame.
    std::size_t max_tasks_per_frame_{0};
};

struct cascade_coordinator_report {
    std::uint16_t accepted_{0};
    std::uint16_t embedded_{0};
    std::uint16_t skipped_{0};
    std::uint16_t failed_{0};
};

// Serialized cascade coordinator. For one decoded primary observation batch it acquires the
// exact retained frame once per admitted face, aligns each, optionally quantizes and runs the
// secondary embedding graph, completes each ticket and closes admission. A per-task failure
// is counted and isolated; it never faults the primary path. Synchronous secondary backend in
// this slice; async and the bounded worker are optimization.
class cascade_coordinator {
public:
    cascade_coordinator() = default;
    [[nodiscard]] status vqec_vision_ai_appl_cscrd_configure(
        const cascade_coordinator_config& _config);
    [[nodiscard]] status vqec_vision_ai_appl_cscrd_process(
        std::uint64_t _steady_now_ns, const observation_batch& _tracked,
        std::vector<alignment_result>& _aligned,
        std::vector<embedding_result>& _embeddings,
        cascade_coordinator_report& _report);
    [[nodiscard]] bool vqec_vision_ai_appl_cscrd_is_configured() const noexcept;

private:
    image_alignment_port* aligner_{nullptr};
    cascade_frame_lease_port* lease_{nullptr};
    inference_graph_port* embedding_graph_{nullptr};
    embedding_decoder_port* embedding_decoder_{nullptr};
    alignment_template template_;
    alignment_capabilities capabilities_{};
    std::array<float, 3> normalize_offset_{0.0F, 0.0F, 0.0F};
    std::array<float, 3> normalize_scale_{1.0F, 1.0F, 1.0F};
    float quant_scale_{0.0F};
    std::int32_t quant_zero_point_{0};
    std::size_t max_tasks_per_frame_{0};
    // Full model input spec taken from the loaded graph (name, dims, dtype, quantization).
    tensor_spec embedding_input_spec_{};
    bool has_embedding_input_spec_{false};
    bool is_configured_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_CASCADE_COORDINATOR_HPP
