#ifndef VQEC_VISION_AI_DSP_V2_WIRE_H
#define VQEC_VISION_AI_DSP_V2_WIRE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Draft ABI 2.0. All multibyte fields are little-endian; never cast wire bytes. */
enum {
    VQEC_VISION_AI_DSP_V2_ENVELOPE_BYTES = 32,
    VQEC_VISION_AI_DSP_V2_MAJOR = 2,
    VQEC_VISION_AI_DSP_V2_MINOR = 0,
    VQEC_VISION_AI_DSP_V2_SYNC_COMPLETION = 1,
    VQEC_VISION_AI_DSP_V2_IMAGE_TRANSFORM = 1,
    VQEC_VISION_AI_DSP_V2_DENSE_DECODE = 2,
    VQEC_VISION_AI_DSP_V2_ANCHOR_DISTANCE_DECODE = 3,
    VQEC_VISION_AI_DSP_V2_ROI_ALIGN = 4,
    VQEC_VISION_AI_DSP_V2_ALL_OPERATIONS = 15
};

/* One owner for the fixed v2 envelope layout. Some offsets have reply/request roles. */
enum {
    VQEC_VISION_AI_DSP_V2_MAGIC_OFFSET = 0,
    VQEC_VISION_AI_DSP_V2_MAJOR_OFFSET = 4,
    VQEC_VISION_AI_DSP_V2_MINOR_OFFSET = 6,
    VQEC_VISION_AI_DSP_V2_HEADER_BYTES_OFFSET = 8,
    VQEC_VISION_AI_DSP_V2_KIND_OFFSET = 10,
    VQEC_VISION_AI_DSP_V2_FIELD_12_OFFSET = 12,
    VQEC_VISION_AI_DSP_V2_FIELD_16_OFFSET = 16,
    VQEC_VISION_AI_DSP_V2_FIELD_20_OFFSET = 20,
    VQEC_VISION_AI_DSP_V2_FIELD_24_OFFSET = 24,
    VQEC_VISION_AI_DSP_V2_FIELD_28_OFFSET = 28
};

typedef enum vqec_vision_ai_dsp_v2_wire_status {
    vqec_vision_ai_dsp_v2_wire_ok = 0,
    vqec_vision_ai_dsp_v2_wire_malformed,
    vqec_vision_ai_dsp_v2_wire_incompatible,
    vqec_vision_ai_dsp_v2_wire_unsupported,
    vqec_vision_ai_dsp_v2_wire_out_of_range,
    vqec_vision_ai_dsp_v2_wire_stale_generation
} vqec_vision_ai_dsp_v2_wire_status;

typedef struct vqec_vision_ai_dsp_v2_capabilities {
    uint32_t operations_mask;
    uint32_t max_descriptor_bytes;
    uint32_t max_input_bytes;
    uint32_t max_output_bytes;
    uint32_t domain_generation;
} vqec_vision_ai_dsp_v2_capabilities;

typedef struct vqec_vision_ai_dsp_v2_request {
    uint16_t operation;
    uint32_t descriptor_bytes;
    uint32_t input_bytes;
    uint32_t output_capacity_bytes;
    uint32_t domain_generation;
} vqec_vision_ai_dsp_v2_request;

vqec_vision_ai_dsp_v2_wire_status vqec_vision_ai_qcom_dvwir_encode_capabilities(
    const vqec_vision_ai_dsp_v2_capabilities* _capabilities, uint8_t* _wire, size_t _wire_bytes);

vqec_vision_ai_dsp_v2_wire_status
vqec_vision_ai_qcom_dvwir_decode_capabilities(const uint8_t* _wire, size_t _wire_bytes,
                                              vqec_vision_ai_dsp_v2_capabilities* _capabilities);

vqec_vision_ai_dsp_v2_wire_status
vqec_vision_ai_qcom_dvwir_encode_request(const vqec_vision_ai_dsp_v2_request* _request,
                                         uint8_t* _descriptor, size_t _descriptor_bytes);

/* Envelope only. Operation payload must be separately validated before execution. */
vqec_vision_ai_dsp_v2_wire_status
vqec_vision_ai_qcom_dvwir_validate_request(const vqec_vision_ai_dsp_v2_capabilities* _capabilities,
                                           const uint8_t* _descriptor, size_t _descriptor_bytes,
                                           size_t _input_bytes, size_t _output_capacity_bytes,
                                           vqec_vision_ai_dsp_v2_request* _request);

#ifdef __cplusplus
}
#endif

#endif
