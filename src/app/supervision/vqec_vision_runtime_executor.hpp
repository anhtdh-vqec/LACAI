#ifndef VQEC_VISION_AI_APPL_RUNTIME_EXECUTOR_HPP
#define VQEC_VISION_AI_APPL_RUNTIME_EXECUTOR_HPP

#include <array>
#include <cstdint>
#include <vector>

#include "vqec_vision_cascade_coordinator.hpp"
#include "vqec_vision_cascade_execution_worker.hpp"
#include "vqec_vision_application_composition.hpp"
#include "vqec_vision_multi_model_feature_pipeline.hpp"
#include "vqec/vision/ai/contracts/output/vqec_vision_output_gate.hpp"
#include "vqec/vision/ai/ports/features/vqec_vision_feature_event_sink.hpp"

namespace vqec::vision::ai {

struct runtime_executor_report {
    std::uint16_t source_index_{g_invalid_source_index};
    std::uint16_t model_slot_{g_invalid_model_slot};
    feature_fanout_report features_;
    cascade_coordinator_report cascade_;
    // Revisions captured when the result was produced, not re-read at dispatch time, so a
    // queued event cannot be relabelled under a newer policy/catalog/deployment.
    std::uint64_t captured_policy_revision_{0};
    std::uint64_t captured_catalog_revision_{0};
    std::uint64_t captured_deployment_revision_{0};
    status_code first_error_code_{status_code::ok};
    bool has_tracked_{false};
    bool has_feature_fanout_{false};
    bool has_cascade_{false};
};

// Device-free telemetry surface for the executor. Counters are cumulative for the life of
// the executor; gauges live in the composition snapshot. This is a seed for a metrics sink,
// not a performance claim.
struct runtime_executor_metrics {
    std::uint64_t steps_{0};
    std::uint64_t results_routed_{0};
    std::uint64_t events_accepted_{0};
    std::uint64_t events_denied_{0};
    std::uint64_t events_failed_{0};
    std::uint64_t cascade_tasks_accepted_{0};
    std::uint64_t cascade_embeddings_{0};
    std::uint64_t cascade_tasks_failed_{0};
    // Routed-result latency from the pipeline PTS to the step that routed it.
    std::uint64_t end_to_end_ns_sum_{0};
    std::uint64_t end_to_end_ns_max_{0};
    std::uint64_t end_to_end_ns_min_{UINT64_MAX};
    std::uint32_t end_to_end_samples_{0};
};

struct feature_dispatch_report {
    std::uint32_t attempted_{0};
    std::uint32_t accepted_{0};
    // Authorization/policy rejections, kept distinct from transport/sink failures.
    std::uint32_t denied_{0};
    std::uint32_t failed_{0};
    status_code first_error_code_{status_code::ok};
    std::uint16_t first_error_slot_{UINT16_MAX};
};

// Serialized executor that connects the composition's pending tensor result to the
// per-source perception/feature pipeline. It owns no hardware and does not publish
// results: authorization and delivery stay downstream. At most one routed result is
// retained; further steps report pending until the caller takes it.
class runtime_executor final {
public:
    runtime_executor(application_composition& _composition,
        const std::array<multi_model_feature_pipeline*, deployment_limits::g_max_sources>&
            _pipelines,
        const std::array<std::uint16_t, deployment_limits::g_max_sources>&
            _cascade_root_slots,
        const std::array<std::uint32_t, deployment_limits::g_max_sources>& _camera_ids,
        const std::array<std::uint32_t, deployment_limits::g_max_sources>& _channel_ids,
        std::uint16_t _source_count) noexcept;
    runtime_executor(const runtime_executor& _other) = delete;
    runtime_executor& operator=(const runtime_executor& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_rtexe_step(
        std::uint64_t _steady_now_ns, runtime_executor_report& _report);
    [[nodiscard]] status vqec_vision_ai_appl_rtexe_take_result(
        std::array<observation_batch, deployment_limits::g_max_models_per_source>& _tracked,
        std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages>& _events,
        runtime_executor_report& _report);
    // Variant used by the recognition owner. Embeddings are moved out exactly once with
    // the correlated tracked observations; callers must keep them behind their policy gate.
    [[nodiscard]] status vqec_vision_ai_appl_rtexe_take_result_with_embeddings(
        std::array<observation_batch, deployment_limits::g_max_models_per_source>& _tracked,
        std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages>& _events,
        std::vector<embedding_result>& _embeddings, runtime_executor_report& _report);
    // Explicitly drop the retained routed result during drain. The recorded error stays
    // available in the report returned by step/take; no output is published.
    void vqec_vision_ai_appl_rtexe_discard_pending() noexcept;
    [[nodiscard]] status vqec_vision_ai_appl_rtexe_request_stop(
        std::uint64_t _steady_now_ns);
    // Optional output boundary. Accepted events are re-authorized against the bound gate;
    // acceptance may mean a bounded pending handoff, not durable delivery. Both are borrowed.
    void vqec_vision_ai_appl_rtexe_bind_event_delivery(
        output_gate& _gate, feature_event_sink_port& _sink) noexcept;
    // Binds the configured coordinator required by the catalog-derived cascade root.
    // Call before the first step. One coordinator is owned by the caller per source.
    [[nodiscard]] status vqec_vision_ai_appl_rtexe_bind_cascade(
        std::uint16_t _source_index, std::uint16_t _model_slot,
        cascade_coordinator& _coordinator, std::size_t _max_results);
    [[nodiscard]] status vqec_vision_ai_appl_rtexe_bind_cascade_worker(
        std::uint16_t _source_index, std::uint16_t _model_slot,
        cascade_execution_worker& _worker);
    // _policy_revision must be the revision captured with the result, never a freshly
    // read one. Only stages selected by _success_mask are dispatched.
    [[nodiscard]] status vqec_vision_ai_appl_rtexe_dispatch_events(
        const std::array<feature_event_batch,
            feature_fanout_limits::g_max_feature_stages>& _events,
        std::uint16_t _source_index, std::uint16_t _model_slot,
        std::uint64_t _policy_revision, std::uint32_t _success_mask,
        std::uint64_t _steady_now_ns, feature_dispatch_report& _report);
    [[nodiscard]] application_composition_snapshot
    vqec_vision_ai_appl_rtexe_get_snapshot() const noexcept;
    [[nodiscard]] runtime_executor_metrics
    vqec_vision_ai_appl_rtexe_get_metrics() const noexcept;
    [[nodiscard]] bool vqec_vision_ai_appl_rtexe_has_pending() const noexcept;

private:
    application_composition& composition_;
    std::array<multi_model_feature_pipeline*, deployment_limits::g_max_sources>
        pipelines_{};
    std::array<cascade_coordinator*, deployment_limits::g_max_sources>
        cascade_coordinators_{};
    std::array<cascade_execution_worker*, deployment_limits::g_max_sources>
        cascade_workers_{};
    std::array<std::uint16_t, deployment_limits::g_max_sources> cascade_root_slots_{};
    std::array<std::uint32_t, deployment_limits::g_max_sources> camera_ids_{};
    std::array<std::uint32_t, deployment_limits::g_max_sources> channel_ids_{};
    std::array<observation_batch, deployment_limits::g_max_models_per_source>
        pending_tracked_{};
    std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages>
        pending_events_{};
    runtime_executor_report pending_report_;
    std::vector<alignment_result> cascade_aligned_;
    std::vector<embedding_result> cascade_embeddings_;
    output_gate* delivery_gate_{nullptr};
    feature_event_sink_port* delivery_sink_{nullptr};
    std::uint16_t source_count_{0};
    std::uint64_t last_now_ns_{0};
    runtime_executor_metrics metrics_;
    bool has_pending_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_RUNTIME_EXECUTOR_HPP
