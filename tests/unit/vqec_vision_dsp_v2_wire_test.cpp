#include <array>
#include <cstdint>
#include <iostream>

#include "vqec_vision_dsp_v2_wire.h"

namespace {

using wire = std::array<std::uint8_t, VQEC_VISION_AI_DSP_V2_ENVELOPE_BYTES>;

constexpr std::uint32_t g_fixture_input_limit_bytes = 8192;
constexpr std::uint32_t g_fixture_output_limit_bytes = 4096;
constexpr std::uint32_t g_fixture_domain_generation = 7;
constexpr std::uint32_t g_request_input_limit_bytes = 1024;
constexpr std::uint32_t g_request_output_limit_bytes = 512;
constexpr std::uint32_t g_request_input_bytes = 64;
constexpr std::uint32_t g_request_output_bytes = 32;
constexpr std::uint32_t g_request_domain_generation = 9;

int vqec_vision_ai_unit_dvwrt_test_capability_round_trip() {
    const vqec_vision_ai_dsp_v2_capabilities input{
        VQEC_VISION_AI_DSP_V2_ALL_OPERATIONS, VQEC_VISION_AI_DSP_V2_ENVELOPE_BYTES,
        g_fixture_input_limit_bytes, g_fixture_output_limit_bytes, g_fixture_domain_generation};
    wire bytes{};
    if (vqec_vision_ai_qcom_dvwir_encode_capabilities(&input, bytes.data(), bytes.size()) !=
        vqec_vision_ai_dsp_v2_wire_ok) {
        return 1;
    }
    vqec_vision_ai_dsp_v2_capabilities decoded{};
    if (vqec_vision_ai_qcom_dvwir_decode_capabilities(bytes.data(), bytes.size(), &decoded) !=
            vqec_vision_ai_dsp_v2_wire_ok ||
        decoded.operations_mask != input.operations_mask ||
        decoded.max_input_bytes != input.max_input_bytes ||
        decoded.domain_generation != input.domain_generation) {
        return 1;
    }
    const auto original = bytes;
    bytes[0] = 0;
    if (vqec_vision_ai_qcom_dvwir_decode_capabilities(bytes.data(), bytes.size(), &decoded) !=
        vqec_vision_ai_dsp_v2_wire_malformed) {
        return 1;
    }
    bytes = original;
    bytes[VQEC_VISION_AI_DSP_V2_MAJOR_OFFSET] = VQEC_VISION_AI_DSP_V2_MAJOR + 1;
    if (vqec_vision_ai_qcom_dvwir_decode_capabilities(bytes.data(), bytes.size(), &decoded) !=
        vqec_vision_ai_dsp_v2_wire_incompatible) {
        return 1;
    }
    bytes = original;
    bytes[VQEC_VISION_AI_DSP_V2_MINOR_OFFSET] = VQEC_VISION_AI_DSP_V2_MINOR + 1;
    if (vqec_vision_ai_qcom_dvwir_decode_capabilities(bytes.data(), bytes.size(), &decoded) !=
        vqec_vision_ai_dsp_v2_wire_incompatible) {
        return 1;
    }
    bytes = original;
    bytes[VQEC_VISION_AI_DSP_V2_HEADER_BYTES_OFFSET] = 0;
    if (vqec_vision_ai_qcom_dvwir_decode_capabilities(bytes.data(), bytes.size(), &decoded) !=
        vqec_vision_ai_dsp_v2_wire_malformed) {
        return 1;
    }
    bytes = original;
    bytes[VQEC_VISION_AI_DSP_V2_KIND_OFFSET] = 0;
    if (vqec_vision_ai_qcom_dvwir_decode_capabilities(bytes.data(), bytes.size(), &decoded) !=
        vqec_vision_ai_dsp_v2_wire_unsupported) {
        return 1;
    }
    bytes = original;
    bytes[VQEC_VISION_AI_DSP_V2_FIELD_12_OFFSET] = 0;
    if (vqec_vision_ai_qcom_dvwir_decode_capabilities(bytes.data(), bytes.size(), &decoded) !=
        vqec_vision_ai_dsp_v2_wire_malformed) {
        return 1;
    }
    return vqec_vision_ai_qcom_dvwir_decode_capabilities(original.data(), original.size() - 1,
                                                         &decoded) ==
                   vqec_vision_ai_dsp_v2_wire_malformed
               ? 0
               : 1;
}

int vqec_vision_ai_unit_dvwrt_test_request_bounds() {
    const vqec_vision_ai_dsp_v2_capabilities caps{
        1U << (VQEC_VISION_AI_DSP_V2_DENSE_DECODE - 1U), VQEC_VISION_AI_DSP_V2_ENVELOPE_BYTES,
        g_request_input_limit_bytes, g_request_output_limit_bytes, g_request_domain_generation};
    const vqec_vision_ai_dsp_v2_request request{
        VQEC_VISION_AI_DSP_V2_DENSE_DECODE, VQEC_VISION_AI_DSP_V2_ENVELOPE_BYTES,
        g_request_input_bytes, g_request_output_bytes, caps.domain_generation};
    wire bytes{};
    if (vqec_vision_ai_qcom_dvwir_encode_request(&request, bytes.data(), bytes.size()) !=
        vqec_vision_ai_dsp_v2_wire_ok) {
        return 1;
    }
    vqec_vision_ai_dsp_v2_request decoded{};
    if (vqec_vision_ai_qcom_dvwir_validate_request(
            &caps, bytes.data(), bytes.size(), request.input_bytes, request.output_capacity_bytes,
            &decoded) != vqec_vision_ai_dsp_v2_wire_ok ||
        decoded.operation != request.operation || decoded.input_bytes != request.input_bytes) {
        return 1;
    }
    if (vqec_vision_ai_qcom_dvwir_validate_request(
            &caps, bytes.data(), bytes.size() - 1, request.input_bytes,
            request.output_capacity_bytes, &decoded) != vqec_vision_ai_dsp_v2_wire_malformed ||
        vqec_vision_ai_qcom_dvwir_validate_request(
            &caps, bytes.data(), bytes.size(), request.input_bytes + 1,
            request.output_capacity_bytes, &decoded) != vqec_vision_ai_dsp_v2_wire_out_of_range) {
        return 1;
    }
    const auto original = bytes;
    bytes[VQEC_VISION_AI_DSP_V2_FIELD_24_OFFSET] =
        static_cast<std::uint8_t>(g_request_domain_generation - 1);
    if (vqec_vision_ai_qcom_dvwir_validate_request(
            &caps, bytes.data(), bytes.size(), request.input_bytes, request.output_capacity_bytes,
            &decoded) != vqec_vision_ai_dsp_v2_wire_stale_generation) {
        return 1;
    }
    bytes = original;
    bytes[VQEC_VISION_AI_DSP_V2_KIND_OFFSET] = VQEC_VISION_AI_DSP_V2_IMAGE_TRANSFORM;
    if (vqec_vision_ai_qcom_dvwir_validate_request(
            &caps, bytes.data(), bytes.size(), request.input_bytes, request.output_capacity_bytes,
            &decoded) != vqec_vision_ai_dsp_v2_wire_unsupported) {
        return 1;
    }
    bytes = original;
    bytes[VQEC_VISION_AI_DSP_V2_FIELD_28_OFFSET] = 1;
    if (vqec_vision_ai_qcom_dvwir_validate_request(
            &caps, bytes.data(), bytes.size(), request.input_bytes, request.output_capacity_bytes,
            &decoded) != vqec_vision_ai_dsp_v2_wire_malformed) {
        return 1;
    }
    bytes = original;
    bytes[VQEC_VISION_AI_DSP_V2_FIELD_16_OFFSET] = 0;
    if (vqec_vision_ai_qcom_dvwir_validate_request(
            &caps, bytes.data(), bytes.size(), request.input_bytes, request.output_capacity_bytes,
            &decoded) != vqec_vision_ai_dsp_v2_wire_out_of_range) {
        return 1;
    }
    bytes = original;
    vqec_vision_ai_dsp_v2_capabilities smaller_caps = caps;
    smaller_caps.max_input_bytes = request.input_bytes - 1;
    if (vqec_vision_ai_qcom_dvwir_validate_request(
            &smaller_caps, bytes.data(), bytes.size(), request.input_bytes,
            request.output_capacity_bytes, &decoded) != vqec_vision_ai_dsp_v2_wire_out_of_range) {
        return 1;
    }
    smaller_caps = caps;
    smaller_caps.max_output_bytes = request.output_capacity_bytes - 1;
    if (vqec_vision_ai_qcom_dvwir_validate_request(
            &smaller_caps, bytes.data(), bytes.size(), request.input_bytes,
            request.output_capacity_bytes, &decoded) != vqec_vision_ai_dsp_v2_wire_out_of_range) {
        return 1;
    }
    return 0;
}

} // namespace

int main() {
    if (vqec_vision_ai_unit_dvwrt_test_capability_round_trip() != 0 ||
        vqec_vision_ai_unit_dvwrt_test_request_bounds() != 0) {
        std::cerr << "FastRPC v2 wire envelope regression failed\n";
        return 1;
    }
    return 0;
}
