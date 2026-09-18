#include "vqec_vision_dsp_v1_image.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    g_payload_version_offset = 32,
    g_payload_bytes_offset = 34,
    g_pixel_format_offset = 36,
    g_matrix_offset = 38,
    g_range_offset = 40,
    g_interpolation_offset = 42,
    g_resize_offset = 44,
    g_placement_offset = 46,
    g_channel_order_offset = 48,
    g_normalization_offset = 50,
    g_dtype_offset = 52,
    g_reserved_u16_offset = 54,
    g_source_width_offset = 56,
    g_source_height_offset = 60,
    g_source_y_offset_offset = 64,
    g_source_y_stride_offset = 68,
    g_source_uv_offset_offset = 72,
    g_source_uv_stride_offset = 76,
    g_crop_x_offset = 80,
    g_crop_y_offset = 84,
    g_crop_width_offset = 88,
    g_crop_height_offset = 92,
    g_tensor_width_offset = 96,
    g_tensor_height_offset = 100,
    g_destination_x_offset = 104,
    g_destination_y_offset = 108,
    g_destination_width_offset = 112,
    g_destination_height_offset = 116,
    g_pad_offset = 120,
    g_normalization_offset_offset = 132,
    g_normalization_scale_offset = 144,
    g_quantization_scale_offset = 156,
    g_quantization_zero_point_offset = 160,
    g_reserved_bytes_offset = 164
};

static uint16_t vqec_vision_ai_qcom_d1img_read_u16(const uint8_t* _bytes) {
    return (uint16_t)((uint16_t)_bytes[0] | ((uint16_t)_bytes[1] << 8));
}

static uint32_t vqec_vision_ai_qcom_d1img_read_u32(const uint8_t* _bytes) {
    return (uint32_t)_bytes[0] | ((uint32_t)_bytes[1] << 8) | ((uint32_t)_bytes[2] << 16) |
           ((uint32_t)_bytes[3] << 24);
}

