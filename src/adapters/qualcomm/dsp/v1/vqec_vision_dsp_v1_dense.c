#include "vqec_vision_dsp_v1_dense.h"

#include <math.h>
#include <string.h>

enum {
    g_payload_version_offset = 32,
    g_payload_bytes_offset = 34,
    g_dtype_offset = 36,
    g_box_encoding_offset = 38,
    g_score_encoding_offset = 40,
    g_layout_offset = 42,
    g_flags_offset = 44,
    g_prediction_count_offset = 48,
    g_class_count_offset = 52,
    g_box_offset_offset = 56,
    g_score_offset_offset = 60,
    g_source_width_offset = 64,
    g_source_height_offset = 68,
    g_candidate_capacity_offset = 72,
    g_output_capacity_offset = 76,
    g_box_scale_offset = 80,
    g_box_zero_point_offset = 84,
    g_score_scale_offset = 88,
    g_score_zero_point_offset = 92,
    g_confidence_threshold_offset = 96,
    g_iou_threshold_offset = 100,
    g_scale_x_offset = 104,
    g_scale_y_offset = 108,
    g_pad_x_offset = 112,
    g_pad_y_offset = 116
};

static uint16_t vqec_vision_ai_qcom_d1dns_read_u16(const uint8_t* _bytes) {
    return (uint16_t)((uint16_t)_bytes[0] | ((uint16_t)_bytes[1] << 8));
}

static void vqec_vision_ai_qcom_d1dns_write_u16(uint8_t* _bytes, uint16_t _value) {
    _bytes[0] = (uint8_t)_value;
    _bytes[1] = (uint8_t)(_value >> 8);
}

static uint32_t vqec_vision_ai_qcom_d1dns_read_u32(const uint8_t* _bytes) {
    return (uint32_t)_bytes[0] | ((uint32_t)_bytes[1] << 8) | ((uint32_t)_bytes[2] << 16) |
           ((uint32_t)_bytes[3] << 24);
}

