#ifndef VQEC_VISION_AI_DSP_V1_IMAGE_H
#define VQEC_VISION_AI_DSP_V1_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#include "vqec_vision_dsp_v1_wire.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VQEC_VISION_AI_DSP_V1_IMAGE_DESCRIPTOR_BYTES 176U
#define VQEC_VISION_AI_DSP_V1_IMAGE_MAX_SIDE 4096U
#define VQEC_VISION_AI_DSP_V1_IMAGE_MAX_INPUT_BYTES (64U * 1024U * 1024U)
#define VQEC_VISION_AI_DSP_V1_IMAGE_MAX_OUTPUT_BYTES (128U * 1024U * 1024U)

#define VQEC_VISION_AI_DSP_V1_IMAGE_PIXEL_NV12 1U
#define VQEC_VISION_AI_DSP_V1_IMAGE_MATRIX_BT601 1U
#define VQEC_VISION_AI_DSP_V1_IMAGE_MATRIX_BT709 2U
#define VQEC_VISION_AI_DSP_V1_IMAGE_RANGE_LIMITED 1U
#define VQEC_VISION_AI_DSP_V1_IMAGE_RANGE_FULL 2U
#define VQEC_VISION_AI_DSP_V1_IMAGE_INTERPOLATION_BILINEAR 1U
#define VQEC_VISION_AI_DSP_V1_IMAGE_RESIZE_LETTERBOX 1U
#define VQEC_VISION_AI_DSP_V1_IMAGE_RESIZE_STRETCH 2U
#define VQEC_VISION_AI_DSP_V1_IMAGE_PLACEMENT_TOP_LEFT 1U
#define VQEC_VISION_AI_DSP_V1_IMAGE_PLACEMENT_CENTRE 2U
#define VQEC_VISION_AI_DSP_V1_IMAGE_PLACEMENT_STRETCH 3U
#define VQEC_VISION_AI_DSP_V1_IMAGE_CHANNEL_RGB 1U
#define VQEC_VISION_AI_DSP_V1_IMAGE_CHANNEL_BGR 2U
#define VQEC_VISION_AI_DSP_V1_IMAGE_NORMALIZATION_OFFSET_SCALE 1U
#define VQEC_VISION_AI_DSP_V1_IMAGE_DTYPE_UINT16 1U

typedef void (*vqec_vision_ai_dsp_v1_scale_luma_function)(
    const uint8_t* _source, uint32_t _source_width, uint32_t _source_height,
    uint32_t _source_stride, uint8_t* _destination, uint32_t _destination_width,
    uint32_t _destination_height, uint32_t _destination_stride);

typedef void (*vqec_vision_ai_dsp_v1_scale_chroma_function)(
    const uint8_t* _source, uint32_t _source_width, uint32_t _source_height,
    uint32_t _source_stride, uint8_t* _destination, uint32_t _destination_width,
    uint32_t _destination_height, uint32_t _destination_stride);

typedef void (*vqec_vision_ai_dsp_v1_color_function)(
    const uint8_t* _source_y, const uint8_t* _source_uv, uint32_t _width,
    uint32_t _height, uint32_t _y_stride, uint32_t _uv_stride,
    uint8_t* _destination, uint32_t _destination_stride);

typedef struct vqec_vision_ai_dsp_v1_image_backend {
    vqec_vision_ai_dsp_v1_scale_luma_function scale_luma;
    vqec_vision_ai_dsp_v1_scale_chroma_function scale_chroma;
    vqec_vision_ai_dsp_v1_color_function convert_color;
} vqec_vision_ai_dsp_v1_image_backend;

typedef struct vqec_vision_ai_dsp_v1_image_config {
    uint16_t pixel_format;
    uint16_t matrix;
    uint16_t range;
    uint16_t interpolation;
    uint16_t resize;
    uint16_t placement;
    uint16_t channel_order;
    uint16_t normalization;
    uint16_t dtype;
    uint32_t source_width;
    uint32_t source_height;
    uint32_t source_y_offset;
    uint32_t source_y_stride;
    uint32_t source_uv_offset;
    uint32_t source_uv_stride;
    uint32_t crop_x;
    uint32_t crop_y;
    uint32_t crop_width;
    uint32_t crop_height;
    uint32_t tensor_width;
    uint32_t tensor_height;
    uint32_t destination_x;
    uint32_t destination_y;
    uint32_t destination_width;
    uint32_t destination_height;
    float pad[3];
    float offset[3];
    float scale[3];
    float quantization_scale;
    int32_t quantization_zero_point;
} vqec_vision_ai_dsp_v1_image_config;

typedef struct vqec_vision_ai_dsp_v1_image_scratch {
    uint8_t* y;
    uint8_t* uv;
    uint8_t* rgb;
    uint32_t width;
    uint32_t height;
} vqec_vision_ai_dsp_v1_image_scratch;

vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_qcom_d1img_encode_descriptor(
    const vqec_vision_ai_dsp_v1_image_config* _config, uint32_t _domain_generation,
    size_t _input_bytes, size_t _output_bytes, uint8_t* _descriptor,
    size_t _descriptor_bytes);

vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_qcom_d1img_execute(
    const vqec_vision_ai_dsp_v1_capabilities* _capabilities,
    const vqec_vision_ai_dsp_v1_image_backend* _backend,
    vqec_vision_ai_dsp_v1_image_scratch* _scratch, const uint8_t* _descriptor,
    size_t _descriptor_bytes, const uint8_t* _input, size_t _input_bytes,
    uint8_t* _output, size_t _output_bytes, uint32_t* _written_bytes);

void vqec_vision_ai_qcom_d1img_release_scratch(
    vqec_vision_ai_dsp_v1_image_scratch* _scratch);

#ifdef __cplusplus
}
#endif

#endif
