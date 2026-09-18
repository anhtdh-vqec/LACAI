#ifndef VQEC_VISION_AI_DSP_V1_OVERLAY_H
#define VQEC_VISION_AI_DSP_V1_OVERLAY_H

#include <stddef.h>
#include <stdint.h>

#include "vqec_vision_dsp_v1_wire.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VQEC_VISION_AI_DSP_V1_OVERLAY_HEADER_BYTES 112U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_BYTES 32U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_BOXES 128U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_LABEL_BYTES 4096U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_DESCRIPTOR_BYTES                                  \
    (VQEC_VISION_AI_DSP_V1_OVERLAY_HEADER_BYTES +                                           \
     VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_BOXES * VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_BYTES +   \
     VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_LABEL_BYTES)
#define VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_SURFACE_BYTES (64U * 1024U * 1024U)

#define VQEC_VISION_AI_DSP_V1_OVERLAY_PIXEL_NV12 1U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_MATRIX_BT709 1U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_RANGE_LIMITED 1U

typedef struct vqec_vision_ai_dsp_v1_overlay_frame {
    uint32_t width;
    uint32_t height;
    uint32_t source_y_offset;
    uint32_t source_y_stride;
    uint32_t source_uv_offset;
    uint32_t source_uv_stride;
    uint32_t destination_y_offset;
    uint32_t destination_y_stride;
    uint32_t destination_uv_offset;
    uint32_t destination_uv_stride;
    uint16_t border_thickness;
    uint16_t font_scale;
} vqec_vision_ai_dsp_v1_overlay_frame;

typedef struct vqec_vision_ai_dsp_v1_overlay_box {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint8_t color_y;
    uint8_t color_u;
    uint8_t color_v;
    uint32_t label_offset;
    uint16_t label_bytes;
} vqec_vision_ai_dsp_v1_overlay_box;

vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_qcom_d1ovr_encode_descriptor(
    const vqec_vision_ai_dsp_v1_capabilities* _capabilities,
    const vqec_vision_ai_dsp_v1_overlay_frame* _frame,
    const vqec_vision_ai_dsp_v1_overlay_box* _boxes, uint32_t _box_count,
    const uint8_t* _labels, uint32_t _label_bytes, uint32_t _input_bytes,
    uint32_t _output_bytes, uint8_t* _descriptor, size_t _descriptor_capacity,
    size_t* _descriptor_bytes);

vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_qcom_d1ovr_execute(
    const vqec_vision_ai_dsp_v1_capabilities* _capabilities, const uint8_t* _descriptor,
    size_t _descriptor_bytes, const uint8_t* _input, size_t _input_bytes, uint8_t* _output,
    size_t _output_bytes, uint32_t* _written_bytes);

#ifdef __cplusplus
}
#endif

#endif
