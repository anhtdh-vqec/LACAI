#ifndef VQEC_VISION_AI_PORTS_INFERENCE_GRAPH_HPP
#define VQEC_VISION_AI_PORTS_INFERENCE_GRAPH_HPP

#include <cstdint>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_inference_execution.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_inference_plan.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_preview_contract.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_source_binding.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_submission_window.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp"
#include "vqec/vision/ai/ports/vqec_vision_raw_source.hpp"

namespace vqec::vision::ai {

enum class inference_graph_state {
    empty,
    configured,
    loading,
    ready,
    starting,
    running,
    draining,
    drained,
    unloading,
    faulted
};

// Serialized owner/view of one model graph. Adapter keeps all vendor recovery state.
// submit_frame retains the input owner on acceptance; const input enables shared fan-out.
class inference_graph_port {
public:
    virtual ~inference_graph_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_ports_infgr_validate_activation()
        const = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_infgr_configure(
        const inference_plan& _plan) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_infgr_load() = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_infgr_poll_state() = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_infgr_bind_source(
        const source_binding& _binding) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_infgr_start(
        const std::vector<tensor_spec>& _outputs,
        std::uint64_t _max_output_bytes) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_infgr_arm(
        std::uint64_t _cycle_id, std::uint64_t _source_epoch,
        std::uint64_t _job_timeout_ns) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_infgr_submit_frame(
        const raw_frame& _frame, std::uint64_t _steady_now_ns,
        submission_ticket& _ticket) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_infgr_poll_result(
        std::uint64_t _steady_now_ns, tensor_result& _result) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_infgr_request_drain() = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_infgr_unload() = 0;
    [[nodiscard]] virtual inference_graph_state
    vqec_vision_ai_ports_infgr_get_state() const noexcept = 0;
    [[nodiscard]] virtual unsigned
    vqec_vision_ai_ports_infgr_get_outstanding() const noexcept = 0;
    [[nodiscard]] virtual submission_ticket
    vqec_vision_ai_ports_infgr_get_pending_ticket() const noexcept = 0;

    // Tensor submission for a backend that does not preprocess pixels itself. The caller
    // owns preprocessing and supplies the exact model input blobs plus the source frame
    // identity used to correlate the result. Default is unsupported: a raw-frame backend
    // keeps using submit_frame, and the pixel-preprocessing path stays neutral.
    [[nodiscard]] virtual status vqec_vision_ai_ports_infgr_submit_tensors(
        const preview_frame_key& _frame, const std::vector<tensor_blob>& _inputs,
        std::uint64_t _steady_now_ns, submission_ticket& _ticket) {
        (void)_frame;
        (void)_inputs;
        (void)_steady_now_ns;
        (void)_ticket;
        return {status_code::unsupported,
            "backend does not accept preprocessed tensor submission"};
    }

    // Conservative default: one synchronous graph, copy memory, float32 only. An adapter
    // overrides this to advertise accelerator optimizations it can actually satisfy.
    [[nodiscard]] virtual inference_capabilities
    vqec_vision_ai_ports_infgr_get_capabilities() const noexcept {
        inference_capabilities capabilities;
        capabilities.supported_dtype_mask_ = static_cast<std::uint32_t>(1U)
            << static_cast<std::uint32_t>(tensor_element_type::float32);
        capabilities.perf_profile_mask_ = static_cast<std::uint8_t>(
            1U << static_cast<unsigned>(inference_perf_profile::balanced));
        capabilities.graph_count_ = 1;
        capabilities.max_inflight_jobs_ = 1;
        return capabilities;
    }

    // Fail-closed by default: a request the adapter did not advertise is rejected.
    [[nodiscard]] virtual status vqec_vision_ai_ports_infgr_validate_policy(
        const inference_execution_policy& _policy) const {
        return vqec_vision_ai_core_inexe_policy_is_supported(
            _policy, vqec_vision_ai_ports_infgr_get_capabilities());
    }
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_INFERENCE_GRAPH_HPP
