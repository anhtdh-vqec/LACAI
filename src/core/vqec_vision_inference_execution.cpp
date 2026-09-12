#include "vqec/vision/ai/contracts/vqec_vision_inference_execution.hpp"

#include <limits>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

std::uint32_t vqec_vision_ai_core_inexe_dtype_bit(tensor_element_type _type) noexcept {
    return static_cast<std::uint32_t>(1U) << static_cast<std::uint32_t>(_type);
}

std::uint32_t vqec_vision_ai_core_inexe_known_dtype_mask() noexcept {
    // Unknown is ordinal 0; the reviewed element types are ordinals 1..10.
    return ((static_cast<std::uint32_t>(1U) << 11U) - 2U);
}

std::uint8_t vqec_vision_ai_core_inexe_profile_bit(
    inference_perf_profile _profile) noexcept {
    return static_cast<std::uint8_t>(1U << static_cast<unsigned>(_profile));
}

std::uint8_t vqec_vision_ai_core_inexe_valid_profile_mask() noexcept {
    return static_cast<std::uint8_t>(0x0FU);  // four reviewed profiles
}

bool vqec_vision_ai_core_inexe_is_valid_mode(inference_execution_mode _mode) noexcept {
    return _mode == inference_execution_mode::synchronous ||
        _mode == inference_execution_mode::asynchronous;
}

bool vqec_vision_ai_core_inexe_is_valid_memory(
    inference_memory_mode _memory) noexcept {
    return _memory == inference_memory_mode::copy ||
        _memory == inference_memory_mode::registered_shared;
}

bool vqec_vision_ai_core_inexe_is_valid_profile(
    inference_perf_profile _profile) noexcept {
    switch (_profile) {
        case inference_perf_profile::low_latency:
        case inference_perf_profile::balanced:
        case inference_perf_profile::high_throughput:
        case inference_perf_profile::sustained:
            return true;
    }
    return false;
}

std::uint32_t vqec_vision_ai_core_inexe_unit_mask(std::uint8_t _count) noexcept {
    if (_count == 0) {
        return 0;
    }
    if (_count >= 32U) {
        return std::numeric_limits<std::uint32_t>::max();
    }
    return (static_cast<std::uint32_t>(1U) << _count) - 1U;
}

}  // namespace

status vqec_vision_ai_core_inexe_validate_capabilities(
    const inference_capabilities& _capabilities) noexcept {
    if (_capabilities.graph_count_ == 0) {
        return {status_code::invalid_argument, "capabilities must expose at least one graph"};
    }
    if (_capabilities.perf_profile_mask_ == 0 ||
        (_capabilities.perf_profile_mask_ &
            static_cast<std::uint8_t>(~vqec_vision_ai_core_inexe_valid_profile_mask())) != 0) {
        return {status_code::invalid_argument, "capability perf-profile mask is invalid"};
    }
    if (_capabilities.supported_dtype_mask_ == 0 ||
        (_capabilities.supported_dtype_mask_ &
            ~vqec_vision_ai_core_inexe_known_dtype_mask()) != 0) {
        return {status_code::invalid_argument, "capability dtype mask is invalid"};
    }
    if (_capabilities.compute_unit_count_ > inference_execution_limits::g_max_compute_units) {
        return {status_code::invalid_argument, "capability compute-unit count exceeds limit"};
    }
    if (_capabilities.max_inflight_jobs_ == 0 ||
        _capabilities.max_inflight_jobs_ > inference_execution_limits::g_max_inflight_jobs) {
        return {status_code::invalid_argument, "capability inflight bound is invalid"};
    }
    if (_capabilities.max_shared_registrations_ >
        inference_execution_limits::g_max_shared_registrations) {
        return {status_code::invalid_argument, "capability shared-registration bound exceeds limit"};
    }
    if (_capabilities.supports_shared_memory_ !=
        (_capabilities.max_shared_registrations_ > 0)) {
        return {status_code::invalid_argument,
            "shared-memory support and registration bound disagree"};
    }
    if (!_capabilities.supports_async_ && _capabilities.max_inflight_jobs_ != 1) {
        return {status_code::invalid_argument,
            "synchronous capability cannot allow more than one inflight job"};
    }
    return {};
}

