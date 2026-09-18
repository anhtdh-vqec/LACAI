#include "vqec_vision_dsp_v1_service.h"

#include <string.h>

vqec_vision_ai_dsp_v1_wire_status
vqec_vision_ai_qcom_d1svc_initialize(vqec_vision_ai_dsp_v1_service* _service,
                                     uint32_t _domain_generation) {
    if (_service == NULL || _domain_generation == 0U) {
        return vqec_vision_ai_dsp_v1_wire_malformed;
    }
    memset(_service, 0, sizeof(*_service));
    _service->capabilities.operations_mask = 1U << (VQEC_VISION_AI_DSP_V1_DENSE_DECODE - 1U);
    _service->capabilities.max_descriptor_bytes = VQEC_VISION_AI_DSP_V1_DENSE_DESCRIPTOR_BYTES;
    _service->capabilities.max_input_bytes = VQEC_VISION_AI_DSP_V1_SERVICE_MAX_INPUT_BYTES;
    _service->capabilities.max_output_bytes = VQEC_VISION_AI_DSP_V1_SERVICE_MAX_OUTPUT_BYTES;
    _service->capabilities.domain_generation = _domain_generation;
    return vqec_vision_ai_dsp_v1_wire_ok;
}

vqec_vision_ai_dsp_v1_wire_status
vqec_vision_ai_qcom_d1svc_query_capabilities(const vqec_vision_ai_dsp_v1_service* _service,
                                             uint8_t* _response, size_t _response_bytes) {
    if (_service == NULL) {
        return vqec_vision_ai_dsp_v1_wire_malformed;
    }
    return vqec_vision_ai_qcom_dvwir_encode_capabilities(&_service->capabilities, _response,
                                                         _response_bytes);
}

vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_qcom_d1svc_execute(
    vqec_vision_ai_dsp_v1_service* _service, const uint8_t* _descriptor, size_t _descriptor_bytes,
    const uint8_t* _input, size_t _input_bytes, uint8_t* _output, size_t _output_bytes,
    uint8_t* _response, size_t _response_bytes) {
    if (_service == NULL || _response == NULL ||
        _response_bytes != VQEC_VISION_AI_DSP_V1_RESPONSE_BYTES) {
        return vqec_vision_ai_dsp_v1_wire_malformed;
    }
    vqec_vision_ai_dsp_v1_dense_result result = {0U, 0U, 0U};
    const vqec_vision_ai_dsp_v1_wire_status operation_status = vqec_vision_ai_qcom_d1dns_execute(
        &_service->capabilities, _descriptor, _descriptor_bytes, _input, _input_bytes, _output,
        _output_bytes, &_service->dense_scratch, &result);
    const vqec_vision_ai_dsp_v1_operation_response operation_response = {
        operation_status == vqec_vision_ai_dsp_v1_wire_ok ? VQEC_VISION_AI_DSP_V1_DENSE_DECODE : 0U,
        operation_status,
        operation_status == vqec_vision_ai_dsp_v1_wire_ok ? result.output_bytes : 0U,
        operation_status == vqec_vision_ai_dsp_v1_wire_ok ? result.truncated_candidates : 0U};
    return vqec_vision_ai_qcom_dvwir_encode_operation_response(&operation_response, _response,
                                                               _response_bytes);
}
