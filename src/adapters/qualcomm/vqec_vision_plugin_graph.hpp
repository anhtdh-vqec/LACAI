#ifndef VQEC_VISION_AI_QUALCOMM_PLUGIN_GRAPH_HPP
#define VQEC_VISION_AI_QUALCOMM_PLUGIN_GRAPH_HPP

#include <memory>
#include <cstdint>
#include <string>
#include <vector>

#include <vqec/vision/ai/contracts/vqec_vision_inference_plan.hpp>
#include <vqec/vision/ai/contracts/vqec_vision_source_binding.hpp>
#include <vqec/vision/ai/contracts/vqec_vision_status.hpp>
#include <vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp>
#include <vqec/vision/ai/contracts/vqec_vision_frame_descriptor.hpp>
#include <vqec/vision/ai/contracts/vqec_vision_submission_window.hpp>

namespace vqec::vision::ai {

namespace graph_retention_limits {
// Bounded containment domain for armed graphs that could not be safely torn down.
inline constexpr unsigned g_slot_count = 4;
inline constexpr unsigned g_invalid_slot = g_slot_count;
}  // namespace graph_retention_limits

class plugin_graph;

// One domain per supervisor lifetime, serialized with all its graphs. Four slots.
// Keep alive until explicit unload; destruction with retained graphs leaks them for safety.
class graph_retention {
public:
    graph_retention();
    ~graph_retention() noexcept;
    graph_retention(const graph_retention& _other) = delete;
    graph_retention& operator=(const graph_retention& _other) = delete;
    [[nodiscard]] unsigned vqec_vision_ai_qcom_plgr_get_retained_count() const noexcept;
    [[nodiscard]] status vqec_vision_ai_qcom_plgr_restore_graph(
        unsigned _slot, plugin_graph& _graph, const std::shared_ptr<graph_retention>& _domain);

private:
    friend class plugin_graph;
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

enum class plugin_graph_state {
    empty, configured, loading, ready, starting, playing, draining, drained, unloading, faulted
};

struct plugin_graph_error {
    std::string source_;
    std::string domain_;
    int code_{0};
    std::string message_;
};

struct plugin_factory_capability {
    std::string factory_name_;
    bool available_{false};
};

struct plugin_property_capability {
    std::string property_name_;
    std::string value_type_name_;
    bool readable_{false};
    bool writable_{false};
    std::vector<std::string> enum_nicks_;
};

// No vendor/GStreamer types cross this header. Serialized backend-worker calls only.
// configure remains NULL-only; load_model may synchronously block in vendor SDK code.
// Explicitly unload before destruction. Armed graphs use bounded retention, not forced NULL.
// Before arm, fallback NULL teardown can still block inside the SDK.
// A failed configure preserves the previously configured graph.
// C++ allocation failures may throw; this is not a C ABI entrypoint.
class plugin_graph {
public:
    plugin_graph();
    ~plugin_graph() noexcept;

    plugin_graph(const plugin_graph& _other) = delete;
    plugin_graph& operator=(const plugin_graph& _other) = delete;
    plugin_graph(plugin_graph&& _other) = delete;
    plugin_graph& operator=(plugin_graph&& _other) = delete;

#if defined(VQEC_VISION_AI_GRAPH_TEST_FIXTURE)
    // Compiled only in the separate synthetic-test library, never a production fallback.
    [[nodiscard]] status vqec_vision_ai_qcom_plgr_configure_fixture(const inference_plan& _plan);
#endif

    [[nodiscard]] status
    vqec_vision_ai_qcom_plgr_configure_graph(const inference_plan& _plan);

    // Runtime-only inventory. The caller supplies factory names from deployment
    // policy; this method never substitutes a backend or a plugin default.
    [[nodiscard]] status vqec_vision_ai_qcom_plgr_probe_factories(
        const std::vector<std::string>& _factory_names,
        std::vector<plugin_factory_capability>& _capabilities) const;
    [[nodiscard]] status vqec_vision_ai_qcom_plgr_probe_properties(
        const std::string& _factory_name, const std::vector<std::string>& _property_names,
        std::vector<plugin_property_capability>& _capabilities) const;

    [[nodiscard]] bool vqec_vision_ai_qcom_plgr_is_configured() const noexcept;
    [[nodiscard]] status vqec_vision_ai_qcom_plgr_load_model();
    [[nodiscard]] status vqec_vision_ai_qcom_plgr_poll_state();
    [[nodiscard]] status vqec_vision_ai_qcom_plgr_unload_model();
    // READY only. Validates trusted policy and sets source caps before PLAYING.
    // Does not acquire the camera, validate evidence artifacts or synchronize memory.
    [[nodiscard]] status vqec_vision_ai_qcom_plgr_bind_source(const source_binding& _binding);
    [[nodiscard]] status vqec_vision_ai_qcom_plgr_start_stream(
        const std::vector<tensor_spec>& _outputs, std::uint64_t _max_output_bytes);
    [[nodiscard]] status vqec_vision_ai_qcom_plgr_request_drain();
    [[nodiscard]] status vqec_vision_ai_qcom_plgr_arm_submission(
        std::uint64_t _cycle_id, std::uint64_t _source_epoch, std::uint64_t _job_timeout_ns,
        const std::shared_ptr<graph_retention>& _retention);
    // Empty ticket required. Nonzero returned token means committed even on push failure.
    [[nodiscard]] status vqec_vision_ai_qcom_plgr_submit_frame(
        const frame_descriptor& _descriptor, int _frame_fd, std::shared_ptr<const void> _owner,
        std::uint64_t _steady_now_ns, submission_ticket& _ticket);
    [[nodiscard]] status vqec_vision_ai_qcom_plgr_poll_jobs(std::uint64_t _steady_now_ns);
    [[nodiscard]] unsigned vqec_vision_ai_qcom_plgr_get_outstanding() const noexcept;
    // Preferred executor entrypoint: check deadline, reconcile input and consume one result.
    // No result publication on fault. Original job correlation is available after restore.
    [[nodiscard]] status vqec_vision_ai_qcom_plgr_poll_result(
        std::uint64_t _steady_now_ns, tensor_result& _result);
    [[nodiscard]] submission_ticket vqec_vision_ai_qcom_plgr_get_pending_ticket() const noexcept;
    // Caller supplies ticket PTS, not UTC. CPU visibility must be established by BSP contract.
    // Low-level zero-wait API: caller MUST poll_jobs(now) first to enforce deadlines.
    // pending means no sample. Error leaves destination unchanged.
    [[nodiscard]] status vqec_vision_ai_qcom_plgr_poll_output(
        std::uint64_t _expected_pipeline_pts_ns, tensor_result& _result);
    [[nodiscard]] plugin_graph_state vqec_vision_ai_qcom_plgr_get_state() const noexcept;
    [[nodiscard]] plugin_graph_error vqec_vision_ai_qcom_plgr_get_last_error() const;
    [[nodiscard]] std::uint64_t vqec_vision_ai_qcom_plgr_get_warning_count() const noexcept;

private:
    friend class graph_retention;
    struct implementation;
    std::unique_ptr<implementation> implementation_;
    std::shared_ptr<graph_retention> retention_;
    unsigned retention_slot_{graph_retention_limits::g_invalid_slot};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QUALCOMM_PLUGIN_GRAPH_HPP
