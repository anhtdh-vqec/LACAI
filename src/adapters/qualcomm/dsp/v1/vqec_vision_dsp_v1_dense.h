#ifndef VQEC_VISION_AI_DSP_V1_DENSE_H
#define VQEC_VISION_AI_DSP_V1_DENSE_H

#include <stddef.h>
#include <stdint.h>

#include "vqec_vision_dsp_v1_wire.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VQEC_VISION_AI_DSP_V1_DENSE_DESCRIPTOR_BYTES 120U
#define VQEC_VISION_AI_DSP_V1_DENSE_PAYLOAD_VERSION_OFFSET VQEC_VISION_AI_DSP_V1_ENVELOPE_BYTES
#define VQEC_VISION_AI_DSP_V1_DENSE_RECORD_BYTES 24U
#define VQEC_VISION_AI_DSP_V1_DENSE_MAX_PREDICTIONS 65536U
#define VQEC_VISION_AI_DSP_V1_DENSE_MAX_CLASSES 256U
#define VQEC_VISION_AI_DSP_V1_DENSE_MAX_CANDIDATES 512U
#define VQEC_VISION_AI_DSP_V1_DENSE_MAX_OUTPUTS 256U

#define VQEC_VISION_AI_DSP_V1_DENSE_DTYPE_UINT16 1U
#define VQEC_VISION_AI_DSP_V1_DENSE_BOX_XYWH 1U
#define VQEC_VISION_AI_DSP_V1_DENSE_SCORE_PROBABILITY 1U
#define VQEC_VISION_AI_DSP_V1_DENSE_LAYOUT_CHANNEL_MAJOR 1U
#define VQEC_VISION_AI_DSP_V1_DENSE_CLASS_AWARE_NMS 1U

typedef struct vqec_vision_ai_dsp_v1_dense_config {
    uint32_t flags;
    uint32_t prediction_count;
    uint32_t class_count;
    uint32_t box_offset;
    uint32_t score_offset;
    uint32_t source_width;
    uint32_t source_height;
    uint32_t candidate_capacity;
    uint32_t output_capacity;
    float box_scale;
    int32_t box_zero_point;
    float score_scale;
    int32_t score_zero_point;
    float confidence_threshold;
    float iou_threshold;
    float scale_x;
    float scale_y;
    float pad_x;
    float pad_y;
} vqec_vision_ai_dsp_v1_dense_config;

typedef struct vqec_vision_ai_dsp_v1_dense_candidate {
    float x1;
    float y1;
    float x2;
    float y2;
    float score;
    uint32_t class_index;
    uint32_t prediction_index;
} vqec_vision_ai_dsp_v1_dense_candidate;

typedef struct vqec_vision_ai_dsp_v1_dense_scratch {
    vqec_vision_ai_dsp_v1_dense_candidate candidates[VQEC_VISION_AI_DSP_V1_DENSE_MAX_CANDIDATES];
    uint16_t order[VQEC_VISION_AI_DSP_V1_DENSE_MAX_CANDIDATES];
    uint16_t kept[VQEC_VISION_AI_DSP_V1_DENSE_MAX_OUTPUTS];
} vqec_vision_ai_dsp_v1_dense_scratch;

typedef struct vqec_vision_ai_dsp_v1_dense_result {
    uint32_t output_count;
    uint32_t truncated_candidates;
    uint32_t output_bytes;
} vqec_vision_ai_dsp_v1_dense_result;

/* Host-side canonical encoder for the fixed v1 payload. */
vqec_vision_ai_dsp_v1_wire_status
vqec_vision_ai_qcom_d1dns_encode_descriptor(const vqec_vision_ai_dsp_v1_dense_config* _config,
                                            uint32_t _domain_generation, size_t _input_bytes,
                                            size_t _output_capacity_bytes, uint8_t* _descriptor,
                                            size_t _descriptor_bytes);

/* Validates the full operation payload and tensor/output bounds before any tensor read. */
vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_qcom_d1dns_execute(
    const vqec_vision_ai_dsp_v1_capabilities* _capabilities, const uint8_t* _descriptor,
    size_t _descriptor_bytes, const uint8_t* _input, size_t _input_bytes, uint8_t* _output,
    size_t _output_capacity_bytes, vqec_vision_ai_dsp_v1_dense_scratch* _scratch,
    vqec_vision_ai_dsp_v1_dense_result* _result);

#ifdef __cplusplus
}
#endif

#endif
