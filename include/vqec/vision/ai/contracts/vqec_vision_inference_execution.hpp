#ifndef VQEC_VISION_AI_CONTRACTS_INFERENCE_EXECUTION_HPP
#define VQEC_VISION_AI_CONTRACTS_INFERENCE_EXECUTION_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp"

namespace vqec::vision::ai {

namespace inference_execution_limits {
inline constexpr std::uint8_t g_max_compute_units = 8;
inline constexpr std::uint16_t g_max_inflight_jobs = 16;
inline constexpr std::uint16_t g_max_shared_registrations = 64;
inline constexpr std::uint32_t g_max_priority = 7;
inline constexpr std::size_t g_max_domain_id_bytes = 128;
inline constexpr std::uint16_t g_max_domain_graphs = 64;
}  // namespace inference_execution_limits

// How a model graph executes relative to its caller. Vendor-neutral; an adapter maps it
// to its own execution mechanism without leaking vendor types into this contract.
enum class inference_execution_mode { synchronous, asynchronous };

// Where model input/output tensors live for a submission. `registered_shared` requests
// imported/registered buffers (a zero-copy candidate), never a promise of zero-copy.
enum class inference_memory_mode { copy, registered_shared };

// Coarse latency/throughput intent. This is not a vendor profile name and an unsupported
// profile must be rejected, never silently downgraded.
enum class inference_perf_profile { low_latency, balanced, high_throughput, sustained };

// Adapter-probed execution capabilities for one graph or one multi-model execution domain.
// The runtime validates a requested policy against these before any resource is acquired.
struct inference_capabilities {
    // One bit per tensor_element_type ordinal.
    std::uint32_t supported_dtype_mask_{0};
    // One bit per inference_perf_profile ordinal; must be nonzero.
    std::uint8_t perf_profile_mask_{0};
    // Accelerator compute units exposed for affinity; 0 means topology not advertised.
    std::uint8_t compute_unit_count_{0};
    // Graphs the domain can host; a single-graph port advertises one.
    std::uint16_t graph_count_{0};
    std::uint16_t max_inflight_jobs_{1};
    std::uint16_t max_shared_registrations_{0};
    bool supports_async_{false};
    bool supports_native_output_{false};
    bool supports_shared_memory_{false};
    bool supports_artifact_update_{false};
    bool supports_multi_model_domain_{false};
};

// Validated execution intent from deployment/model catalog. Always vendor-neutral.
struct inference_execution_policy {
    inference_execution_mode mode_{inference_execution_mode::synchronous};
    inference_memory_mode memory_{inference_memory_mode::copy};
    inference_perf_profile profile_{inference_perf_profile::balanced};
    // Bitmask over compute units; 0 selects the adapter default topology.
    std::uint32_t compute_unit_affinity_{0};
    // Requested unit count; 0 selects the adapter default.
    std::uint8_t compute_unit_count_{0};
    std::uint16_t max_inflight_jobs_{1};
    std::uint32_t priority_{0};
    bool prefer_native_output_{false};
};

// One shared accelerator resource domain that several model graphs may bind to (one
// backend/device/context). Identity and capacity only; the adapter owns the real domain
// object and the runtime admits the aggregate against it.
struct inference_execution_domain {
    std::string domain_id_;
    std::uint16_t max_graphs_{0};
    std::uint16_t max_inflight_jobs_{0};
    bool allows_shared_context_{false};
};

// Structural validation only; no I/O, allocation or vendor call.
[[nodiscard]] status vqec_vision_ai_core_inexe_validate_capabilities(
    const inference_capabilities& _capabilities) noexcept;

[[nodiscard]] status vqec_vision_ai_core_inexe_validate_domain(
    const inference_execution_domain& _domain) noexcept;

// Admits a graph set and its aggregate inflight bound into a domain owned by a backend
// whose capabilities are supplied. Multi-graph admission requires shared-context support.
[[nodiscard]] status vqec_vision_ai_core_inexe_domain_admits(
    const inference_execution_domain& _domain,
    const inference_capabilities& _domain_capabilities,
    std::uint16_t _graph_count, std::uint32_t _total_inflight_jobs) noexcept;

[[nodiscard]] status vqec_vision_ai_core_inexe_validate_policy(
    const inference_execution_policy& _policy) noexcept;

// Fail-closed: rejects any policy a capability cannot satisfy. It never downgrades a
// requested mode, memory, profile, native output, affinity or inflight bound silently.
[[nodiscard]] status vqec_vision_ai_core_inexe_policy_is_supported(
    const inference_execution_policy& _policy,
    const inference_capabilities& _capabilities) noexcept;

[[nodiscard]] bool vqec_vision_ai_core_inexe_dtype_supported(
    const inference_capabilities& _capabilities, tensor_element_type _dtype) noexcept;

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_INFERENCE_EXECUTION_HPP
