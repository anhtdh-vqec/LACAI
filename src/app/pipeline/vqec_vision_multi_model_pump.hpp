#ifndef VQEC_VISION_AI_APP_MULTI_MODEL_PUMP_HPP
#define VQEC_VISION_AI_APP_MULTI_MODEL_PUMP_HPP

#include <array>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "vqec_vision_model_cadence.hpp"
#include "vqec_vision_cascade_frame_store.hpp"
#include "vqec/vision/ai/ports/inference/vqec_vision_image_processor.hpp"
#include "vqec/vision/ai/ports/inference/vqec_vision_inference_graph.hpp"
#include "vqec/vision/ai/ports/inference/vqec_vision_raw_source.hpp"

namespace vqec::vision::ai {

struct multi_model_graph_binding {
    inference_graph_port* graph_{nullptr};
    std::uint64_t cycle_id_{0};
    std::uint64_t job_timeout_ns_{0};
    // Optional neutral preprocessing for a backend that does not preprocess pixels.
    // When set, the binding's plan is required and the pump submits preprocessed tensors.
    image_processor_port* processor_{nullptr};
    const inference_plan* plan_{nullptr};
    // True when this model's decoded results may spawn secondary tasks that need the exact
    // source frame. A cascade-root submission retains the frame in the borrowed store.
    bool cascade_root_{false};
};

struct multi_model_pump_report {
    std::array<submission_ticket, deployment_limits::g_max_models_per_source>
        submitted_tickets_{};
    submission_ticket result_ticket_;
    std::uint64_t skipped_cadence_intervals_{0};
    std::uint16_t due_model_mask_{0};
    std::uint16_t submitted_model_mask_{0};
    std::uint16_t busy_model_mask_{0};
    // Slots whose due preprocessed input was parked in the one-slot mailbox because the
    // graph was still busy (latest_wins / replace_pending).
    std::uint16_t pending_model_mask_{0};
    std::uint16_t result_model_slot_{UINT16_MAX};
    std::uint16_t error_model_slot_{UINT16_MAX};
    // Cascade-root slots whose frame was retained this step, and those skipped because the
    // borrowed store had no admitted budget.
    std::uint16_t cascade_retained_model_mask_{0};
    std::uint16_t cascade_dropped_model_mask_{0};
    bool has_result_{false};
    // Frame owner retained for the reported result so an AI-owned output stage can render
    // the same pixels. Valid until the next submission for that slot or stop; it keeps the
    // source frame owner alive only for the job that produced the result.
    bool has_frame_{false};
    raw_frame frame_;
};

// One serialized source executor. Bindings are borrowed and immutable after configure.
// A received owner is shared only with graph submissions accepted during the same step.
class multi_model_pump {
public:
    explicit multi_model_pump(raw_source_port& _source);
    ~multi_model_pump() noexcept;
    multi_model_pump(const multi_model_pump& _other) = delete;
    multi_model_pump& operator=(const multi_model_pump& _other) = delete;
    [[nodiscard]] status vqec_vision_ai_appl_mmump_configure(
        const model_cadence_config& _cadence,
        const std::array<multi_model_graph_binding,
            deployment_limits::g_max_models_per_source>& _bindings,
        std::uint16_t _binding_count, bool _use_model_workers = false);
    // Resolves and caches the input tensor identity of every preprocessing binding once,
    // after its graph is loaded and before any frame is received, so the per-frame path
    // performs no metadata lookup (S04/O07). A non-preprocessing binding is unaffected.
    [[nodiscard]] status vqec_vision_ai_appl_mmump_resolve_targets();
    [[nodiscard]] status vqec_vision_ai_appl_mmump_resolve_model_target(
        std::uint16_t _model_slot);
    // Serialized activation boundary. A disabled slot accepts no new work, but an already
    // submitted job is still polled to real completion. Newly enabled slots must already be
    // running; lifecycle remains owned by multi_model_session.
    [[nodiscard]] status vqec_vision_ai_appl_mmump_set_active_model_mask(
        std::uint16_t _active_model_mask);
    // Clears one inactive, fully completed slot before graph unload/reload. It never treats
    // disable or timeout as hardware completion.
    [[nodiscard]] status vqec_vision_ai_appl_mmump_release_model_slot(
        std::uint16_t _model_slot);
    [[nodiscard]] std::uint16_t
    vqec_vision_ai_appl_mmump_get_active_model_mask() const noexcept;
    [[nodiscard]] bool vqec_vision_ai_appl_mmump_has_model_worker_work(
        std::uint16_t _model_slot) const noexcept;
    // Binds the session-owned cascade frame store before the first frame is received. The
    // pump borrows it and retains cascade-root frames under the source key. It does not
    // complete or retire entries on stop; the session owns that drain.
    [[nodiscard]] status vqec_vision_ai_appl_mmump_bind_cascade_store(
        cascade_frame_store& _store, std::uint32_t _camera_id,
        std::uint32_t _channel_id);
    [[nodiscard]] status vqec_vision_ai_appl_mmump_pump_step(
        std::uint64_t _steady_now_ns, tensor_result& _result,
        multi_model_pump_report& _report);
    // Takes the newest RAW frame received by the pump independently of model cadence.
    // The one-slot mailbox is latest-wins and keeps the source owner alive until take.
    [[nodiscard]] status vqec_vision_ai_appl_mmump_take_preview_frame(
        raw_frame& _frame);
    void vqec_vision_ai_appl_mmump_begin_stop() noexcept;
    [[nodiscard]] std::uint16_t
    vqec_vision_ai_appl_mmump_get_model_count() const noexcept;
    [[nodiscard]] bool vqec_vision_ai_appl_mmump_has_failed() const noexcept;

private:
    struct model_worker_job {
        raw_frame frame_;
        submission_ticket ticket_;
        status result_;
        std::uint64_t steady_now_ns_{0};
        bool pending_{false};
        bool running_{false};
        bool completed_{false};
        bool exiting_{false};
    };