status vqec_vision_ai_core_inexe_validate_domain(
    const inference_execution_domain& _domain) noexcept {
    if (!vqec_vision_ai_cntr_ident_is_valid(
            _domain.domain_id_, inference_execution_limits::g_max_domain_id_bytes)) {
        return {status_code::invalid_argument, "execution domain id is invalid"};
    }
    if (_domain.max_graphs_ == 0 ||
        _domain.max_graphs_ > inference_execution_limits::g_max_domain_graphs) {
        return {status_code::invalid_argument, "execution domain graph capacity is invalid"};
    }
    if (_domain.max_inflight_jobs_ == 0 ||
        _domain.max_inflight_jobs_ > inference_execution_limits::g_max_inflight_jobs) {
        return {status_code::invalid_argument, "execution domain inflight bound is invalid"};
    }
    return {};
}

status vqec_vision_ai_core_inexe_domain_admits(
    const inference_execution_domain& _domain,
    const inference_capabilities& _domain_capabilities,
    std::uint16_t _graph_count, std::uint32_t _total_inflight_jobs) noexcept {
    const auto valid_domain = vqec_vision_ai_core_inexe_validate_domain(_domain);
    if (valid_domain.code_ != status_code::ok) {
        return valid_domain;
    }
    const auto valid_capabilities =
        vqec_vision_ai_core_inexe_validate_capabilities(_domain_capabilities);
    if (valid_capabilities.code_ != status_code::ok) {
        return valid_capabilities;
    }
    if (_graph_count == 0 || _total_inflight_jobs == 0) {
        return {status_code::invalid_argument, "domain admission requires graphs and jobs"};
    }
    if (_graph_count > _domain.max_graphs_) {
        return {status_code::resource_exhausted, "domain graph capacity exceeded"};
    }
    if (_graph_count > 1 &&
        (!_domain.allows_shared_context_ ||
            !_domain_capabilities.supports_multi_model_domain_)) {
        return {status_code::unsupported,
            "multi-graph domain requires shared-context support"};
    }
    if (_total_inflight_jobs > _domain.max_inflight_jobs_) {
        return {status_code::resource_exhausted, "domain inflight bound exceeded"};
    }
    const auto aggregate =
        static_cast<std::uint64_t>(_domain_capabilities.max_inflight_jobs_) * _graph_count;
    if (static_cast<std::uint64_t>(_total_inflight_jobs) > aggregate) {
        return {status_code::resource_exhausted,
            "aggregate inflight exceeds backend capacity"};
    }
    return {};
}

status vqec_vision_ai_core_inexe_validate_policy(
    const inference_execution_policy& _policy) noexcept {
    if (!vqec_vision_ai_core_inexe_is_valid_mode(_policy.mode_) ||
        !vqec_vision_ai_core_inexe_is_valid_memory(_policy.memory_) ||
        !vqec_vision_ai_core_inexe_is_valid_profile(_policy.profile_)) {
        return {status_code::invalid_argument, "execution policy enum is invalid"};
    }
    if (_policy.max_inflight_jobs_ == 0 ||
        _policy.max_inflight_jobs_ > inference_execution_limits::g_max_inflight_jobs) {
        return {status_code::invalid_argument, "execution policy inflight bound is invalid"};
    }
    if (_policy.mode_ == inference_execution_mode::synchronous &&
        _policy.max_inflight_jobs_ != 1) {
        return {status_code::invalid_argument,
            "synchronous policy must keep exactly one inflight job"};
    }
    if (_policy.compute_unit_count_ > inference_execution_limits::g_max_compute_units) {
        return {status_code::invalid_argument, "execution policy compute-unit count exceeds limit"};
    }
    if (_policy.compute_unit_count_ != 0 &&
        (_policy.compute_unit_affinity_ &
            ~vqec_vision_ai_core_inexe_unit_mask(_policy.compute_unit_count_)) != 0) {
        return {status_code::invalid_argument, "execution policy affinity exceeds unit count"};
    }
    if (_policy.priority_ > inference_execution_limits::g_max_priority) {
        return {status_code::invalid_argument, "execution policy priority exceeds limit"};
    }
    return {};
}