static int32_t vqec_vision_ai_qcom_d1dns_read_i32(const uint8_t* _bytes) {
    uint32_t bits = vqec_vision_ai_qcom_d1dns_read_u32(_bytes);
    int32_t value = 0;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static float vqec_vision_ai_qcom_d1dns_read_f32(const uint8_t* _bytes) {
    uint32_t bits = vqec_vision_ai_qcom_d1dns_read_u32(_bytes);
    float value = 0.0F;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void vqec_vision_ai_qcom_d1dns_write_u32(uint8_t* _bytes, uint32_t _value) {
    _bytes[0] = (uint8_t)_value;
    _bytes[1] = (uint8_t)(_value >> 8);
    _bytes[2] = (uint8_t)(_value >> 16);
    _bytes[3] = (uint8_t)(_value >> 24);
}

static void vqec_vision_ai_qcom_d1dns_write_i32(uint8_t* _bytes, int32_t _value) {
    uint32_t bits = 0;
    memcpy(&bits, &_value, sizeof(bits));
    vqec_vision_ai_qcom_d1dns_write_u32(_bytes, bits);
}

static void vqec_vision_ai_qcom_d1dns_write_f32(uint8_t* _bytes, float _value) {
    uint32_t bits = 0;
    memcpy(&bits, &_value, sizeof(bits));
    vqec_vision_ai_qcom_d1dns_write_u32(_bytes, bits);
}

static int vqec_vision_ai_qcom_d1dns_multiply_size(size_t _left, size_t _right, size_t* _product) {
    if (_product == NULL || (_left != 0 && _right > SIZE_MAX / _left)) {
        return 0;
    }
    *_product = _left * _right;
    return 1;
}

static int vqec_vision_ai_qcom_d1dns_add_size(size_t _left, size_t _right, size_t* _sum) {
    if (_sum == NULL || _right > SIZE_MAX - _left) {
        return 0;
    }
    *_sum = _left + _right;
    return 1;
}

static vqec_vision_ai_dsp_v1_wire_status
vqec_vision_ai_qcom_d1dns_decode_descriptor(const uint8_t* _descriptor, size_t _descriptor_bytes,
                                            size_t _input_bytes, size_t _output_capacity_bytes,
                                            vqec_vision_ai_dsp_v1_dense_config* _decoded) {
    if (_descriptor == NULL || _decoded == NULL ||
        _descriptor_bytes != VQEC_VISION_AI_DSP_V1_DENSE_DESCRIPTOR_BYTES ||
        vqec_vision_ai_qcom_d1dns_read_u16(_descriptor + g_payload_version_offset) !=
            VQEC_VISION_AI_BASELINE_SCHEMA_VERSION ||
        vqec_vision_ai_qcom_d1dns_read_u16(_descriptor + g_payload_bytes_offset) !=
            VQEC_VISION_AI_DSP_V1_DENSE_DESCRIPTOR_BYTES ||
        vqec_vision_ai_qcom_d1dns_read_u16(_descriptor + g_dtype_offset) !=
            VQEC_VISION_AI_DSP_V1_DENSE_DTYPE_UINT16 ||
        vqec_vision_ai_qcom_d1dns_read_u16(_descriptor + g_box_encoding_offset) !=
            VQEC_VISION_AI_DSP_V1_DENSE_BOX_XYWH ||
        vqec_vision_ai_qcom_d1dns_read_u16(_descriptor + g_score_encoding_offset) !=
            VQEC_VISION_AI_DSP_V1_DENSE_SCORE_PROBABILITY ||
        vqec_vision_ai_qcom_d1dns_read_u16(_descriptor + g_layout_offset) !=
            VQEC_VISION_AI_DSP_V1_DENSE_LAYOUT_CHANNEL_MAJOR) {
        return vqec_vision_ai_dsp_v1_wire_malformed;
    }

    vqec_vision_ai_dsp_v1_dense_config value;
    value.flags = vqec_vision_ai_qcom_d1dns_read_u32(_descriptor + g_flags_offset);
    value.prediction_count =
        vqec_vision_ai_qcom_d1dns_read_u32(_descriptor + g_prediction_count_offset);
    value.class_count = vqec_vision_ai_qcom_d1dns_read_u32(_descriptor + g_class_count_offset);
    value.box_offset = vqec_vision_ai_qcom_d1dns_read_u32(_descriptor + g_box_offset_offset);
    value.score_offset = vqec_vision_ai_qcom_d1dns_read_u32(_descriptor + g_score_offset_offset);
    value.source_width = vqec_vision_ai_qcom_d1dns_read_u32(_descriptor + g_source_width_offset);
    value.source_height = vqec_vision_ai_qcom_d1dns_read_u32(_descriptor + g_source_height_offset);
    value.candidate_capacity =
        vqec_vision_ai_qcom_d1dns_read_u32(_descriptor + g_candidate_capacity_offset);
    value.output_capacity =
        vqec_vision_ai_qcom_d1dns_read_u32(_descriptor + g_output_capacity_offset);
    value.box_scale = vqec_vision_ai_qcom_d1dns_read_f32(_descriptor + g_box_scale_offset);
    value.box_zero_point =
        vqec_vision_ai_qcom_d1dns_read_i32(_descriptor + g_box_zero_point_offset);
    value.score_scale = vqec_vision_ai_qcom_d1dns_read_f32(_descriptor + g_score_scale_offset);
    value.score_zero_point =
        vqec_vision_ai_qcom_d1dns_read_i32(_descriptor + g_score_zero_point_offset);
    value.confidence_threshold =
        vqec_vision_ai_qcom_d1dns_read_f32(_descriptor + g_confidence_threshold_offset);
    value.iou_threshold = vqec_vision_ai_qcom_d1dns_read_f32(_descriptor + g_iou_threshold_offset);
    value.scale_x = vqec_vision_ai_qcom_d1dns_read_f32(_descriptor + g_scale_x_offset);
    value.scale_y = vqec_vision_ai_qcom_d1dns_read_f32(_descriptor + g_scale_y_offset);
    value.pad_x = vqec_vision_ai_qcom_d1dns_read_f32(_descriptor + g_pad_x_offset);
    value.pad_y = vqec_vision_ai_qcom_d1dns_read_f32(_descriptor + g_pad_y_offset);

    if ((value.flags & ~VQEC_VISION_AI_DSP_V1_DENSE_CLASS_AWARE_NMS) != 0 ||
        value.prediction_count == 0 ||
        value.prediction_count > VQEC_VISION_AI_DSP_V1_DENSE_MAX_PREDICTIONS ||
        value.class_count == 0 || value.class_count > VQEC_VISION_AI_DSP_V1_DENSE_MAX_CLASSES ||
        value.source_width == 0 || value.source_height == 0 || value.candidate_capacity == 0 ||
        value.candidate_capacity > VQEC_VISION_AI_DSP_V1_DENSE_MAX_CANDIDATES ||
        value.output_capacity == 0 ||
        value.output_capacity > VQEC_VISION_AI_DSP_V1_DENSE_MAX_OUTPUTS ||
        value.output_capacity > value.candidate_capacity || !isfinite(value.box_scale) ||
        value.box_scale <= 0.0F || !isfinite(value.score_scale) || value.score_scale <= 0.0F ||
        !isfinite(value.confidence_threshold) || value.confidence_threshold < 0.0F ||
        value.confidence_threshold > 1.0F || !isfinite(value.iou_threshold) ||
        value.iou_threshold < 0.0F || value.iou_threshold > 1.0F || !isfinite(value.scale_x) ||
        value.scale_x <= 0.0F || !isfinite(value.scale_y) || value.scale_y <= 0.0F ||
        !isfinite(value.pad_x) || !isfinite(value.pad_y)) {
        return vqec_vision_ai_dsp_v1_wire_out_of_range;
    }

    size_t box_elements = 0;
    size_t score_elements = 0;
    size_t box_bytes = 0;
    size_t score_bytes = 0;
    size_t box_end = 0;
    size_t score_end = 0;
    size_t input_bytes = 0;
    size_t output_bytes = 0;
    if (!vqec_vision_ai_qcom_d1dns_multiply_size(value.prediction_count, 4U, &box_elements) ||
        !vqec_vision_ai_qcom_d1dns_multiply_size(value.prediction_count, value.class_count,
                                                 &score_elements) ||
        !vqec_vision_ai_qcom_d1dns_multiply_size(box_elements, sizeof(uint16_t), &box_bytes) ||
        !vqec_vision_ai_qcom_d1dns_multiply_size(score_elements, sizeof(uint16_t), &score_bytes) ||
        !vqec_vision_ai_qcom_d1dns_add_size(value.box_offset, box_bytes, &box_end) ||
        !vqec_vision_ai_qcom_d1dns_add_size(value.score_offset, score_bytes, &score_end) ||
        !vqec_vision_ai_qcom_d1dns_add_size(box_bytes, score_bytes, &input_bytes) ||
        input_bytes != _input_bytes ||
        !((value.box_offset == 0U && value.score_offset == box_bytes &&
           score_end == _input_bytes) ||
          (value.score_offset == 0U && value.box_offset == score_bytes &&
           box_end == _input_bytes)) ||
        !vqec_vision_ai_qcom_d1dns_multiply_size(
            value.output_capacity, VQEC_VISION_AI_DSP_V1_DENSE_RECORD_BYTES, &output_bytes) ||
        output_bytes > _output_capacity_bytes) {
        return vqec_vision_ai_dsp_v1_wire_out_of_range;
    }
    *_decoded = value;
    return vqec_vision_ai_dsp_v1_wire_ok;
}

vqec_vision_ai_dsp_v1_wire_status
vqec_vision_ai_qcom_d1dns_encode_descriptor(const vqec_vision_ai_dsp_v1_dense_config* _config,
                                            uint32_t _domain_generation, size_t _input_bytes,
                                            size_t _output_capacity_bytes, uint8_t* _descriptor,
                                            size_t _descriptor_bytes) {
    if (_config == NULL || _descriptor == NULL ||
        _descriptor_bytes != VQEC_VISION_AI_DSP_V1_DENSE_DESCRIPTOR_BYTES || _input_bytes == 0U ||
        _input_bytes > UINT32_MAX || _output_capacity_bytes == 0U ||
        _output_capacity_bytes > UINT32_MAX) {
        return vqec_vision_ai_dsp_v1_wire_malformed;
    }
    memset(_descriptor, 0, _descriptor_bytes);
    const vqec_vision_ai_dsp_v1_request request = {
        VQEC_VISION_AI_DSP_V1_DENSE_DECODE, VQEC_VISION_AI_DSP_V1_DENSE_DESCRIPTOR_BYTES,
        (uint32_t)_input_bytes, (uint32_t)_output_capacity_bytes, _domain_generation};
    const vqec_vision_ai_dsp_v1_wire_status envelope =
        vqec_vision_ai_qcom_dvwir_encode_request(&request, _descriptor, _descriptor_bytes);
    if (envelope != vqec_vision_ai_dsp_v1_wire_ok) {
        return envelope;
    }
    vqec_vision_ai_qcom_d1dns_write_u16(_descriptor + g_payload_version_offset,
                                        VQEC_VISION_AI_BASELINE_SCHEMA_VERSION);
    vqec_vision_ai_qcom_d1dns_write_u16(_descriptor + g_payload_bytes_offset,
                                        VQEC_VISION_AI_DSP_V1_DENSE_DESCRIPTOR_BYTES);
    vqec_vision_ai_qcom_d1dns_write_u16(_descriptor + g_dtype_offset,
                                        VQEC_VISION_AI_DSP_V1_DENSE_DTYPE_UINT16);
    vqec_vision_ai_qcom_d1dns_write_u16(_descriptor + g_box_encoding_offset,
                                        VQEC_VISION_AI_DSP_V1_DENSE_BOX_XYWH);
    vqec_vision_ai_qcom_d1dns_write_u16(_descriptor + g_score_encoding_offset,
                                        VQEC_VISION_AI_DSP_V1_DENSE_SCORE_PROBABILITY);
    vqec_vision_ai_qcom_d1dns_write_u16(_descriptor + g_layout_offset,
                                        VQEC_VISION_AI_DSP_V1_DENSE_LAYOUT_CHANNEL_MAJOR);
    vqec_vision_ai_qcom_d1dns_write_u32(_descriptor + g_flags_offset, _config->flags);
    vqec_vision_ai_qcom_d1dns_write_u32(_descriptor + g_prediction_count_offset,
                                        _config->prediction_count);
    vqec_vision_ai_qcom_d1dns_write_u32(_descriptor + g_class_count_offset, _config->class_count);
    vqec_vision_ai_qcom_d1dns_write_u32(_descriptor + g_box_offset_offset, _config->box_offset);
    vqec_vision_ai_qcom_d1dns_write_u32(_descriptor + g_score_offset_offset, _config->score_offset);
    vqec_vision_ai_qcom_d1dns_write_u32(_descriptor + g_source_width_offset, _config->source_width);
    vqec_vision_ai_qcom_d1dns_write_u32(_descriptor + g_source_height_offset,
                                        _config->source_height);
    vqec_vision_ai_qcom_d1dns_write_u32(_descriptor + g_candidate_capacity_offset,
                                        _config->candidate_capacity);
    vqec_vision_ai_qcom_d1dns_write_u32(_descriptor + g_output_capacity_offset,
                                        _config->output_capacity);
    vqec_vision_ai_qcom_d1dns_write_f32(_descriptor + g_box_scale_offset, _config->box_scale);
    vqec_vision_ai_qcom_d1dns_write_i32(_descriptor + g_box_zero_point_offset,
                                        _config->box_zero_point);
    vqec_vision_ai_qcom_d1dns_write_f32(_descriptor + g_score_scale_offset, _config->score_scale);
    vqec_vision_ai_qcom_d1dns_write_i32(_descriptor + g_score_zero_point_offset,
                                        _config->score_zero_point);
    vqec_vision_ai_qcom_d1dns_write_f32(_descriptor + g_confidence_threshold_offset,
                                        _config->confidence_threshold);
    vqec_vision_ai_qcom_d1dns_write_f32(_descriptor + g_iou_threshold_offset,
                                        _config->iou_threshold);
    vqec_vision_ai_qcom_d1dns_write_f32(_descriptor + g_scale_x_offset, _config->scale_x);
    vqec_vision_ai_qcom_d1dns_write_f32(_descriptor + g_scale_y_offset, _config->scale_y);
    vqec_vision_ai_qcom_d1dns_write_f32(_descriptor + g_pad_x_offset, _config->pad_x);
    vqec_vision_ai_qcom_d1dns_write_f32(_descriptor + g_pad_y_offset, _config->pad_y);

    vqec_vision_ai_dsp_v1_dense_config decoded;
    return vqec_vision_ai_qcom_d1dns_decode_descriptor(_descriptor, _descriptor_bytes, _input_bytes,
                                                       _output_capacity_bytes, &decoded);
}

static float vqec_vision_ai_qcom_d1dns_dequantize(uint16_t _value, float _scale,
                                                  int32_t _zero_point) {
    return ((float)_value - (float)_zero_point) * _scale;
}

static float vqec_vision_ai_qcom_d1dns_clamp(float _value, float _maximum) {
    return _value < 0.0F ? 0.0F : (_value > _maximum ? _maximum : _value);
}

static float vqec_vision_ai_qcom_d1dns_iou(const vqec_vision_ai_dsp_v1_dense_candidate* _left,
                                           const vqec_vision_ai_dsp_v1_dense_candidate* _right) {
    const float intersection_width = fminf(_left->x2, _right->x2) - fmaxf(_left->x1, _right->x1);
    const float intersection_height = fminf(_left->y2, _right->y2) - fmaxf(_left->y1, _right->y1);
    if (intersection_width <= 0.0F || intersection_height <= 0.0F) {
        return 0.0F;
    }
    const float intersection = intersection_width * intersection_height;
    const float union_area = (_left->x2 - _left->x1) * (_left->y2 - _left->y1) +
                             (_right->x2 - _right->x1) * (_right->y2 - _right->y1) - intersection;
    return union_area > 0.0F ? intersection / union_area : 0.0F;
}

static void vqec_vision_ai_qcom_d1dns_store_candidate(
    vqec_vision_ai_dsp_v1_dense_candidate* _candidate, float _centre_x, float _centre_y,
    float _width, float _height, float _score, uint32_t _class_index, uint32_t _prediction_index,
    const vqec_vision_ai_dsp_v1_dense_config* _descriptor) {
    const float half_width = _width * 0.5F;
    const float half_height = _height * 0.5F;
    _candidate->x1 = vqec_vision_ai_qcom_d1dns_clamp((_centre_x - half_width - _descriptor->pad_x) /
                                                         _descriptor->scale_x,
                                                     (float)_descriptor->source_width);
    _candidate->y1 = vqec_vision_ai_qcom_d1dns_clamp(
        (_centre_y - half_height - _descriptor->pad_y) / _descriptor->scale_y,
        (float)_descriptor->source_height);
    _candidate->x2 = vqec_vision_ai_qcom_d1dns_clamp((_centre_x + half_width - _descriptor->pad_x) /
                                                         _descriptor->scale_x,
                                                     (float)_descriptor->source_width);
    _candidate->y2 = vqec_vision_ai_qcom_d1dns_clamp(
        (_centre_y + half_height - _descriptor->pad_y) / _descriptor->scale_y,
        (float)_descriptor->source_height);
    _candidate->score = _score;
    _candidate->class_index = _class_index;
    _candidate->prediction_index = _prediction_index;
}

static int vqec_vision_ai_qcom_d1dns_precedes(const vqec_vision_ai_dsp_v1_dense_candidate* _left,
                                              const vqec_vision_ai_dsp_v1_dense_candidate* _right) {
    if (_left->score != _right->score) {
        return _left->score > _right->score;
    }
    if (_left->class_index != _right->class_index) {
        return _left->class_index < _right->class_index;
    }
    return _left->prediction_index < _right->prediction_index;
}

static int vqec_vision_ai_qcom_d1dns_weaker(const vqec_vision_ai_dsp_v1_dense_candidate* _left,
                                            const vqec_vision_ai_dsp_v1_dense_candidate* _right) {
    return vqec_vision_ai_qcom_d1dns_precedes(_right, _left);
}

static void
vqec_vision_ai_qcom_d1dns_swap_candidate(vqec_vision_ai_dsp_v1_dense_candidate* _left,
                                         vqec_vision_ai_dsp_v1_dense_candidate* _right) {
    const vqec_vision_ai_dsp_v1_dense_candidate value = *_left;
    *_left = *_right;
    *_right = value;
}

static void vqec_vision_ai_qcom_d1dns_heap_insert(vqec_vision_ai_dsp_v1_dense_scratch* _scratch,
                                                  uint32_t _index) {
    uint32_t child = _index;
    while (child > 0U) {
        const uint32_t parent = (child - 1U) / 2U;
        if (!vqec_vision_ai_qcom_d1dns_weaker(&_scratch->candidates[child],
                                              &_scratch->candidates[parent])) {
            break;
        }
        vqec_vision_ai_qcom_d1dns_swap_candidate(&_scratch->candidates[child],
                                                 &_scratch->candidates[parent]);
        child = parent;
    }
}

static void vqec_vision_ai_qcom_d1dns_heap_replace_root(
    vqec_vision_ai_dsp_v1_dense_scratch* _scratch, uint32_t _count,
    const vqec_vision_ai_dsp_v1_dense_candidate* _candidate) {
    _scratch->candidates[0] = *_candidate;
    uint32_t parent = 0U;
    while (1) {
        const uint32_t left = parent * 2U + 1U;
        if (left >= _count) {
            return;
        }
        const uint32_t right = left + 1U;
        uint32_t weaker = left;
        if (right < _count && vqec_vision_ai_qcom_d1dns_weaker(&_scratch->candidates[right],
                                                               &_scratch->candidates[left])) {
            weaker = right;
        }
        if (!vqec_vision_ai_qcom_d1dns_weaker(&_scratch->candidates[weaker],
                                              &_scratch->candidates[parent])) {
            return;
        }
        vqec_vision_ai_qcom_d1dns_swap_candidate(&_scratch->candidates[parent],
                                                 &_scratch->candidates[weaker]);
        parent = weaker;
    }
}

static void
vqec_vision_ai_qcom_d1dns_write_record(uint8_t* _output,
                                       const vqec_vision_ai_dsp_v1_dense_candidate* _candidate) {
    vqec_vision_ai_qcom_d1dns_write_f32(_output, _candidate->x1);
    vqec_vision_ai_qcom_d1dns_write_f32(_output + 4U, _candidate->y1);
    vqec_vision_ai_qcom_d1dns_write_f32(_output + 8U, _candidate->x2);
    vqec_vision_ai_qcom_d1dns_write_f32(_output + 12U, _candidate->y2);
    vqec_vision_ai_qcom_d1dns_write_f32(_output + 16U, _candidate->score);
    vqec_vision_ai_qcom_d1dns_write_u32(_output + 20U, _candidate->class_index);
}

vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_qcom_d1dns_execute(
    const vqec_vision_ai_dsp_v1_capabilities* _capabilities, const uint8_t* _descriptor,
    size_t _descriptor_bytes, const uint8_t* _input, size_t _input_bytes, uint8_t* _output,
    size_t _output_capacity_bytes, vqec_vision_ai_dsp_v1_dense_scratch* _scratch,
    vqec_vision_ai_dsp_v1_dense_result* _result) {
    if (_input == NULL || _output == NULL || _scratch == NULL || _result == NULL) {
        return vqec_vision_ai_dsp_v1_wire_malformed;
    }
    vqec_vision_ai_dsp_v1_request request;
    const vqec_vision_ai_dsp_v1_wire_status envelope =
        vqec_vision_ai_qcom_dvwir_validate_request(_capabilities, _descriptor, _descriptor_bytes,
                                                   _input_bytes, _output_capacity_bytes, &request);
    if (envelope != vqec_vision_ai_dsp_v1_wire_ok) {
        return envelope;
    }
    if (request.operation != VQEC_VISION_AI_DSP_V1_DENSE_DECODE) {
        return vqec_vision_ai_dsp_v1_wire_unsupported;
    }
    vqec_vision_ai_dsp_v1_dense_config descriptor;
    const vqec_vision_ai_dsp_v1_wire_status decoded = vqec_vision_ai_qcom_d1dns_decode_descriptor(
        _descriptor, _descriptor_bytes, _input_bytes, _output_capacity_bytes, &descriptor);
    if (decoded != vqec_vision_ai_dsp_v1_wire_ok) {
        return decoded;
    }

    uint32_t candidate_count = 0;
    uint32_t truncated = 0;
    for (uint32_t prediction = 0; prediction < descriptor.prediction_count; ++prediction) {
        for (uint32_t class_index = 0; class_index < descriptor.class_count; ++class_index) {
            const size_t score_index =
                (size_t)descriptor.score_offset +
                ((size_t)class_index * descriptor.prediction_count + prediction) * sizeof(uint16_t);
            const float score = vqec_vision_ai_qcom_d1dns_dequantize(
                vqec_vision_ai_qcom_d1dns_read_u16(_input + score_index), descriptor.score_scale,
                descriptor.score_zero_point);
            if (!isfinite(score) || score < 0.0F || score > 1.0F) {
                return vqec_vision_ai_dsp_v1_wire_out_of_range;
            }
            if (!(score >= descriptor.confidence_threshold)) {
                continue;
            }
            float box_values[4];
            for (uint32_t channel = 0; channel < 4U; ++channel) {
                const size_t box_index =
                    (size_t)descriptor.box_offset +
                    ((size_t)channel * descriptor.prediction_count + prediction) * sizeof(uint16_t);
                box_values[channel] = vqec_vision_ai_qcom_d1dns_dequantize(
                    vqec_vision_ai_qcom_d1dns_read_u16(_input + box_index), descriptor.box_scale,
                    descriptor.box_zero_point);
            }
            if (!isfinite(box_values[0]) || !isfinite(box_values[1]) || !isfinite(box_values[2]) ||
                !isfinite(box_values[3])) {
                return vqec_vision_ai_dsp_v1_wire_out_of_range;
            }
            vqec_vision_ai_dsp_v1_dense_candidate candidate;
            vqec_vision_ai_qcom_d1dns_store_candidate(&candidate, box_values[0], box_values[1],
                                                      box_values[2], box_values[3], score,
                                                      class_index, prediction, &descriptor);
            if (!isfinite(candidate.x1) || !isfinite(candidate.y1) || !isfinite(candidate.x2) ||
                !isfinite(candidate.y2) || !isfinite(candidate.score) ||
                candidate.x2 <= candidate.x1 || candidate.y2 <= candidate.y1) {
                continue;
            }
            if (candidate_count < descriptor.candidate_capacity) {
                _scratch->candidates[candidate_count] = candidate;
                vqec_vision_ai_qcom_d1dns_heap_insert(_scratch, candidate_count);
                ++candidate_count;
            } else {
                ++truncated;
                if (score <= _scratch->candidates[0].score) {
                    continue;
                }
                vqec_vision_ai_qcom_d1dns_heap_replace_root(_scratch, candidate_count, &candidate);
            }
        }
    }

    for (uint32_t index = 0; index < candidate_count; ++index) {
        _scratch->order[index] = (uint16_t)index;
    }
    for (uint32_t index = 1; index < candidate_count; ++index) {
        const uint16_t value = _scratch->order[index];
        uint32_t insertion = index;
        while (insertion > 0 && vqec_vision_ai_qcom_d1dns_precedes(
                                    &_scratch->candidates[value],
                                    &_scratch->candidates[_scratch->order[insertion - 1U]])) {
            _scratch->order[insertion] = _scratch->order[insertion - 1U];
            --insertion;
        }
        _scratch->order[insertion] = value;
    }

    uint32_t kept_count = 0;
    for (uint32_t index = 0; index < candidate_count && kept_count < descriptor.output_capacity;
         ++index) {
        const uint16_t candidate_index = _scratch->order[index];
        const vqec_vision_ai_dsp_v1_dense_candidate* candidate =
            &_scratch->candidates[candidate_index];
        int suppressed = 0;
        for (uint32_t kept = 0; kept < kept_count && !suppressed; ++kept) {
            const vqec_vision_ai_dsp_v1_dense_candidate* selected =
                &_scratch->candidates[_scratch->kept[kept]];
            const int comparable =
                (descriptor.flags & VQEC_VISION_AI_DSP_V1_DENSE_CLASS_AWARE_NMS) == 0 ||
                selected->class_index == candidate->class_index;
            if (comparable &&
                vqec_vision_ai_qcom_d1dns_iou(candidate, selected) > descriptor.iou_threshold) {
                suppressed = 1;
            }
        }
        if (!suppressed) {
            _scratch->kept[kept_count++] = candidate_index;
        }
    }

    for (uint32_t index = 0; index < kept_count; ++index) {
        vqec_vision_ai_qcom_d1dns_write_record(
            _output + (size_t)index * VQEC_VISION_AI_DSP_V1_DENSE_RECORD_BYTES,
            &_scratch->candidates[_scratch->kept[index]]);
    }
    _result->output_count = kept_count;
    _result->truncated_candidates = truncated;
    _result->output_bytes = kept_count * VQEC_VISION_AI_DSP_V1_DENSE_RECORD_BYTES;
    return vqec_vision_ai_dsp_v1_wire_ok;
}
