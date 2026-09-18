#include "vqec_vision_dsp_v1_service.h"

#include <string.h>

vqec_vision_ai_dsp_v1_wire_status
vqec_vision_ai_qcom_d1svc_initialize(vqec_vision_ai_dsp_v1_service* _service,
                                     uint32_t _domain_generation) {
    if (_service == NULL || _domain_generation == 0U) {
        return vqec_vision_ai_dsp_v1_wire_malformed;
    }
    memset(_service, 0, sizeof(*_service));
    _service->capabilities.operations_mask =
        (1U << (VQEC_VISION_AI_DSP_V1_DENSE_DECODE - 1U)) |
        (1U << (VQEC_VISION_AI_DSP_V1_OVERLAY_COMPOSE - 1U));
    _service->capabilities.max_descriptor_bytes =
        VQEC_VISION_AI_DSP_V1_SERVICE_MAX_DESCRIPTOR_BYTES;
    _service->capabilities.max_input_bytes = VQEC_VISION_AI_DSP_V1_SERVICE_MAX_INPUT_BYTES;
    _service->capabilities.max_output_bytes = VQEC_VISION_AI_DSP_V1_SERVICE_MAX_OUTPUT_BYTES;
    _service->capabilities.domain_generation = _domain_generation;
    return vqec_vision_ai_dsp_v1_wire_ok;
}

vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_qcom_d1svc_configure_image_backend(
    vqec_vision_ai_dsp_v1_service* _service,
    const vqec_vision_ai_dsp_v1_image_backend* _backend) {
    if (_service == NULL || _backend == NULL || _backend->scale_luma == NULL ||
        _backend->scale_chroma == NULL || _backend->convert_color == NULL) {
        return vqec_vision_ai_dsp_v1_wire_malformed;
    }
    _service->image_backend = *_backend;
    _service->capabilities.operations_mask |=
        1U << (VQEC_VISION_AI_DSP_V1_IMAGE_TRANSFORM - 1U);
    return vqec_vision_ai_dsp_v1_wire_ok;
}

void vqec_vision_ai_qcom_d1svc_cleanup(vqec_vision_ai_dsp_v1_service* _service) {
    if (_service == NULL) {
        return;
    }
    vqec_vision_ai_qcom_d1img_release_scratch(&_service->image_scratch);
    memset(&_service->image_backend, 0, sizeof(_service->image_backend));
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
    vqec_vision_ai_dsp_v1_dense_result dense_result = {0U, 0U, 0U};
    vqec_vision_ai_dsp_v1_request request = {0U, 0U, 0U, 0U, 0U};
    uint32_t overlay_output_bytes = 0U;
    uint32_t image_output_bytes = 0U;
    vqec_vision_ai_dsp_v1_wire_status operation_status =
        vqec_vision_ai_qcom_dvwir_validate_request(
            &_service->capabilities, _descriptor, _descriptor_bytes, _input_bytes,
            _output_bytes, &request);
    const int request_valid = operation_status == vqec_vision_ai_dsp_v1_wire_ok;
    if (request_valid) {
        if (request.operation == VQEC_VISION_AI_DSP_V1_IMAGE_TRANSFORM) {
            operation_status = vqec_vision_ai_qcom_d1img_execute(
                &_service->capabilities, &_service->image_backend,
                &_service->image_scratch, _descriptor, _descriptor_bytes, _input,
                _input_bytes, _output, _output_bytes, &image_output_bytes);
        } else if (request.operation == VQEC_VISION_AI_DSP_V1_DENSE_DECODE) {
            operation_status = vqec_vision_ai_qcom_d1dns_execute(
                &_service->capabilities, _descriptor, _descriptor_bytes, _input, _input_bytes,
                _output, _output_bytes, &_service->dense_scratch, &dense_result);
        } else if (request.operation == VQEC_VISION_AI_DSP_V1_OVERLAY_COMPOSE) {
            operation_status = vqec_vision_ai_qcom_d1ovr_execute(
                &_service->capabilities, _descriptor, _descriptor_bytes, _input, _input_bytes,
                _output, _output_bytes, &overlay_output_bytes);
        } else {
            operation_status = vqec_vision_ai_dsp_v1_wire_unsupported;
        }
    }
    const uint16_t response_operation = request_valid ? request.operation : 0U;
    const uint32_t response_output_bytes =
        request.operation == VQEC_VISION_AI_DSP_V1_IMAGE_TRANSFORM ? image_output_bytes :
        request.operation == VQEC_VISION_AI_DSP_V1_DENSE_DECODE ?
            dense_result.output_bytes : overlay_output_bytes;
    const uint32_t response_detail = request.operation == VQEC_VISION_AI_DSP_V1_DENSE_DECODE ?
        dense_result.truncated_candidates : 0U;
    const vqec_vision_ai_dsp_v1_operation_response operation_response = {
        response_operation, operation_status,
        operation_status == vqec_vision_ai_dsp_v1_wire_ok ? response_output_bytes : 0U,
        operation_status == vqec_vision_ai_dsp_v1_wire_ok ? response_detail : 0U};
    return vqec_vision_ai_qcom_dvwir_encode_operation_response(&operation_response, _response,
                                                               _response_bytes);
}