status vqec_vision_ai_core_inexe_policy_is_supported(
    const inference_execution_policy& _policy,
    const inference_capabilities& _capabilities) noexcept {
    const auto valid_policy = vqec_vision_ai_core_inexe_validate_policy(_policy);
    if (valid_policy.code_ != status_code::ok) {
        return valid_policy;
    }
    const auto valid_capabilities =
        vqec_vision_ai_core_inexe_validate_capabilities(_capabilities);
    if (valid_capabilities.code_ != status_code::ok) {
        return valid_capabilities;
    }
    if (_policy.mode_ == inference_execution_mode::asynchronous &&
        !_capabilities.supports_async_) {
        return {status_code::unsupported, "backend does not support asynchronous execution"};
    }
    if (_policy.memory_ == inference_memory_mode::registered_shared &&
        !_capabilities.supports_shared_memory_) {
        return {status_code::unsupported, "backend does not support shared/registered memory"};
    }
    if (_policy.prefer_native_output_ && !_capabilities.supports_native_output_) {
        return {status_code::unsupported, "backend does not support native output dtype"};
    }
    if ((_capabilities.perf_profile_mask_ &
            vqec_vision_ai_core_inexe_profile_bit(_policy.profile_)) == 0) {
        return {status_code::unsupported, "backend does not support the requested perf profile"};
    }
    if ((_policy.compute_unit_count_ != 0 || _policy.compute_unit_affinity_ != 0) &&
        _capabilities.compute_unit_count_ == 0) {
        return {status_code::unsupported,
            "backend does not advertise accelerator compute-unit topology"};
    }
    if (_policy.compute_unit_count_ > _capabilities.compute_unit_count_) {
        return {status_code::unsupported,
            "requested compute-unit count exceeds the advertised topology"};
    }
    if ((_policy.compute_unit_affinity_ &
            ~vqec_vision_ai_core_inexe_unit_mask(_capabilities.compute_unit_count_)) != 0) {
        return {status_code::unsupported,
            "requested affinity selects an unadvertised compute unit"};
    }
    if (_policy.max_inflight_jobs_ > _capabilities.max_inflight_jobs_) {
        return {status_code::unsupported, "requested inflight bound exceeds backend capacity"};
    }
    return {};
}

bool vqec_vision_ai_core_inexe_dtype_supported(
    const inference_capabilities& _capabilities, tensor_element_type _dtype) noexcept {
    if (_dtype == tensor_element_type::unknown) {
        return false;
    }
    return (_capabilities.supported_dtype_mask_ &
        vqec_vision_ai_core_inexe_dtype_bit(_dtype)) != 0;
}

status vqec_vision_ai_core_inexe_validate_shared_buffer(
    const inference_shared_buffer& _buffer) noexcept {
    if (_buffer.registration_id_ == 0 || _buffer.allocation_id_ == 0 ||
        _buffer.generation_ == 0) {
        return {status_code::invalid_argument,
            "shared buffer registration/allocation/generation must be nonzero"};
    }
    if (_buffer.bytes_ == 0 ||
        _buffer.bytes_ > inference_execution_limits::g_max_shared_buffer_bytes) {
        return {status_code::invalid_argument, "shared buffer size is invalid"};
    }
    if (_buffer.access_ != shared_buffer_access::read_only &&
        _buffer.access_ != shared_buffer_access::read_write) {
        return {status_code::invalid_argument, "shared buffer access mode is invalid"};
    }
    return {};
}

status vqec_vision_ai_core_inexe_shared_buffer_supported(
    const inference_shared_buffer& _buffer,
    const inference_capabilities& _capabilities,
    std::uint16_t _active_registrations) noexcept {
    const auto valid_buffer = vqec_vision_ai_core_inexe_validate_shared_buffer(_buffer);
    if (valid_buffer.code_ != status_code::ok) {
        return valid_buffer;
    }
    const auto valid_capabilities =
        vqec_vision_ai_core_inexe_validate_capabilities(_capabilities);
    if (valid_capabilities.code_ != status_code::ok) {
        return valid_capabilities;
    }
    if (!_capabilities.supports_shared_memory_) {
        return {status_code::unsupported, "backend does not support shared memory"};
    }
    if (_active_registrations >= _capabilities.max_shared_registrations_) {
        return {status_code::resource_exhausted, "shared buffer registration capacity reached"};
    }
    return {};
}

}  // namespace vqec::vision::ai