    void vqec_vision_ai_appl_mmump_model_worker_main(std::uint16_t _slot) noexcept;
    [[nodiscard]] status vqec_vision_ai_appl_mmump_start_model_workers();
    void vqec_vision_ai_appl_mmump_stop_model_workers() noexcept;
    [[nodiscard]] bool vqec_vision_ai_appl_mmump_model_busy(
        std::uint16_t _slot) const noexcept;
    [[nodiscard]] status vqec_vision_ai_appl_mmump_submit_model_work(
        std::uint16_t _slot, const raw_frame& _frame,
        std::uint64_t _steady_now_ns);
    [[nodiscard]] status vqec_vision_ai_appl_mmump_harvest_model_work(
        multi_model_pump_report& _report);
    // A one-slot mailbox for a graph that is still busy. The newest due frame's
    // preprocessed tensors are retained and submitted when the graph frees up. Bounded by
    // construction: one entry per model slot.
    struct pending_tensor_submission {
        std::vector<tensor_blob> blobs_;
        std::uint64_t source_epoch_{0};
        std::uint64_t source_frame_id_{0};
        std::uint64_t source_pts_ns_{0};
        bool has_{false};
    };

    [[nodiscard]] status vqec_vision_ai_appl_mmump_poll_result(
        std::uint64_t _steady_now_ns, tensor_result& _result,
        multi_model_pump_report& _report);
    [[nodiscard]] status vqec_vision_ai_appl_mmump_ensure_target(std::uint16_t _slot);
    [[nodiscard]] status vqec_vision_ai_appl_mmump_preprocess(
        std::uint16_t _slot, const raw_frame& _frame, std::vector<tensor_blob>& _blobs);
    [[nodiscard]] status vqec_vision_ai_appl_mmump_store_pending(
        std::uint16_t _slot, const raw_frame& _frame);
    [[nodiscard]] status vqec_vision_ai_appl_mmump_flush_pending(
        std::uint64_t _steady_now_ns, multi_model_pump_report& _report);
    [[nodiscard]] status vqec_vision_ai_appl_mmump_get_policy(
        std::uint16_t _slot, model_dispatch_policy& _policy) const noexcept;

    raw_source_port& source_;
    model_cadence_scheduler cadence_;
    std::array<multi_model_graph_binding, deployment_limits::g_max_models_per_source>
        bindings_{};
    std::array<bool, deployment_limits::g_max_models_per_source> is_armed_{};
    std::array<tensor_spec, deployment_limits::g_max_models_per_source> target_specs_{};
    std::array<bool, deployment_limits::g_max_models_per_source> has_target_spec_{};
    std::array<pending_tensor_submission, deployment_limits::g_max_models_per_source>
        pending_{};
    // Persistent per-binding preprocessing output. It is reused across frames because the
    // pump only preprocesses/submits a binding when its graph is not outstanding, so the
    // previous input is complete per the graph's own completion semantics.
    std::array<std::vector<tensor_blob>, deployment_limits::g_max_models_per_source>
        preprocess_buffers_{};
    std::array<model_worker_job, deployment_limits::g_max_models_per_source>
        model_worker_jobs_{};
    std::array<std::thread, deployment_limits::g_max_models_per_source>
        model_workers_{};
    mutable std::mutex model_worker_mutex_;
    std::array<std::condition_variable, deployment_limits::g_max_models_per_source>
        model_worker_conditions_{};
    // Last received source epoch; a change discards parked inputs so an old-epoch result
    // can never be submitted into a new epoch.
    std::uint64_t last_source_epoch_{0};
    std::uint64_t last_now_ns_{0};
    std::uint16_t model_count_{0};
    std::uint16_t active_model_mask_{0};
    std::uint16_t result_cursor_{0};
    // One retained frame per model slot, released when a new submission replaces it or the
    // pump stops. This is what keeps the owner alive from submission to result take.
    std::array<raw_frame, deployment_limits::g_max_models_per_source> retained_frames_{};
    raw_frame preview_frame_;
    // Borrowed session-owned cascade retention. Null means no cascade-root binding is
    // admitted; a cascade-root binding without a bound store fails closed.
    cascade_frame_store* cascade_store_{nullptr};
    std::uint32_t camera_id_{0};
    std::uint32_t channel_id_{0};
    bool has_cascade_root_{false};
    bool has_received_frame_{false};
    bool has_preview_frame_{false};
    bool use_model_workers_{false};
    bool model_workers_started_{false};
    bool is_configured_{false};
    bool is_stopping_{false};
    bool is_failed_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_MULTI_MODEL_PUMP_HPP
