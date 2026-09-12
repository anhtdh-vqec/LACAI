#include <cstdint>
#include <iostream>

#include "vqec/vision/ai/contracts/vqec_vision_inference_execution.hpp"

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    const auto dtype_bit = [](tensor_element_type _type) {
        return static_cast<std::uint32_t>(1U) << static_cast<std::uint32_t>(_type);
    };
    const auto full_dtype_mask = [&]() {
        return dtype_bit(tensor_element_type::int8) |
            dtype_bit(tensor_element_type::uint8) |
            dtype_bit(tensor_element_type::float16) |
            dtype_bit(tensor_element_type::float32);
    };

    inference_capabilities capabilities;
    capabilities.supported_dtype_mask_ = full_dtype_mask();
    capabilities.perf_profile_mask_ =
        static_cast<std::uint8_t>(0x03U);  // low_latency + balanced
    capabilities.compute_unit_count_ = 4;
    capabilities.graph_count_ = 1;
    capabilities.max_inflight_jobs_ = 4;
    capabilities.max_shared_registrations_ = 8;
    capabilities.supports_async_ = true;
    capabilities.supports_native_output_ = true;
    capabilities.supports_shared_memory_ = true;
    capabilities.supports_artifact_update_ = true;
    capabilities.supports_multi_model_domain_ = true;
    check(vqec_vision_ai_core_inexe_validate_capabilities(capabilities).code_ ==
          status_code::ok);

    check(vqec_vision_ai_core_inexe_dtype_supported(
              capabilities, tensor_element_type::float32));
    check(!vqec_vision_ai_core_inexe_dtype_supported(
              capabilities, tensor_element_type::int32));
    check(!vqec_vision_ai_core_inexe_dtype_supported(
              capabilities, tensor_element_type::unknown));

    // Capability structural validation.
    {
        auto bad = capabilities;
        bad.graph_count_ = 0;
        check(vqec_vision_ai_core_inexe_validate_capabilities(bad).code_ ==
              status_code::invalid_argument);
        bad = capabilities;
        bad.perf_profile_mask_ = 0;
        check(vqec_vision_ai_core_inexe_validate_capabilities(bad).code_ ==
              status_code::invalid_argument);
        bad = capabilities;
        bad.perf_profile_mask_ = 0xF0;
        check(vqec_vision_ai_core_inexe_validate_capabilities(bad).code_ ==
              status_code::invalid_argument);
        bad = capabilities;
        bad.supported_dtype_mask_ = 0;
        check(vqec_vision_ai_core_inexe_validate_capabilities(bad).code_ ==
              status_code::invalid_argument);
        bad = capabilities;
        bad.supported_dtype_mask_ = 1U;  // unknown dtype bit
        check(vqec_vision_ai_core_inexe_validate_capabilities(bad).code_ ==
              status_code::invalid_argument);
        bad = capabilities;
        bad.max_inflight_jobs_ = 0;
        check(vqec_vision_ai_core_inexe_validate_capabilities(bad).code_ ==
              status_code::invalid_argument);
        bad = capabilities;
        bad.supports_shared_memory_ = false;  // registrations remain 8
        check(vqec_vision_ai_core_inexe_validate_capabilities(bad).code_ ==
              status_code::invalid_argument);
        bad = capabilities;
        bad.supports_async_ = false;  // inflight remains 4
        check(vqec_vision_ai_core_inexe_validate_capabilities(bad).code_ ==
              status_code::invalid_argument);
    }

    // Policy structural validation.
    {
        inference_execution_policy policy;
        policy.mode_ = inference_execution_mode::asynchronous;
        policy.memory_ = inference_memory_mode::registered_shared;
        policy.profile_ = inference_perf_profile::low_latency;
        policy.compute_unit_count_ = 4;
        policy.compute_unit_affinity_ = 0x05;
        policy.max_inflight_jobs_ = 3;
        policy.priority_ = 2;
        policy.prefer_native_output_ = true;
        check(vqec_vision_ai_core_inexe_validate_policy(policy).code_ == status_code::ok);
        check(vqec_vision_ai_core_inexe_policy_is_supported(
                  policy, capabilities).code_ == status_code::ok);

        auto bad = policy;
        bad.mode_ = inference_execution_mode::synchronous;  // inflight still 3
        check(vqec_vision_ai_core_inexe_validate_policy(bad).code_ ==
              status_code::invalid_argument);
        bad = policy;
        bad.mode_ = static_cast<inference_execution_mode>(999);
        check(vqec_vision_ai_core_inexe_validate_policy(bad).code_ ==
              status_code::invalid_argument);
        bad = policy;
        bad.compute_unit_affinity_ = 0x10;  // outside 4 units
        check(vqec_vision_ai_core_inexe_validate_policy(bad).code_ ==
              status_code::invalid_argument);
        bad = policy;
        bad.priority_ = 99;
        check(vqec_vision_ai_core_inexe_validate_policy(bad).code_ ==
              status_code::invalid_argument);

        // Fail-closed policy/capability mismatches.
        bad = policy;
        bad.prefer_native_output_ = false;
        bad.mode_ = inference_execution_mode::synchronous;
        bad.max_inflight_jobs_ = 1;
        check(vqec_vision_ai_core_inexe_policy_is_supported(
                  bad, capabilities).code_ == status_code::ok);

        auto async_policy = bad;
        async_policy.mode_ = inference_execution_mode::asynchronous;
        async_policy.max_inflight_jobs_ = 2;
        auto no_async = capabilities;
        no_async.supports_async_ = false;
        no_async.max_inflight_jobs_ = 1;
        check(vqec_vision_ai_core_inexe_policy_is_supported(
                  async_policy, no_async).code_ == status_code::unsupported);

        auto shared_policy = bad;
        shared_policy.memory_ = inference_memory_mode::registered_shared;
        auto no_shared = capabilities;
        no_shared.supports_shared_memory_ = false;
        no_shared.max_shared_registrations_ = 0;
        check(vqec_vision_ai_core_inexe_policy_is_supported(
                  shared_policy, no_shared).code_ == status_code::unsupported);

        auto native_policy = bad;
        native_policy.prefer_native_output_ = true;
        auto no_native = capabilities;
        no_native.supports_native_output_ = false;
        check(vqec_vision_ai_core_inexe_policy_is_supported(
                  native_policy, no_native).code_ == status_code::unsupported);

        auto high_throughput = bad;
        high_throughput.profile_ = inference_perf_profile::high_throughput;
        check(vqec_vision_ai_core_inexe_policy_is_supported(
                  high_throughput, capabilities).code_ == status_code::unsupported);

        // Affinity valid for the requested count but selecting a unit the backend does
        // not advertise must fail closed.
        auto affinity_policy = bad;
        affinity_policy.compute_unit_count_ = 4;
        affinity_policy.compute_unit_affinity_ = 0x08;  // unit 3
        auto two_units = capabilities;
        two_units.compute_unit_count_ = 2;
        check(vqec_vision_ai_core_inexe_policy_is_supported(
                  affinity_policy, two_units).code_ == status_code::unsupported);

        auto no_topology = capabilities;
        no_topology.compute_unit_count_ = 0;
        auto wants_units = bad;
        wants_units.compute_unit_count_ = 1;
        wants_units.compute_unit_affinity_ = 0x01;
        check(vqec_vision_ai_core_inexe_policy_is_supported(
                  wants_units, no_topology).code_ == status_code::unsupported);

        auto capped = capabilities;
        capped.max_inflight_jobs_ = 1;
        auto too_many = async_policy;
        too_many.max_inflight_jobs_ = 2;
        check(vqec_vision_ai_core_inexe_policy_is_supported(
                  too_many, capped).code_ == status_code::unsupported);
    }

    // Execution domain identity, capacity and aggregate admission.
    {
        inference_execution_domain domain;
        domain.domain_id_ = "qcs6490_htp0";
        domain.max_graphs_ = 4;
        domain.max_inflight_jobs_ = 8;
        domain.allows_shared_context_ = true;
        check(vqec_vision_ai_core_inexe_validate_domain(domain).code_ == status_code::ok);
        check(vqec_vision_ai_core_inexe_domain_admits(
                  domain, capabilities, 2, 6).code_ == status_code::ok);

        auto bad = domain;
        bad.domain_id_.clear();
        check(vqec_vision_ai_core_inexe_validate_domain(bad).code_ ==
              status_code::invalid_argument);
        bad = domain;
        bad.max_graphs_ = 0;
        check(vqec_vision_ai_core_inexe_validate_domain(bad).code_ ==
              status_code::invalid_argument);

        check(vqec_vision_ai_core_inexe_domain_admits(
                  domain, capabilities, 0, 1).code_ == status_code::invalid_argument);
        check(vqec_vision_ai_core_inexe_domain_admits(
                  domain, capabilities, 5, 6).code_ == status_code::resource_exhausted);
        check(vqec_vision_ai_core_inexe_domain_admits(
                  domain, capabilities, 1, 9).code_ == status_code::resource_exhausted);

        auto no_share = domain;
        no_share.allows_shared_context_ = false;
        check(vqec_vision_ai_core_inexe_domain_admits(
                  no_share, capabilities, 2, 2).code_ == status_code::unsupported);

        auto no_multi = capabilities;
        no_multi.supports_multi_model_domain_ = false;
        check(vqec_vision_ai_core_inexe_domain_admits(
                  domain, no_multi, 2, 2).code_ == status_code::unsupported);

        auto capped = capabilities;
        capped.max_inflight_jobs_ = 1;
        check(vqec_vision_ai_core_inexe_domain_admits(
                  domain, capped, 2, 3).code_ == status_code::resource_exhausted);
    }

    // Shared/registered buffer identity and admission.
    {
        inference_shared_buffer buffer;
        buffer.registration_id_ = 1;
        buffer.allocation_id_ = 42;
        buffer.generation_ = 3;
        buffer.bytes_ = 640U * 640U * 3U;
        buffer.access_ = shared_buffer_access::read_only;
        check(vqec_vision_ai_core_inexe_validate_shared_buffer(buffer).code_ ==
              status_code::ok);
        check(vqec_vision_ai_core_inexe_shared_buffer_supported(
                  buffer, capabilities, 0).code_ == status_code::ok);

        auto bad = buffer;
        bad.registration_id_ = 0;
        check(vqec_vision_ai_core_inexe_validate_shared_buffer(bad).code_ ==
              status_code::invalid_argument);
        bad = buffer;
        bad.bytes_ = 0;
        check(vqec_vision_ai_core_inexe_validate_shared_buffer(bad).code_ ==
              status_code::invalid_argument);
        bad = buffer;
        bad.bytes_ = inference_execution_limits::g_max_shared_buffer_bytes + 1U;
        check(vqec_vision_ai_core_inexe_validate_shared_buffer(bad).code_ ==
              status_code::invalid_argument);
        bad = buffer;
        bad.access_ = static_cast<shared_buffer_access>(99);
        check(vqec_vision_ai_core_inexe_validate_shared_buffer(bad).code_ ==
              status_code::invalid_argument);

        auto no_shared = capabilities;
        no_shared.supports_shared_memory_ = false;
        no_shared.max_shared_registrations_ = 0;
        check(vqec_vision_ai_core_inexe_shared_buffer_supported(
                  buffer, no_shared, 0).code_ == status_code::unsupported);
        check(vqec_vision_ai_core_inexe_shared_buffer_supported(
                  buffer, capabilities,
                  capabilities.max_shared_registrations_).code_ ==
              status_code::resource_exhausted);
    }

    // Adapter/LoRA model-update descriptor.
    {
        inference_model_update update;
        update.base_model_id_ = "edgeface_xxs";
        update.base_model_version_ = "1.0";
        update.update_artifact_ref_ = "edgeface_xxs_lora_v2";
        update.update_artifact_sha256_ = std::string(64, 'a');
        update.update_revision_ = 2;
        check(vqec_vision_ai_core_inexe_validate_model_update(update).code_ ==
              status_code::ok);
        check(vqec_vision_ai_core_inexe_model_update_supported(
                  update, capabilities).code_ == status_code::ok);

        auto bad = update;
        bad.base_model_id_.clear();
        check(vqec_vision_ai_core_inexe_validate_model_update(bad).code_ ==
              status_code::invalid_argument);
        bad = update;
        bad.update_artifact_sha256_ = std::string(64, 'z');
        check(vqec_vision_ai_core_inexe_validate_model_update(bad).code_ ==
              status_code::invalid_argument);
        bad = update;
        bad.update_revision_ = 0;
        check(vqec_vision_ai_core_inexe_validate_model_update(bad).code_ ==
              status_code::invalid_argument);

        auto no_update = capabilities;
        no_update.supports_artifact_update_ = false;
        check(vqec_vision_ai_core_inexe_model_update_supported(
                  update, no_update).code_ == status_code::unsupported);
    }

    // S01: the owned QNN adapter advertises only implemented operations. This mirrors the
    // engine profile (single graph, synchronous client buffers, native output, no async /
    // shared memory / update / domain) so every unimplemented request is rejected before
    // load or submit even when the SDK exposes the corresponding symbol.
    {
        inference_capabilities engine;
        engine.supported_dtype_mask_ = full_dtype_mask();
        engine.perf_profile_mask_ = static_cast<std::uint8_t>(
            1U << static_cast<unsigned>(inference_perf_profile::balanced));
        engine.graph_count_ = 1;
        engine.max_inflight_jobs_ = 1;
        engine.supports_native_output_ = true;
        check(vqec_vision_ai_core_inexe_validate_capabilities(engine).code_ ==
              status_code::ok);

        inference_execution_policy policy;  // synchronous, copy, balanced, native off
        check(vqec_vision_ai_core_inexe_policy_is_supported(policy, engine).code_ ==
              status_code::ok);

        auto native_policy = policy;
        native_policy.prefer_native_output_ = true;
        check(vqec_vision_ai_core_inexe_policy_is_supported(
                  native_policy, engine).code_ == status_code::ok);

        auto async_policy = policy;
        async_policy.mode_ = inference_execution_mode::asynchronous;
        async_policy.max_inflight_jobs_ = 2;
        check(vqec_vision_ai_core_inexe_policy_is_supported(
                  async_policy, engine).code_ == status_code::unsupported);

        auto shared_policy = policy;
        shared_policy.memory_ = inference_memory_mode::registered_shared;
        check(vqec_vision_ai_core_inexe_policy_is_supported(
                  shared_policy, engine).code_ == status_code::unsupported);

        auto throughput_policy = policy;
        throughput_policy.profile_ = inference_perf_profile::high_throughput;
        check(vqec_vision_ai_core_inexe_policy_is_supported(
                  throughput_policy, engine).code_ == status_code::unsupported);

        auto affinity_policy = policy;
        affinity_policy.compute_unit_count_ = 1;
        affinity_policy.compute_unit_affinity_ = 0x01;
        check(vqec_vision_ai_core_inexe_policy_is_supported(
                  affinity_policy, engine).code_ == status_code::unsupported);

        inference_model_update update;
        update.base_model_id_ = "edgeface_xxs";
        update.base_model_version_ = "1.0";
        update.update_artifact_ref_ = "edgeface_xxs_lora_v2";
        update.update_artifact_sha256_ = std::string(64, 'a');
        update.update_revision_ = 2;
        check(vqec_vision_ai_core_inexe_model_update_supported(
                  update, engine).code_ == status_code::unsupported);

        inference_execution_domain domain;
        domain.domain_id_ = "qnn_single";
        domain.max_graphs_ = 2;
        domain.max_inflight_jobs_ = 2;
        domain.allows_shared_context_ = true;
        check(vqec_vision_ai_core_inexe_domain_admits(
                  domain, engine, 2, 2).code_ == status_code::unsupported);
    }

    std::cout << "inference execution failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