static int32_t vqec_vision_ai_qcom_d1img_read_i32(const uint8_t* _bytes) {
    const uint32_t bits = vqec_vision_ai_qcom_d1img_read_u32(_bytes);
    int32_t value = 0;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static float vqec_vision_ai_qcom_d1img_read_f32(const uint8_t* _bytes) {
    const uint32_t bits = vqec_vision_ai_qcom_d1img_read_u32(_bytes);
    float value = 0.0F;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void vqec_vision_ai_qcom_d1img_write_u16(uint8_t* _bytes, uint16_t _value) {
    _bytes[0] = (uint8_t)_value;
    _bytes[1] = (uint8_t)(_value >> 8);
}

static void vqec_vision_ai_qcom_d1img_write_u32(uint8_t* _bytes, uint32_t _value) {
    _bytes[0] = (uint8_t)_value;
    _bytes[1] = (uint8_t)(_value >> 8);
    _bytes[2] = (uint8_t)(_value >> 16);
    _bytes[3] = (uint8_t)(_value >> 24);
}

static void vqec_vision_ai_qcom_d1img_write_i32(uint8_t* _bytes, int32_t _value) {
    uint32_t bits = 0U;
    memcpy(&bits, &_value, sizeof(bits));
    vqec_vision_ai_qcom_d1img_write_u32(_bytes, bits);
}

static void vqec_vision_ai_qcom_d1img_write_f32(uint8_t* _bytes, float _value) {
    uint32_t bits = 0U;
    memcpy(&bits, &_value, sizeof(bits));
    vqec_vision_ai_qcom_d1img_write_u32(_bytes, bits);
}

static int vqec_vision_ai_qcom_d1img_mul_size(size_t _left, size_t _right, size_t* _result) {
    if (_result == NULL || (_left != 0U && _right > SIZE_MAX / _left)) {
        return 0;
    }
    *_result = _left * _right;
    return 1;
}

static int vqec_vision_ai_qcom_d1img_add_size(size_t _left, size_t _right, size_t* _result) {
    if (_result == NULL || _right > SIZE_MAX - _left) {
        return 0;
    }
    *_result = _left + _right;
    return 1;
}

static int vqec_vision_ai_qcom_d1img_direct_mapping_valid(
    const vqec_vision_ai_dsp_v1_image_config* _config) {
    const double output_max = 65535.0;
    for (size_t channel = 0U; channel < 3U; ++channel) {
        if (!isfinite(_config->pad[channel]) || !isfinite(_config->offset[channel]) ||
            !isfinite(_config->scale[channel])) {
            return 0;
        }
        for (uint32_t pixel = 0U; pixel <= 255U; ++pixel) {
            const double real = ((double)pixel - (double)_config->offset[channel]) *
                                (double)_config->scale[channel];
            double stored = round(real / (double)_config->quantization_scale) +
                            (double)_config->quantization_zero_point;
            if (stored < 0.0) {
                stored = 0.0;
            } else if (stored > output_max) {
                stored = output_max;
            }
            if (fabs(stored - (double)(pixel * 257U)) > 1.0) {
                return 0;
            }
        }
    }
    return 1;
}

static vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_qcom_d1img_validate_config(
    const vqec_vision_ai_dsp_v1_image_config* _config, size_t _input_bytes,
    size_t _output_bytes) {
    if (_config == NULL || _config->pixel_format != VQEC_VISION_AI_DSP_V1_IMAGE_PIXEL_NV12 ||
        _config->matrix != VQEC_VISION_AI_DSP_V1_IMAGE_MATRIX_BT709 ||
        _config->range != VQEC_VISION_AI_DSP_V1_IMAGE_RANGE_LIMITED ||
        _config->interpolation != VQEC_VISION_AI_DSP_V1_IMAGE_INTERPOLATION_BILINEAR ||
        _config->resize != VQEC_VISION_AI_DSP_V1_IMAGE_RESIZE_LETTERBOX ||
        (_config->placement != VQEC_VISION_AI_DSP_V1_IMAGE_PLACEMENT_TOP_LEFT &&
         _config->placement != VQEC_VISION_AI_DSP_V1_IMAGE_PLACEMENT_CENTRE) ||
        _config->channel_order != VQEC_VISION_AI_DSP_V1_IMAGE_CHANNEL_RGB ||
        _config->normalization != VQEC_VISION_AI_DSP_V1_IMAGE_NORMALIZATION_OFFSET_SCALE ||
        _config->dtype != VQEC_VISION_AI_DSP_V1_IMAGE_DTYPE_UINT16 ||
        _config->source_width == 0U || _config->source_height == 0U ||
        _config->source_width > VQEC_VISION_AI_DSP_V1_IMAGE_MAX_SIDE ||
        _config->source_height > VQEC_VISION_AI_DSP_V1_IMAGE_MAX_SIDE ||
        (_config->source_width & 1U) != 0U || (_config->source_height & 1U) != 0U ||
        _config->source_y_stride < _config->source_width ||
        _config->source_uv_stride < _config->source_width ||
        _config->crop_x != 0U || _config->crop_y != 0U ||
        _config->crop_width != _config->source_width ||
        _config->crop_height != _config->source_height ||
        _config->tensor_width == 0U || _config->tensor_height == 0U ||
        _config->tensor_width > VQEC_VISION_AI_DSP_V1_IMAGE_MAX_SIDE ||
        _config->tensor_height > VQEC_VISION_AI_DSP_V1_IMAGE_MAX_SIDE ||
        _config->destination_width == 0U || _config->destination_height == 0U ||
        (_config->destination_width & 1U) != 0U ||
        (_config->destination_height & 1U) != 0U ||
        _config->destination_x > _config->tensor_width ||
        _config->destination_width > _config->tensor_width - _config->destination_x ||
        _config->destination_y > _config->tensor_height ||
        _config->destination_height > _config->tensor_height - _config->destination_y ||
        !isfinite(_config->quantization_scale) || _config->quantization_scale <= 0.0F) {
        return vqec_vision_ai_dsp_v1_wire_unsupported;
    }
    if (_config->pad[0] != _config->pad[1] || _config->pad[0] != _config->pad[2] ||
        _config->pad[0] < 0.0F || _config->pad[0] > 255.0F ||
        floorf(_config->pad[0]) != _config->pad[0] ||
        !vqec_vision_ai_qcom_d1img_direct_mapping_valid(_config)) {
        return vqec_vision_ai_dsp_v1_wire_unsupported;
    }
    size_t y_end = 0U;
    size_t uv_rows_bytes = 0U;
    size_t uv_end = 0U;
    size_t output_elements = 0U;
    size_t expected_output_bytes = 0U;
    if (!vqec_vision_ai_qcom_d1img_mul_size(_config->source_y_stride,
                                            _config->source_height, &y_end) ||
        !vqec_vision_ai_qcom_d1img_add_size(_config->source_y_offset, y_end, &y_end) ||
        !vqec_vision_ai_qcom_d1img_mul_size(_config->source_uv_stride,
                                            _config->source_height / 2U, &uv_rows_bytes) ||
        !vqec_vision_ai_qcom_d1img_add_size(_config->source_uv_offset, uv_rows_bytes,
                                            &uv_end) ||
        _config->source_uv_offset < y_end || uv_end > _input_bytes ||
        !vqec_vision_ai_qcom_d1img_mul_size(_config->tensor_width,
                                            _config->tensor_height, &output_elements) ||
        !vqec_vision_ai_qcom_d1img_mul_size(output_elements, 3U * sizeof(uint16_t),
                                            &expected_output_bytes) ||
        expected_output_bytes != _output_bytes) {
        return vqec_vision_ai_dsp_v1_wire_out_of_range;
    }
    return vqec_vision_ai_dsp_v1_wire_ok;
}

static vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_qcom_d1img_decode_descriptor(
    const uint8_t* _descriptor, size_t _descriptor_bytes, size_t _input_bytes,
    size_t _output_bytes, vqec_vision_ai_dsp_v1_image_config* _config) {
    if (_descriptor == NULL || _config == NULL ||
        _descriptor_bytes != VQEC_VISION_AI_DSP_V1_IMAGE_DESCRIPTOR_BYTES ||
        vqec_vision_ai_qcom_d1img_read_u16(_descriptor + g_payload_version_offset) !=
            VQEC_VISION_AI_BASELINE_SCHEMA_VERSION ||
        vqec_vision_ai_qcom_d1img_read_u16(_descriptor + g_payload_bytes_offset) !=
            VQEC_VISION_AI_DSP_V1_IMAGE_DESCRIPTOR_BYTES ||
        vqec_vision_ai_qcom_d1img_read_u16(_descriptor + g_reserved_u16_offset) != 0U) {
        return vqec_vision_ai_dsp_v1_wire_malformed;
    }
    for (size_t index = g_reserved_bytes_offset;
         index < VQEC_VISION_AI_DSP_V1_IMAGE_DESCRIPTOR_BYTES; ++index) {
        if (_descriptor[index] != 0U) {
            return vqec_vision_ai_dsp_v1_wire_malformed;
        }
    }
    memset(_config, 0, sizeof(*_config));
    _config->pixel_format = vqec_vision_ai_qcom_d1img_read_u16(_descriptor + g_pixel_format_offset);
    _config->matrix = vqec_vision_ai_qcom_d1img_read_u16(_descriptor + g_matrix_offset);
    _config->range = vqec_vision_ai_qcom_d1img_read_u16(_descriptor + g_range_offset);
    _config->interpolation = vqec_vision_ai_qcom_d1img_read_u16(_descriptor + g_interpolation_offset);
    _config->resize = vqec_vision_ai_qcom_d1img_read_u16(_descriptor + g_resize_offset);
    _config->placement = vqec_vision_ai_qcom_d1img_read_u16(_descriptor + g_placement_offset);
    _config->channel_order = vqec_vision_ai_qcom_d1img_read_u16(_descriptor + g_channel_order_offset);
    _config->normalization = vqec_vision_ai_qcom_d1img_read_u16(_descriptor + g_normalization_offset);
    _config->dtype = vqec_vision_ai_qcom_d1img_read_u16(_descriptor + g_dtype_offset);
    _config->source_width = vqec_vision_ai_qcom_d1img_read_u32(_descriptor + g_source_width_offset);
    _config->source_height = vqec_vision_ai_qcom_d1img_read_u32(_descriptor + g_source_height_offset);
    _config->source_y_offset = vqec_vision_ai_qcom_d1img_read_u32(_descriptor + g_source_y_offset_offset);
    _config->source_y_stride = vqec_vision_ai_qcom_d1img_read_u32(_descriptor + g_source_y_stride_offset);
    _config->source_uv_offset = vqec_vision_ai_qcom_d1img_read_u32(_descriptor + g_source_uv_offset_offset);
    _config->source_uv_stride = vqec_vision_ai_qcom_d1img_read_u32(_descriptor + g_source_uv_stride_offset);
    _config->crop_x = vqec_vision_ai_qcom_d1img_read_u32(_descriptor + g_crop_x_offset);
    _config->crop_y = vqec_vision_ai_qcom_d1img_read_u32(_descriptor + g_crop_y_offset);
    _config->crop_width = vqec_vision_ai_qcom_d1img_read_u32(_descriptor + g_crop_width_offset);
    _config->crop_height = vqec_vision_ai_qcom_d1img_read_u32(_descriptor + g_crop_height_offset);
    _config->tensor_width = vqec_vision_ai_qcom_d1img_read_u32(_descriptor + g_tensor_width_offset);
    _config->tensor_height = vqec_vision_ai_qcom_d1img_read_u32(_descriptor + g_tensor_height_offset);
    _config->destination_x = vqec_vision_ai_qcom_d1img_read_u32(_descriptor + g_destination_x_offset);
    _config->destination_y = vqec_vision_ai_qcom_d1img_read_u32(_descriptor + g_destination_y_offset);
    _config->destination_width = vqec_vision_ai_qcom_d1img_read_u32(_descriptor + g_destination_width_offset);
    _config->destination_height = vqec_vision_ai_qcom_d1img_read_u32(_descriptor + g_destination_height_offset);
    for (size_t channel = 0U; channel < 3U; ++channel) {
        _config->pad[channel] = vqec_vision_ai_qcom_d1img_read_f32(
            _descriptor + g_pad_offset + channel * sizeof(float));
        _config->offset[channel] = vqec_vision_ai_qcom_d1img_read_f32(
            _descriptor + g_normalization_offset_offset + channel * sizeof(float));
        _config->scale[channel] = vqec_vision_ai_qcom_d1img_read_f32(
            _descriptor + g_normalization_scale_offset + channel * sizeof(float));
    }
    _config->quantization_scale =
        vqec_vision_ai_qcom_d1img_read_f32(_descriptor + g_quantization_scale_offset);
    _config->quantization_zero_point =
        vqec_vision_ai_qcom_d1img_read_i32(_descriptor + g_quantization_zero_point_offset);
    return vqec_vision_ai_qcom_d1img_validate_config(_config, _input_bytes, _output_bytes);
}

vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_qcom_d1img_encode_descriptor(
    const vqec_vision_ai_dsp_v1_image_config* _config, uint32_t _domain_generation,
    size_t _input_bytes, size_t _output_bytes, uint8_t* _descriptor,
    size_t _descriptor_bytes) {
    if (_config == NULL || _descriptor == NULL ||
        _descriptor_bytes != VQEC_VISION_AI_DSP_V1_IMAGE_DESCRIPTOR_BYTES ||
        _input_bytes == 0U || _input_bytes > UINT32_MAX || _output_bytes == 0U ||
        _output_bytes > UINT32_MAX) {
        return vqec_vision_ai_dsp_v1_wire_malformed;
    }
    const vqec_vision_ai_dsp_v1_wire_status config_status =
        vqec_vision_ai_qcom_d1img_validate_config(_config, _input_bytes, _output_bytes);
    if (config_status != vqec_vision_ai_dsp_v1_wire_ok) {
        return config_status;
    }
    memset(_descriptor, 0, _descriptor_bytes);
    const vqec_vision_ai_dsp_v1_request request = {
        VQEC_VISION_AI_DSP_V1_IMAGE_TRANSFORM,
        VQEC_VISION_AI_DSP_V1_IMAGE_DESCRIPTOR_BYTES,
        (uint32_t)_input_bytes, (uint32_t)_output_bytes, _domain_generation};
    const vqec_vision_ai_dsp_v1_wire_status envelope =
        vqec_vision_ai_qcom_dvwir_encode_request(&request, _descriptor, _descriptor_bytes);
    if (envelope != vqec_vision_ai_dsp_v1_wire_ok) {
        return envelope;
    }
    vqec_vision_ai_qcom_d1img_write_u16(_descriptor + g_payload_version_offset,
                                        VQEC_VISION_AI_BASELINE_SCHEMA_VERSION);
    vqec_vision_ai_qcom_d1img_write_u16(_descriptor + g_payload_bytes_offset,
                                        VQEC_VISION_AI_DSP_V1_IMAGE_DESCRIPTOR_BYTES);
#define VQEC_VISION_AI_D1IMG_WRITE_U16(field, offset) \
    vqec_vision_ai_qcom_d1img_write_u16(_descriptor + offset, _config->field)
#define VQEC_VISION_AI_D1IMG_WRITE_U32(field, offset) \
    vqec_vision_ai_qcom_d1img_write_u32(_descriptor + offset, _config->field)
    VQEC_VISION_AI_D1IMG_WRITE_U16(pixel_format, g_pixel_format_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U16(matrix, g_matrix_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U16(range, g_range_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U16(interpolation, g_interpolation_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U16(resize, g_resize_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U16(placement, g_placement_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U16(channel_order, g_channel_order_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U16(normalization, g_normalization_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U16(dtype, g_dtype_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U32(source_width, g_source_width_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U32(source_height, g_source_height_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U32(source_y_offset, g_source_y_offset_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U32(source_y_stride, g_source_y_stride_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U32(source_uv_offset, g_source_uv_offset_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U32(source_uv_stride, g_source_uv_stride_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U32(crop_x, g_crop_x_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U32(crop_y, g_crop_y_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U32(crop_width, g_crop_width_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U32(crop_height, g_crop_height_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U32(tensor_width, g_tensor_width_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U32(tensor_height, g_tensor_height_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U32(destination_x, g_destination_x_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U32(destination_y, g_destination_y_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U32(destination_width, g_destination_width_offset);
    VQEC_VISION_AI_D1IMG_WRITE_U32(destination_height, g_destination_height_offset);
#undef VQEC_VISION_AI_D1IMG_WRITE_U16
#undef VQEC_VISION_AI_D1IMG_WRITE_U32
    for (size_t channel = 0U; channel < 3U; ++channel) {
        vqec_vision_ai_qcom_d1img_write_f32(
            _descriptor + g_pad_offset + channel * sizeof(float), _config->pad[channel]);
        vqec_vision_ai_qcom_d1img_write_f32(
            _descriptor + g_normalization_offset_offset + channel * sizeof(float),
            _config->offset[channel]);
        vqec_vision_ai_qcom_d1img_write_f32(
            _descriptor + g_normalization_scale_offset + channel * sizeof(float),
            _config->scale[channel]);
    }
    vqec_vision_ai_qcom_d1img_write_f32(_descriptor + g_quantization_scale_offset,
                                        _config->quantization_scale);
    vqec_vision_ai_qcom_d1img_write_i32(_descriptor + g_quantization_zero_point_offset,
                                        _config->quantization_zero_point);
    vqec_vision_ai_dsp_v1_image_config decoded;
    return vqec_vision_ai_qcom_d1img_decode_descriptor(
        _descriptor, _descriptor_bytes, _input_bytes, _output_bytes, &decoded);
}

void vqec_vision_ai_qcom_d1img_release_scratch(
    vqec_vision_ai_dsp_v1_image_scratch* _scratch) {
    if (_scratch == NULL) {
        return;
    }
    free(_scratch->y);
    free(_scratch->uv);
    free(_scratch->rgb);
    memset(_scratch, 0, sizeof(*_scratch));
}

static int vqec_vision_ai_qcom_d1img_ensure_scratch(
    vqec_vision_ai_dsp_v1_image_scratch* _scratch, uint32_t _width, uint32_t _height) {
    if (_scratch->y != NULL && _scratch->uv != NULL && _scratch->rgb != NULL &&
        _scratch->width >= _width && _scratch->height >= _height) {
        return 1;
    }
    size_t pixels = 0U;
    size_t rgb_bytes = 0U;
    if (!vqec_vision_ai_qcom_d1img_mul_size(_width, _height, &pixels) ||
        !vqec_vision_ai_qcom_d1img_mul_size(pixels, 3U, &rgb_bytes)) {
        return 0;
    }
    uint8_t* y = (uint8_t*)malloc(pixels);
    uint8_t* uv = (uint8_t*)malloc(pixels / 2U);
    uint8_t* rgb = (uint8_t*)malloc(rgb_bytes);
    if (y == NULL || uv == NULL || rgb == NULL) {
        free(y);
        free(uv);
        free(rgb);
        return 0;
    }
    vqec_vision_ai_qcom_d1img_release_scratch(_scratch);
    _scratch->y = y;
    _scratch->uv = uv;
    _scratch->rgb = rgb;
    _scratch->width = _width;
    _scratch->height = _height;
    return 1;
}

vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_qcom_d1img_execute(
    const vqec_vision_ai_dsp_v1_capabilities* _capabilities,
    const vqec_vision_ai_dsp_v1_image_backend* _backend,
    vqec_vision_ai_dsp_v1_image_scratch* _scratch, const uint8_t* _descriptor,
    size_t _descriptor_bytes, const uint8_t* _input, size_t _input_bytes,
    uint8_t* _output, size_t _output_bytes, uint32_t* _written_bytes) {
    if (_capabilities == NULL || _backend == NULL || _scratch == NULL || _input == NULL ||
        _output == NULL || _written_bytes == NULL || _backend->scale_luma == NULL ||
        _backend->scale_chroma == NULL || _backend->convert_color == NULL) {
        return vqec_vision_ai_dsp_v1_wire_malformed;
    }
    vqec_vision_ai_dsp_v1_request request;
    vqec_vision_ai_dsp_v1_wire_status status = vqec_vision_ai_qcom_dvwir_validate_request(
        _capabilities, _descriptor, _descriptor_bytes, _input_bytes, _output_bytes, &request);
    if (status != vqec_vision_ai_dsp_v1_wire_ok ||
        request.operation != VQEC_VISION_AI_DSP_V1_IMAGE_TRANSFORM) {
        return status != vqec_vision_ai_dsp_v1_wire_ok ? status :
            vqec_vision_ai_dsp_v1_wire_unsupported;
    }
    vqec_vision_ai_dsp_v1_image_config config;
    status = vqec_vision_ai_qcom_d1img_decode_descriptor(
        _descriptor, _descriptor_bytes, _input_bytes, _output_bytes, &config);
    if (status != vqec_vision_ai_dsp_v1_wire_ok) {
        return status;
    }
    if (!vqec_vision_ai_qcom_d1img_ensure_scratch(
            _scratch, config.destination_width, config.destination_height)) {
        return vqec_vision_ai_dsp_v1_wire_out_of_range;
    }
    const uint8_t* source_y = _input + config.source_y_offset;
    const uint8_t* source_uv = _input + config.source_uv_offset;
    _backend->scale_luma(source_y, config.source_width, config.source_height,
                         config.source_y_stride, _scratch->y, config.destination_width,
                         config.destination_height, config.destination_width);
    _backend->scale_chroma(source_uv, config.source_width / 2U,
                           config.source_height / 2U, config.source_uv_stride,
                           _scratch->uv, config.destination_width / 2U,
                           config.destination_height / 2U, config.destination_width);
    _backend->convert_color(_scratch->y, _scratch->uv, config.destination_width,
                            config.destination_height, config.destination_width,
                            config.destination_width, _scratch->rgb,
                            config.destination_width * 3U);

    const uint16_t pad = (uint16_t)((uint32_t)config.pad[0] * 257U);
    uint16_t* tensor = (uint16_t*)_output;
    const size_t tensor_elements =
        (size_t)config.tensor_width * (size_t)config.tensor_height * 3U;
    for (size_t index = 0U; index < tensor_elements; ++index) {
        tensor[index] = pad;
    }
    for (uint32_t row = 0U; row < config.destination_height; ++row) {
        const uint8_t* source = _scratch->rgb + (size_t)row * config.destination_width * 3U;
        uint16_t* destination = tensor +
            ((size_t)(config.destination_y + row) * config.tensor_width +
             config.destination_x) * 3U;
        const size_t row_elements = (size_t)config.destination_width * 3U;
        for (size_t index = 0U; index < row_elements; ++index) {
            const uint8_t value = source[index];
            destination[index] = (uint16_t)(((uint16_t)value << 8U) | value);
        }
    }
    *_written_bytes = (uint32_t)_output_bytes;
    return vqec_vision_ai_dsp_v1_wire_ok;
}
