#ifndef VQEC_VISION_AI_APPL_CASCADE_COORDINATOR_HPP
#define VQEC_VISION_AI_APPL_CASCADE_COORDINATOR_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "vqec/vision/ai/contracts/perception/vqec_vision_embedding.hpp"
#include "vqec/vision/ai/contracts/media/vqec_vision_image_alignment.hpp"
#include "vqec/vision/ai/contracts/perception/vqec_vision_observation.hpp"
#include "vqec/vision/ai/ports/media/vqec_vision_cascade_frame_lease.hpp"
#include "vqec/vision/ai/ports/perception/vqec_vision_embedding_decoder.hpp"
#include "vqec/vision/ai/ports/inference/vqec_vision_image_alignment.hpp"
#include "vqec/vision/ai/ports/inference/vqec_vision_inference_graph.hpp"
#include "vqec/vision/ai/ports/perception/vqec_vision_face_image_inference.hpp"

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
    // Submission-window identity and deadline for the secondary graph. The coordinator
    // arms the graph once for its source epoch before the first tensor submission.
    std::uint64_t cycle_id_{0};
    std::uint64_t job_timeout_ns_{0};
    // Maximum faces aligned per primary frame.
    std::size_t max_tasks_per_frame_{0};
    // Optional steady-nanosecond budget per frame batch to preserve control response.
    // When nonzero, remaining unstarted tasks in the batch are skipped once elapsed >= budget.
    std::uint64_t control_budget_ns_{0};
    // Optional minimum interval between embeddings of the same track_id.
    // When nonzero, a tracked face already embedded within this interval is skipped.
    std::uint64_t track_refresh_interval_ns_{0};
};

namespace cascade_coordinator_limits {
inline constexpr std::uint64_t g_default_control_budget_ns = 25000000ULL;  // 25 ms
inline constexpr std::uint64_t g_default_track_refresh_interval_ns = 3000000000ULL;  // 3 s
inline constexpr std::size_t g_max_tracked_faces = 256;
inline constexpr std::uint64_t g_tracked_face_retention_ns = 10000000000ULL;  // 10 s
}  // namespace cascade_coordinator_limits

struct cascade_coordinator_report {
    std::uint16_t accepted_{0};
    std::uint16_t embedded_{0};
    std::uint16_t skipped_{0};
    std::uint16_t failed_{0};
};

struct cascade_coordinator_metrics {
    std::uint64_t oldest_job_ns_{0};
    std::uint64_t stop_duration_ns_{0};
    std::size_t active_tasks_{0};
    std::size_t queue_depth_{0};
    std::size_t quarantine_count_{0};
    status_code root_backend_error_code_{status_code::ok};
    std::uint64_t tasks_accepted_{0};
    std::uint64_t tasks_embedded_{0};
    std::uint64_t tasks_skipped_{0};
    std::uint64_t tasks_failed_{0};
};

// Serialized cascade coordinator. For one decoded primary observation batch it acquires the
// exact retained frame once per admitted face, aligns each, optionally quantizes and runs the
// secondary embedding graph, completes each ticket and closes admission. A per-task failure
// is counted and isolated; it never faults the primary path. Synchronous secondary backend in
// this slice; async and the bounded worker are optimization.
class cascade_coordinator : public face_image_cascade_port {
public:
    cascade_coordinator() = default;
    [[nodiscard]] status vqec_vision_ai_appl_cscrd_configure(
        const cascade_coordinator_config& _config);
    [[nodiscard]] status vqec_vision_ai_appl_cscrd_process(
        std::uint64_t _steady_now_ns, const observation_batch& _tracked,
        std::vector<alignment_result>& _aligned,
        std::vector<embedding_result>& _embeddings,
        cascade_coordinator_report& _report);
    // File/offline path: the caller already owns the exact frame represented by _tracked.
    [[nodiscard]] status vqec_vision_ai_appl_cscrd_process_frame(
        std::uint64_t _steady_now_ns, const raw_frame& _frame,
        const observation_batch& _tracked, std::vector<alignment_result>& _aligned,
        std::vector<embedding_result>& _embeddings,
        cascade_coordinator_report& _report);
    [[nodiscard]] status vqec_vision_ai_ports_ficas_run(
        const raw_frame& _frame, const observation_batch& _detections,
        std::uint64_t _steady_now_ns, std::vector<embedding_result>& _embeddings,
        std::size_t& _failed_tasks) override;
    [[nodiscard]] bool vqec_vision_ai_appl_cscrd_is_configured() const noexcept;
    [[nodiscard]] const status&
    vqec_vision_ai_appl_cscrd_get_last_task_error() const noexcept;
    [[nodiscard]] status vqec_vision_ai_appl_cscrd_request_stop(
        std::uint64_t _steady_now_ns);
    [[nodiscard]] bool vqec_vision_ai_appl_cscrd_is_stopping() const noexcept;
    [[nodiscard]] cascade_coordinator_metrics
    vqec_vision_ai_appl_cscrd_get_metrics() const noexcept;

private:
    [[nodiscard]] status vqec_vision_ai_appl_cscrd_process_with_lease(
        std::uint64_t _steady_now_ns, cascade_frame_lease_port& _lease,
        const observation_batch& _tracked, std::vector<alignment_result>& _aligned,
        std::vector<embedding_result>& _embeddings,
        cascade_coordinator_report& _report);
    image_alignment_port* aligner_{nullptr};
    cascade_frame_lease_port* lease_{nullptr};
    inference_graph_port* embedding_graph_{nullptr};
    embedding_decoder_port* embedding_decoder_{nullptr};
    alignment_template template_;
    alignment_capabilities capabilities_{};
    std::array<float, 3> normalize_offset_{0.0F, 0.0F, 0.0F};
    std::array<float, 3> normalize_scale_{1.0F, 1.0F, 1.0F};
    std::uint64_t cycle_id_{0};
    std::uint64_t job_timeout_ns_{0};
    std::uint64_t armed_source_epoch_{0};
    std::size_t max_tasks_per_frame_{0};
    std::uint64_t control_budget_ns_{0};
    std::uint64_t track_refresh_interval_ns_{0};
    std::unordered_map<std::uint64_t, std::uint64_t> last_embedded_track_ns_;
    std::uint64_t stop_ns_{0};
    bool is_stopping_{false};
    cascade_coordinator_metrics metrics_{};
    // Full model input spec taken from the loaded graph (name, dims, dtype, quantization).
    tensor_spec embedding_input_spec_{};
    // The coordinator is serialized, so one activation-sized quantization workspace and
    // one graph-input vector can be reused for every admitted face without frame-path
    // allocation or an initializer-list deep copy of the tensor payload.
    std::vector<std::uint16_t> embedding_quantized_workspace_;
    std::vector<tensor_blob> embedding_inputs_;
    status last_task_error_{};
    bool has_embedding_input_spec_{false};
    bool is_configured_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_CASCADE_COORDINATOR_HPP
