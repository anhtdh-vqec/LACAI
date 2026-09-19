#ifndef VQEC_VISION_AI_APP_MULTI_MODEL_SESSION_HPP
#define VQEC_VISION_AI_APP_MULTI_MODEL_SESSION_HPP

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "vqec_vision_cascade_frame_store.hpp"
#include "vqec_vision_multi_model_pump.hpp"
#include "vqec_vision_source_session.hpp"
#include "vqec/vision/ai/ports/media/vqec_vision_cascade_frame_lease.hpp"

namespace vqec::vision::ai {

enum class multi_model_session_state {
    idle,
    acquiring,
    awaiting_first_frame,
    configuring,
    loading,
    binding,
    starting,
    running,
    draining_graphs,
    releasing_source,
    stopped
};

struct multi_model_graph_config {
    inference_graph_port* graph_{nullptr};
    // Optional neutral preprocessing for a backend that does not preprocess pixels.
    image_processor_port* processor_{nullptr};
    inference_plan plan_;
    source_binding binding_;
    std::vector<tensor_spec> outputs_;
    std::uint64_t max_output_bytes_{0};
    std::uint64_t cycle_id_{0};
    std::uint64_t job_timeout_ns_{1000000000};
    // True when this model's decoded results may spawn secondary tasks that need the exact
    // source frame; the session retains such frames until the dependents complete.
    bool cascade_root_{false};
};

// Explicit stop semantics for a result that becomes ready while graphs drain. A model
// hot-swap/update may need the last result; a shutdown may not. This is never implicit.
enum class multi_model_drain_policy {
    // Reconcile ownership and discard the business result (default shutdown behavior).
    drain_and_discard,
    // Retain the last completed result for the caller via take_drain_result.
    drain_and_deliver
};

struct multi_model_session_config {
    std::array<multi_model_graph_config, deployment_limits::g_max_models_per_source>
        graphs_{};
    model_cadence_config cadence_;
    std::uint64_t startup_timeout_ns_{30000000000};
    std::uint64_t stop_timeout_ns_{10000000000};
    std::uint16_t graph_count_{0};
    int rpc_timeout_ms_{1000};
    multi_model_drain_policy drain_policy_{multi_model_drain_policy::drain_and_discard};
    // Source identity used to key retained cascade frames.
    std::uint32_t camera_id_{0};
    std::uint32_t channel_id_{0};
    // Cascade retention budget. Required (and must be nonzero) when any graph is a cascade
    // root; ignored otherwise. Sized by composition from the admitted workload.
    std::size_t cascade_frames_{0};
    std::size_t cascade_tasks_per_frame_{0};
    std::uint64_t cascade_max_bytes_{0};
    // One persistent worker per active root model permits independent vendor graph owners
    // to preprocess/execute concurrently. False preserves serialized deterministic tests.
    bool use_model_workers_{false};
};

struct multi_model_session_snapshot {
    std::array<inference_graph_state, deployment_limits::g_max_models_per_source>
        graph_states_{};
    multi_model_session_state session_state_{multi_model_session_state::idle};
    raw_source_state source_state_{raw_source_state::idle};
    status_code first_error_code_{status_code::ok};
    unsigned outstanding_jobs_{0};
    unsigned source_readers_{0};
    std::uint16_t graph_count_{0};
    std::uint16_t running_graph_count_{0};
    std::uint64_t cascade_bytes_{0};
    bool is_recovery_required_{false};
};

// One acquisition cycle for 1..16 borrowed graphs. Calls are serialized and monotonic.
// Explicit stopped state is required before destroying source, graph or retention owners.
class multi_model_session final : public source_session_port,
                                  public cascade_frame_lease_port {
public:
    multi_model_session(raw_source_port& _source, multi_model_session_config _config);
    multi_model_session(const multi_model_session& _other) = delete;
    multi_model_session& operator=(const multi_model_session& _other) = delete;
    [[nodiscard]] status vqec_vision_ai_appl_mmses_step(
        std::uint64_t _steady_now_ns, tensor_result& _result,
        source_session_progress& _progress);
    [[nodiscard]] status vqec_vision_ai_appl_mmses_request_stop(
        std::uint64_t _steady_now_ns);
    [[nodiscard]] multi_model_session_snapshot
    vqec_vision_ai_appl_mmses_get_snapshot() const noexcept;
    // Moves out the result retained by a drain_and_deliver stop, then clears the slot.
    // Returns pending when the policy discarded it or no result was ever ready.
    [[nodiscard]] status vqec_vision_ai_appl_mmses_take_drain_result(tensor_result& _result);
    [[nodiscard]] const status&
    vqec_vision_ai_appl_mmses_get_last_error() const noexcept;
    // Frame owner retained for the most recent result so an AI-owned output stage can render
    // the same pixels; released on the next result or stop.
    [[nodiscard]] const raw_frame& vqec_vision_ai_appl_mmses_get_result_frame()
        const noexcept;
    // Takes the newest camera frame independently of result cadence. Serialized caller only.
    [[nodiscard]] status vqec_vision_ai_appl_mmses_take_preview_frame(raw_frame& _frame);
    // Cascade coordinator boundary. A secondary task acquires the exact retained source
    // frame by key and completes its ticket only after the dependent device read finishes.
    [[nodiscard]] status vqec_vision_ai_appl_mmses_acquire_cascade_frame(
        const preview_frame_key& _key, raw_frame& _frame, std::uint64_t& _ticket);
    [[nodiscard]] status vqec_vision_ai_appl_mmses_complete_cascade_task(
        std::uint64_t _ticket);
    [[nodiscard]] status vqec_vision_ai_appl_mmses_retire_cascade_frame(
        const preview_frame_key& _key);
    [[nodiscard]] std::uint64_t
    vqec_vision_ai_appl_mmses_get_cascade_bytes() const noexcept;
    [[nodiscard]] bool vqec_vision_ai_appl_mmses_has_cascade_store() const noexcept;
    [[nodiscard]] status vqec_vision_ai_appl_srcsn_step(
        std::uint64_t _steady_now_ns, tensor_result& _result,
        source_session_progress& _progress) override;
    [[nodiscard]] status vqec_vision_ai_appl_srcsn_request_stop(
        std::uint64_t _steady_now_ns) override;
    [[nodiscard]] source_session_health
    vqec_vision_ai_appl_srcsn_get_health() const noexcept override;
    // cascade_frame_lease_port: the session-owned store, driven by the cascade coordinator.
    [[nodiscard]] status vqec_vision_ai_ports_cflse_acquire(
        const preview_frame_key& _key, raw_frame& _frame,
        std::uint64_t& _ticket) override;
    [[nodiscard]] status vqec_vision_ai_ports_cflse_retire(
        const preview_frame_key& _key) override;
    [[nodiscard]] status vqec_vision_ai_ports_cflse_complete(
        std::uint64_t _ticket) override;

private:
    [[nodiscard]] status vqec_vision_ai_appl_mmses_check_time(
        std::uint64_t _steady_now_ns);
    [[nodiscard]] status vqec_vision_ai_appl_mmses_prepare_activation();
    [[nodiscard]] status vqec_vision_ai_appl_mmses_start_graph();
    [[nodiscard]] status vqec_vision_ai_appl_mmses_stop_graph(
        std::uint64_t _steady_now_ns);
    void vqec_vision_ai_appl_mmses_record_error(const status& _error);
    raw_source_port& source_;
    multi_model_session_config config_;
    multi_model_pump pump_;
    std::unique_ptr<cascade_frame_store> cascade_store_;
    raw_frame last_result_frame_;
    multi_model_session_state state_{multi_model_session_state::idle};
    status last_error_;
    std::uint64_t last_now_ns_{0};
    std::uint64_t start_ns_{0};
    std::uint64_t stop_ns_{0};
    tensor_result drain_result_;
    std::uint16_t active_graph_slot_{0};
    std::uint16_t drain_graph_slot_{0};
    bool has_drain_result_{false};
    bool is_recovery_required_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_MULTI_MODEL_SESSION_HPP
