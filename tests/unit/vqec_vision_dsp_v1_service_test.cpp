// Device-free test for capability negotiation and operation/result separation.

#include <array>
#include <cstdint>
#include <iostream>

#include "vqec_vision_dsp_v1_service.h"

int main() {
    constexpr std::uint32_t domain_generation = 11U;
    vqec_vision_ai_dsp_v1_service service{};
    if (vqec_vision_ai_qcom_d1svc_initialize(&service, domain_generation) !=
        vqec_vision_ai_dsp_v1_wire_ok) {
        return 1;
    }

    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_ENVELOPE_BYTES> capability_wire{};
    vqec_vision_ai_dsp_v1_capabilities capabilities{};
    if (vqec_vision_ai_qcom_d1svc_query_capabilities(&service, capability_wire.data(),
                                                     capability_wire.size()) !=
            vqec_vision_ai_dsp_v1_wire_ok ||
        vqec_vision_ai_qcom_dvwir_decode_capabilities(capability_wire.data(),
                                                      capability_wire.size(), &capabilities) !=
            vqec_vision_ai_dsp_v1_wire_ok ||
        capabilities.domain_generation != domain_generation ||
        capabilities.max_descriptor_bytes != VQEC_VISION_AI_DSP_V1_DENSE_DESCRIPTOR_BYTES) {
        return 1;
    }

    vqec_vision_ai_dsp_v1_dense_config config{};
    config.flags = VQEC_VISION_AI_DSP_V1_DENSE_CLASS_AWARE_NMS;
    config.prediction_count = 1U;
    config.class_count = 1U;
    config.box_offset = 0U;
    config.score_offset = 4U * sizeof(std::uint16_t);
    config.source_width = 64U;
    config.source_height = 64U;
    config.candidate_capacity = 1U;
    config.output_capacity = 1U;
    config.box_scale = 1.0F;
    config.score_scale = 0.001F;
    config.confidence_threshold = 0.5F;
    config.iou_threshold = 0.5F;
    config.scale_x = 1.0F;
    config.scale_y = 1.0F;

    const std::array<std::uint8_t, 10U> input{16U, 0U, 16U, 0U, 8U, 0U, 8U, 0U, 0x84U, 0x03U};
    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_DENSE_DESCRIPTOR_BYTES> descriptor{};
    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_DENSE_RECORD_BYTES> output{};
    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_RESPONSE_BYTES> response_wire{};
    if (vqec_vision_ai_qcom_d1dns_encode_descriptor(
            &config, domain_generation, sizeof(input), output.size(), descriptor.data(),
            descriptor.size()) != vqec_vision_ai_dsp_v1_wire_ok ||
        vqec_vision_ai_qcom_d1svc_execute(&service, descriptor.data(), descriptor.size(),
                                          input.data(), input.size(), output.data(), output.size(),
                                          response_wire.data(),
                                          response_wire.size()) != vqec_vision_ai_dsp_v1_wire_ok) {
        return 1;
    }
    vqec_vision_ai_dsp_v1_operation_response response{};
    if (vqec_vision_ai_qcom_dvwir_decode_operation_response(response_wire.data(),
                                                            response_wire.size(), &response) !=
            vqec_vision_ai_dsp_v1_wire_ok ||
        response.operation != VQEC_VISION_AI_DSP_V1_DENSE_DECODE ||
        response.status != vqec_vision_ai_dsp_v1_wire_ok ||
        response.output_bytes != VQEC_VISION_AI_DSP_V1_DENSE_RECORD_BYTES) {
        return 1;
    }

    // A stale request still produces a trusted failure response with no valid output.
    if (vqec_vision_ai_qcom_d1dns_encode_descriptor(
            &config, domain_generation - 1U, sizeof(input), output.size(), descriptor.data(),
            descriptor.size()) != vqec_vision_ai_dsp_v1_wire_ok ||
        vqec_vision_ai_qcom_d1svc_execute(&service, descriptor.data(), descriptor.size(),
                                          input.data(), input.size(), output.data(), output.size(),
                                          response_wire.data(),
                                          response_wire.size()) != vqec_vision_ai_dsp_v1_wire_ok ||
        vqec_vision_ai_qcom_dvwir_decode_operation_response(response_wire.data(),
                                                            response_wire.size(), &response) !=
            vqec_vision_ai_dsp_v1_wire_ok ||
        response.operation != 0U ||
        response.status != vqec_vision_ai_dsp_v1_wire_stale_generation ||
        response.output_bytes != 0U) {
        return 1;
    }

    std::cout << "dsp v1 service dispatch passed\n";
    return 0;
}
