#ifndef VQEC_VISION_AI_DSP_V1_WIRE_H
#define VQEC_VISION_AI_DSP_V1_WIRE_H

#include <stddef.h>
#include <stdint.h>

#include "vqec/vision/ai/contracts/vqec_vision_version_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Draft ABI 1.0. Fixed wire constants are macros for C/C++ compile-time use. */
#define VQEC_VISION_AI_DSP_V1_ENVELOPE_BYTES 32U
#define VQEC_VISION_AI_DSP_V1_RESPONSE_BYTES 24U
#define VQEC_VISION_AI_DSP_V1_MAGIC_BYTES 4U
#define VQEC_VISION_AI_DSP_V1_MAGIC_0 'V'
#define VQEC_VISION_AI_DSP_V1_MAGIC_1 'Q'
#define VQEC_VISION_AI_DSP_V1_MAGIC_2 '1'
#define VQEC_VISION_AI_DSP_V1_MAGIC_3 '!'
#define VQEC_VISION_AI_DSP_V1_MAJOR VQEC_VISION_AI_BASELINE_ABI_MAJOR
#define VQEC_VISION_AI_DSP_V1_MINOR VQEC_VISION_AI_BASELINE_ABI_MINOR
#define VQEC_VISION_AI_DSP_V1_SYNC_COMPLETION 1U
#define VQEC_VISION_AI_DSP_V1_IMAGE_TRANSFORM 1U
#define VQEC_VISION_AI_DSP_V1_DENSE_DECODE 2U
#define VQEC_VISION_AI_DSP_V1_ANCHOR_DISTANCE_DECODE 3U
#define VQEC_VISION_AI_DSP_V1_ROI_ALIGN 4U
#define VQEC_VISION_AI_DSP_V1_ALL_OPERATIONS 15U

/* One owner for the fixed little-endian layout; offsets have reply/request roles. */
#define VQEC_VISION_AI_DSP_V1_MAJOR_OFFSET 4U
#define VQEC_VISION_AI_DSP_V1_MINOR_OFFSET 6U
#define VQEC_VISION_AI_DSP_V1_HEADER_BYTES_OFFSET 8U
#define VQEC_VISION_AI_DSP_V1_KIND_OFFSET 10U
#define VQEC_VISION_AI_DSP_V1_FIELD_12_OFFSET 12U
#define VQEC_VISION_AI_DSP_V1_FIELD_16_OFFSET 16U
#define VQEC_VISION_AI_DSP_V1_FIELD_20_OFFSET 20U
#define VQEC_VISION_AI_DSP_V1_FIELD_24_OFFSET 24U
#define VQEC_VISION_AI_DSP_V1_FIELD_28_OFFSET 28U

typedef enum vqec_vision_ai_dsp_v1_wire_status {
    vqec_vision_ai_dsp_v1_wire_ok = 0,
    vqec_vision_ai_dsp_v1_wire_malformed,
    vqec_vision_ai_dsp_v1_wire_incompatible,
    vqec_vision_ai_dsp_v1_wire_unsupported,
    vqec_vision_ai_dsp_v1_wire_out_of_range,
    vqec_vision_ai_dsp_v1_wire_stale_generation
} vqec_vision_ai_dsp_v1_wire_status;

typedef struct vqec_vision_ai_dsp_v1_capabilities {
    uint32_t operations_mask;
    uint32_t max_descriptor_bytes;
    uint32_t max_input_bytes;
    uint32_t max_output_bytes;
    uint32_t domain_generation;
} vqec_vision_ai_dsp_v1_capabilities;

typedef struct vqec_vision_ai_dsp_v1_request {
    uint16_t operation;
    uint32_t descriptor_bytes;
    uint32_t input_bytes;
    uint32_t output_capacity_bytes;
    uint32_t domain_generation;
} vqec_vision_ai_dsp_v1_request;

typedef struct vqec_vision_ai_dsp_v1_operation_response {
    uint16_t operation;
    vqec_vision_ai_dsp_v1_wire_status status;
    uint32_t output_bytes;
    uint32_t detail;
} vqec_vision_ai_dsp_v1_operation_response;

vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_qcom_dvwir_encode_capabilities(
    const vqec_vision_ai_dsp_v1_capabilities* _capabilities, uint8_t* _wire, size_t _wire_bytes);

vqec_vision_ai_dsp_v1_wire_status
vqec_vision_ai_qcom_dvwir_decode_capabilities(const uint8_t* _wire, size_t _wire_bytes,
                                              vqec_vision_ai_dsp_v1_capabilities* _capabilities);

vqec_vision_ai_dsp_v1_wire_status
vqec_vision_ai_qcom_dvwir_encode_request(const vqec_vision_ai_dsp_v1_request* _request,
                                         uint8_t* _descriptor, size_t _descriptor_bytes);

/* Envelope only. Operation payload must be separately validated before execution. */
vqec_vision_ai_dsp_v1_wire_status
vqec_vision_ai_qcom_dvwir_validate_request(const vqec_vision_ai_dsp_v1_capabilities* _capabilities,
                                           const uint8_t* _descriptor, size_t _descriptor_bytes,
                                           size_t _input_bytes, size_t _output_capacity_bytes,
                                           vqec_vision_ai_dsp_v1_request* _request);

vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_qcom_dvwir_encode_operation_response(
    const vqec_vision_ai_dsp_v1_operation_response* _response, uint8_t* _wire, size_t _wire_bytes);

vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_qcom_dvwir_decode_operation_response(
    const uint8_t* _wire, size_t _wire_bytes, vqec_vision_ai_dsp_v1_operation_response* _response);

#ifdef __cplusplus
}
#endif

#endif
