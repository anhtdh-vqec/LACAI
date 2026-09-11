#ifndef VQEC_VISION_AI_APPL_RUNTIME_EXECUTOR_HPP
#define VQEC_VISION_AI_APPL_RUNTIME_EXECUTOR_HPP

#include <array>
#include <cstdint>

#include "vqec_vision_application_composition.hpp"
#include "vqec_vision_multi_model_feature_pipeline.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_output_gate.hpp"
#include "vqec/vision/ai/ports/vqec_vision_feature_event_sink.hpp"

namespace vqec::vision::ai {

struct runtime_executor_report {
    std::uint16_t source_index_{g_invalid_source_index};
    std::uint16_t model_slot_{g_invalid_model_slot};
    feature_fanout_report features_;
    // Revisions captured when the result was produced, not re-read at dispatch time, so a
    // queued event cannot be relabelled under a newer policy/catalog/deployment.
    std::uint64_t captured_policy_revision_{0};
    std::uint64_t captured_catalog_revision_{0};
    std::uint64_t captured_deployment_revision_{0};
    status_code first_error_code_{status_code::ok};
    bool has_tracked_{false};
    bool has_feature_fanout_{false};
};

struct feature_dispatch_report {
    std::uint32_t attempted_{0};
    std::uint32_t delivered_{0};
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
        std::uint16_t _source_count) noexcept;
    runtime_executor(const runtime_executor& _other) = delete;
    runtime_executor& operator=(const runtime_executor& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_rtexe_step(
        std::uint64_t _steady_now_ns, runtime_executor_report& _report);
    [[nodiscard]] status vqec_vision_ai_appl_rtexe_take_result(
        std::array<observation_batch, deployment_limits::g_max_models_per_source>& _tracked,
        std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages>& _events,
        runtime_executor_report& _report);
    // Explicitly drop the retained routed result during drain. The recorded error stays
    // available in the report returned by step/take; no output is published.
    void vqec_vision_ai_appl_rtexe_discard_pending() noexcept;
    [[nodiscard]] status vqec_vision_ai_appl_rtexe_request_stop(
        std::uint64_t _steady_now_ns);
    // Optional output boundary. Delivered events are re-authorized against the bound
    // gate; taking a result is not itself permission to publish. Both are borrowed.
    void vqec_vision_ai_appl_rtexe_bind_event_delivery(
        output_gate& _gate, feature_event_sink_port& _sink) noexcept;
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
    [[nodiscard]] bool vqec_vision_ai_appl_rtexe_has_pending() const noexcept;

private:
    application_composition& composition_;
    std::array<multi_model_feature_pipeline*, deployment_limits::g_max_sources>
        pipelines_{};
    std::array<observation_batch, deployment_limits::g_max_models_per_source>
        pending_tracked_{};
    std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages>
        pending_events_{};
    runtime_executor_report pending_report_;
    output_gate* delivery_gate_{nullptr};
    feature_event_sink_port* delivery_sink_{nullptr};
    std::uint16_t source_count_{0};
    std::uint64_t last_now_ns_{0};
    bool has_pending_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_RUNTIME_EXECUTOR_HPP
