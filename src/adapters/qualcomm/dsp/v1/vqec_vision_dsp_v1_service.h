#ifndef VQEC_VISION_AI_DSP_V1_SERVICE_H
#define VQEC_VISION_AI_DSP_V1_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "vqec_vision_dsp_v1_dense.h"
#include "vqec_vision_dsp_v1_overlay.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VQEC_VISION_AI_DSP_V1_SERVICE_MAX_INPUT_BYTES \
    VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_SURFACE_BYTES
#define VQEC_VISION_AI_DSP_V1_SERVICE_MAX_OUTPUT_BYTES \
    VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_SURFACE_BYTES
#define VQEC_VISION_AI_DSP_V1_SERVICE_MAX_DESCRIPTOR_BYTES \
    VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_DESCRIPTOR_BYTES

typedef struct vqec_vision_ai_dsp_v1_service {
    vqec_vision_ai_dsp_v1_capabilities capabilities;
    vqec_vision_ai_dsp_v1_dense_scratch dense_scratch;
} vqec_vision_ai_dsp_v1_service;

vqec_vision_ai_dsp_v1_wire_status
vqec_vision_ai_qcom_d1svc_initialize(vqec_vision_ai_dsp_v1_service* _service,
                                     uint32_t _domain_generation);

vqec_vision_ai_dsp_v1_wire_status
vqec_vision_ai_qcom_d1svc_query_capabilities(const vqec_vision_ai_dsp_v1_service* _service,
                                             uint8_t* _response, size_t _response_bytes);

/* A wire-ok return means the operation response was encoded; inspect response.status. */
vqec_vision_ai_dsp_v1_wire_status
vqec_vision_ai_qcom_d1svc_execute(vqec_vision_ai_dsp_v1_service* _service,
                                  const uint8_t* _descriptor, size_t _descriptor_bytes,
                                  const uint8_t* _input, size_t _input_bytes, uint8_t* _output,
                                  size_t _output_bytes, uint8_t* _response, size_t _response_bytes);

#ifdef __cplusplus
}
#endif

#endif
