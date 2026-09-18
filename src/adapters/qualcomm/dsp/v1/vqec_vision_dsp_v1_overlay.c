#include "vqec_vision_dsp_v1_overlay.h"

#include <limits.h>
#include <string.h>

#define VQEC_VISION_AI_DSP_V1_OVERLAY_PAYLOAD_MAJOR_OFFSET 32U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_PAYLOAD_MINOR_OFFSET 34U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_PIXEL_FORMAT_OFFSET 36U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_MATRIX_OFFSET 38U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_RANGE_OFFSET 40U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_RESERVED_42_OFFSET 42U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_WIDTH_OFFSET 44U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_HEIGHT_OFFSET 48U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_SOURCE_Y_OFFSET 52U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_SOURCE_Y_STRIDE_OFFSET 56U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_SOURCE_UV_OFFSET 60U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_SOURCE_UV_STRIDE_OFFSET 64U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_DESTINATION_Y_OFFSET 68U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_DESTINATION_Y_STRIDE_OFFSET 72U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_DESTINATION_UV_OFFSET 76U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_DESTINATION_UV_STRIDE_OFFSET 80U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_COUNT_OFFSET 84U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_BOXES_OFFSET_OFFSET 88U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_LABELS_OFFSET_OFFSET 92U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_LABEL_BYTES_OFFSET 96U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_BORDER_OFFSET 100U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_FONT_SCALE_OFFSET 102U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_RESERVED_OFFSET 104U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_RESERVED_BYTES 8U

#define VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_X_OFFSET 0U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_Y_OFFSET 4U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_WIDTH_OFFSET 8U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_HEIGHT_OFFSET 12U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_COLOR_Y_OFFSET 16U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_COLOR_U_OFFSET 17U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_COLOR_V_OFFSET 18U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_RESERVED_19_OFFSET 19U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_LABEL_OFFSET_OFFSET 20U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_LABEL_BYTES_OFFSET 24U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_RESERVED_OFFSET 26U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_RESERVED_BYTES 6U

#define VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_BORDER 8U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_FONT_SCALE 4U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_FONT_WIDTH 5U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_FONT_HEIGHT 7U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_FONT_ADVANCE 6U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_TEXT_DARK_Y 16U
#define VQEC_VISION_AI_DSP_V1_OVERLAY_TEXT_LIGHT_Y 235U

/* Project-owned compact 5x7 glyphs. Rows are bits 4..0, indexed 0..9 then A..Z. */
static const uint8_t g_vqec_vision_ai_dsp_v1_overlay_font[36U][7U] = {
    {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},
    {14, 17, 1, 2, 4, 8, 31}, {30, 1, 1, 14, 1, 1, 30},
    {2, 6, 10, 18, 31, 2, 2}, {31, 16, 16, 30, 1, 1, 30},
    {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},
    {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14},
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30},
    {14, 17, 16, 16, 16, 17, 14}, {30, 17, 17, 17, 17, 17, 30},
    {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17},
    {14, 4, 4, 4, 4, 4, 14}, {1, 1, 1, 1, 17, 17, 14},
    {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17},
    {14, 17, 17, 17, 17, 17, 14}, {30, 17, 17, 30, 16, 16, 16},
    {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30}, {31, 4, 4, 4, 4, 4, 4},
    {17, 17, 17, 17, 17, 17, 14}, {17, 17, 17, 17, 17, 10, 4},
    {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4}, {31, 1, 2, 4, 8, 16, 31}};
static const uint8_t g_vqec_vision_ai_dsp_v1_overlay_unknown_glyph[7U] = {
    14, 17, 1, 2, 4, 0, 4};

static uint16_t vqec_vision_ai_qcom_d1ovr_read_u16(const uint8_t* _bytes) {
    return (uint16_t)((uint16_t)_bytes[0] | ((uint16_t)_bytes[1] << 8));
}

static uint32_t vqec_vision_ai_qcom_d1ovr_read_u32(const uint8_t* _bytes) {
    return (uint32_t)_bytes[0] | ((uint32_t)_bytes[1] << 8) | ((uint32_t)_bytes[2] << 16) |
           ((uint32_t)_bytes[3] << 24);
}

static void vqec_vision_ai_qcom_d1ovr_write_u16(uint8_t* _bytes, uint16_t _value) {
    _bytes[0] = (uint8_t)_value;
    _bytes[1] = (uint8_t)(_value >> 8);
}

static void vqec_vision_ai_qcom_d1ovr_write_u32(uint8_t* _bytes, uint32_t _value) {
    _bytes[0] = (uint8_t)_value;
    _bytes[1] = (uint8_t)(_value >> 8);
    _bytes[2] = (uint8_t)(_value >> 16);
    _bytes[3] = (uint8_t)(_value >> 24);
}

static int vqec_vision_ai_qcom_d1ovr_add_u32(uint32_t _left, uint32_t _right,
                                             uint32_t* _sum) {
    if (_sum == NULL || _right > UINT32_MAX - _left) {
        return 0;
    }
    *_sum = _left + _right;
    return 1;
}

static int vqec_vision_ai_qcom_d1ovr_surface_valid(uint32_t _width, uint32_t _height,
                                                   uint32_t _y_offset, uint32_t _y_stride,
                                                   uint32_t _uv_offset, uint32_t _uv_stride,
                                                   size_t _bytes, uint32_t* _required) {
    uint64_t y_end;
    uint64_t uv_end;
    if (_width == 0U || _height == 0U || (_width & 1U) != 0U || (_height & 1U) != 0U ||
        _y_stride < _width || _uv_stride < _width) {
        return 0;
    }
    y_end = (uint64_t)_y_offset + (uint64_t)_y_stride * (uint64_t)_height;
    uv_end = (uint64_t)_uv_offset + (uint64_t)_uv_stride * (uint64_t)(_height / 2U);
    if (_uv_offset < y_end || y_end > _bytes || uv_end > _bytes || uv_end > UINT32_MAX) {
        return 0;
    }
    if (_required != NULL) {
        *_required = (uint32_t)uv_end;
    }
    return 1;
}

static const uint8_t* vqec_vision_ai_qcom_d1ovr_glyph(uint8_t _character) {
    uint8_t normalized = _character;
    if (normalized >= (uint8_t)'a' && normalized <= (uint8_t)'z') {
        normalized = (uint8_t)(normalized - (uint8_t)'a' + (uint8_t)'A');
    }
    if (normalized >= (uint8_t)'0' && normalized <= (uint8_t)'9') {
        return g_vqec_vision_ai_dsp_v1_overlay_font[normalized - (uint8_t)'0'];
    }
    if (normalized >= (uint8_t)'A' && normalized <= (uint8_t)'Z') {
        return g_vqec_vision_ai_dsp_v1_overlay_font[10U + normalized - (uint8_t)'A'];
    }
    if (normalized == (uint8_t)' ') {
        return NULL;
    }
    return g_vqec_vision_ai_dsp_v1_overlay_unknown_glyph;
}

static void vqec_vision_ai_qcom_d1ovr_put_y(uint8_t* _plane, uint32_t _stride,
                                            uint32_t _width, uint32_t _height, int32_t _x,
                                            int32_t _y, uint8_t _value) {
    if (_x >= 0 && _y >= 0 && (uint32_t)_x < _width && (uint32_t)_y < _height) {
        _plane[(size_t)(uint32_t)_y * _stride + (uint32_t)_x] = _value;
    }
}

static void vqec_vision_ai_qcom_d1ovr_draw_glyph(uint8_t* _plane, uint32_t _stride,
                                                  uint32_t _width, uint32_t _height,
                                                  int32_t _origin_x, int32_t _origin_y,
                                                  uint8_t _character, uint16_t _scale,
                                                  uint8_t _value) {
    const uint8_t* glyph = vqec_vision_ai_qcom_d1ovr_glyph(_character);
    uint32_t row;
    if (glyph == NULL) {
        return;
    }
    for (row = 0U; row < VQEC_VISION_AI_DSP_V1_OVERLAY_FONT_HEIGHT; ++row) {
        uint32_t column;
        for (column = 0U; column < VQEC_VISION_AI_DSP_V1_OVERLAY_FONT_WIDTH; ++column) {
            uint32_t scale_y;
            if ((glyph[row] & (uint8_t)(1U << (4U - column))) == 0U) {
                continue;
            }
            for (scale_y = 0U; scale_y < _scale; ++scale_y) {
                uint32_t scale_x;
                for (scale_x = 0U; scale_x < _scale; ++scale_x) {
                    vqec_vision_ai_qcom_d1ovr_put_y(
                        _plane, _stride, _width, _height,
                        _origin_x + (int32_t)(column * _scale + scale_x),
                        _origin_y + (int32_t)(row * _scale + scale_y), _value);
                }
            }
        }
    }
}

static void vqec_vision_ai_qcom_d1ovr_draw_label(uint8_t* _plane, uint32_t _stride,
                                                  uint32_t _width, uint32_t _height,
                                                  int32_t _x, int32_t _y,
                                                  const uint8_t* _label,
                                                  uint16_t _label_bytes, uint16_t _scale) {
    uint16_t index;
    for (index = 0U; index < _label_bytes; ++index) {
        const int32_t glyph_x =
            _x + (int32_t)((uint32_t)index * VQEC_VISION_AI_DSP_V1_OVERLAY_FONT_ADVANCE * _scale);
        vqec_vision_ai_qcom_d1ovr_draw_glyph(_plane, _stride, _width, _height,
                                              glyph_x + 1, _y + 1, _label[index], _scale,
                                              VQEC_VISION_AI_DSP_V1_OVERLAY_TEXT_DARK_Y);
        vqec_vision_ai_qcom_d1ovr_draw_glyph(_plane, _stride, _width, _height,
                                              glyph_x, _y, _label[index], _scale,
                                              VQEC_VISION_AI_DSP_V1_OVERLAY_TEXT_LIGHT_Y);
    }
}

static void vqec_vision_ai_qcom_d1ovr_draw_box(uint8_t* _y_plane, uint8_t* _uv_plane,
                                                uint32_t _y_stride, uint32_t _uv_stride,
                                                uint32_t _frame_width, uint32_t _frame_height,
                                                uint32_t _x, uint32_t _y, uint32_t _width,
                                                uint32_t _height, uint16_t _thickness,
                                                uint8_t _color_y, uint8_t _color_u,
                                                uint8_t _color_v) {
    uint32_t x2 = _x + _width;
    uint32_t y2 = _y + _height;
    uint32_t offset;
    for (offset = 0U; offset < _thickness; ++offset) {
        uint32_t column;
        uint32_t row;
        for (column = _x; column < x2; ++column) {
            _y_plane[(size_t)(_y + offset) * _y_stride + column] = _color_y;
            _y_plane[(size_t)(y2 - 1U - offset) * _y_stride + column] = _color_y;
        }
        for (row = _y; row < y2; ++row) {
            _y_plane[(size_t)row * _y_stride + _x + offset] = _color_y;
            _y_plane[(size_t)row * _y_stride + x2 - 1U - offset] = _color_y;
        }
    }
    {
        const uint32_t uv_x1 = _x / 2U;
        const uint32_t uv_y1 = _y / 2U;
        const uint32_t uv_x2 = (x2 + 1U) / 2U;
        const uint32_t uv_y2 = (y2 + 1U) / 2U;
        uint32_t uv_x;
        uint32_t uv_y;
        (void)_frame_width;
        (void)_frame_height;
        for (uv_x = uv_x1; uv_x < uv_x2; ++uv_x) {
            const size_t top = (size_t)uv_y1 * _uv_stride + uv_x * 2U;
            const size_t bottom = (size_t)(uv_y2 - 1U) * _uv_stride + uv_x * 2U;
            _uv_plane[top] = _color_u;
            _uv_plane[top + 1U] = _color_v;
            _uv_plane[bottom] = _color_u;
            _uv_plane[bottom + 1U] = _color_v;
        }
        for (uv_y = uv_y1; uv_y < uv_y2; ++uv_y) {
            const size_t left = (size_t)uv_y * _uv_stride + uv_x1 * 2U;
            const size_t right = (size_t)uv_y * _uv_stride + (uv_x2 - 1U) * 2U;
            _uv_plane[left] = _color_u;
            _uv_plane[left + 1U] = _color_v;
            _uv_plane[right] = _color_u;
            _uv_plane[right + 1U] = _color_v;
        }
    }
}

vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_qcom_d1ovr_encode_descriptor(
    const vqec_vision_ai_dsp_v1_capabilities* _capabilities,
    const vqec_vision_ai_dsp_v1_overlay_frame* _frame,
    const vqec_vision_ai_dsp_v1_overlay_box* _boxes, uint32_t _box_count,
    const uint8_t* _labels, uint32_t _label_bytes, uint32_t _input_bytes,
    uint32_t _output_bytes, uint8_t* _descriptor, size_t _descriptor_capacity,
    size_t* _descriptor_bytes) {
    uint32_t boxes_bytes;
    uint32_t labels_offset;
    uint32_t total_bytes;
    uint32_t index;
    vqec_vision_ai_dsp_v1_request request;
    if (_capabilities == NULL || _frame == NULL || _descriptor == NULL ||
        _descriptor_bytes == NULL || (_box_count != 0U && _boxes == NULL) ||
        (_label_bytes != 0U && _labels == NULL) ||
        _box_count > VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_BOXES ||
        _label_bytes > VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_LABEL_BYTES ||
        _frame->border_thickness == 0U ||
        _frame->border_thickness > VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_BORDER ||
        _frame->font_scale == 0U || _frame->font_scale > VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_FONT_SCALE ||
        _input_bytes == 0U || _output_bytes == 0U) {
        return vqec_vision_ai_dsp_v1_wire_malformed;
    }
    boxes_bytes = _box_count * VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_BYTES;
    for (index = 0U; index < _label_bytes; ++index) {
        if (_labels[index] < 32U || _labels[index] > 126U) {
            return vqec_vision_ai_dsp_v1_wire_malformed;
        }
    }
    if (!vqec_vision_ai_qcom_d1ovr_add_u32(VQEC_VISION_AI_DSP_V1_OVERLAY_HEADER_BYTES,
                                           boxes_bytes, &labels_offset) ||
        !vqec_vision_ai_qcom_d1ovr_add_u32(labels_offset, _label_bytes, &total_bytes) ||
        total_bytes > _descriptor_capacity || total_bytes > _capabilities->max_descriptor_bytes ||
        (_capabilities->operations_mask &
         (1U << (VQEC_VISION_AI_DSP_V1_OVERLAY_COMPOSE - 1U))) == 0U) {
        return vqec_vision_ai_dsp_v1_wire_out_of_range;
    }
    memset(_descriptor, 0, total_bytes);
    request.operation = VQEC_VISION_AI_DSP_V1_OVERLAY_COMPOSE;
    request.descriptor_bytes = total_bytes;
    request.input_bytes = _input_bytes;
    request.output_capacity_bytes = _output_bytes;
    request.domain_generation = _capabilities->domain_generation;
    if (vqec_vision_ai_qcom_dvwir_encode_request(&request, _descriptor, total_bytes) !=
        vqec_vision_ai_dsp_v1_wire_ok) {
        return vqec_vision_ai_dsp_v1_wire_malformed;
    }
    vqec_vision_ai_qcom_d1ovr_write_u16(
        _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_PAYLOAD_MAJOR_OFFSET,
        VQEC_VISION_AI_BASELINE_ABI_MAJOR);
    vqec_vision_ai_qcom_d1ovr_write_u16(
        _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_PAYLOAD_MINOR_OFFSET,
        VQEC_VISION_AI_BASELINE_ABI_MINOR);
    vqec_vision_ai_qcom_d1ovr_write_u16(
        _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_PIXEL_FORMAT_OFFSET,
        VQEC_VISION_AI_DSP_V1_OVERLAY_PIXEL_NV12);
    vqec_vision_ai_qcom_d1ovr_write_u16(
        _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_MATRIX_OFFSET,
        VQEC_VISION_AI_DSP_V1_OVERLAY_MATRIX_BT709);
    vqec_vision_ai_qcom_d1ovr_write_u16(
        _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_RANGE_OFFSET,
        VQEC_VISION_AI_DSP_V1_OVERLAY_RANGE_LIMITED);
    vqec_vision_ai_qcom_d1ovr_write_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_WIDTH_OFFSET,
                                        _frame->width);
    vqec_vision_ai_qcom_d1ovr_write_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_HEIGHT_OFFSET,
                                        _frame->height);
    vqec_vision_ai_qcom_d1ovr_write_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_SOURCE_Y_OFFSET,
                                        _frame->source_y_offset);
    vqec_vision_ai_qcom_d1ovr_write_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_SOURCE_Y_STRIDE_OFFSET,
                                        _frame->source_y_stride);
    vqec_vision_ai_qcom_d1ovr_write_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_SOURCE_UV_OFFSET,
                                        _frame->source_uv_offset);
    vqec_vision_ai_qcom_d1ovr_write_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_SOURCE_UV_STRIDE_OFFSET,
                                        _frame->source_uv_stride);
    vqec_vision_ai_qcom_d1ovr_write_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_DESTINATION_Y_OFFSET,
                                        _frame->destination_y_offset);
    vqec_vision_ai_qcom_d1ovr_write_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_DESTINATION_Y_STRIDE_OFFSET,
                                        _frame->destination_y_stride);
    vqec_vision_ai_qcom_d1ovr_write_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_DESTINATION_UV_OFFSET,
                                        _frame->destination_uv_offset);
    vqec_vision_ai_qcom_d1ovr_write_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_DESTINATION_UV_STRIDE_OFFSET,
                                        _frame->destination_uv_stride);
    vqec_vision_ai_qcom_d1ovr_write_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_COUNT_OFFSET,
                                        _box_count);
    vqec_vision_ai_qcom_d1ovr_write_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_BOXES_OFFSET_OFFSET,
                                        VQEC_VISION_AI_DSP_V1_OVERLAY_HEADER_BYTES);
    vqec_vision_ai_qcom_d1ovr_write_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_LABELS_OFFSET_OFFSET,
                                        labels_offset);
    vqec_vision_ai_qcom_d1ovr_write_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_LABEL_BYTES_OFFSET,
                                        _label_bytes);
    vqec_vision_ai_qcom_d1ovr_write_u16(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_BORDER_OFFSET,
                                        _frame->border_thickness);
    vqec_vision_ai_qcom_d1ovr_write_u16(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_FONT_SCALE_OFFSET,
                                        _frame->font_scale);
    for (index = 0U; index < _box_count; ++index) {
        uint8_t* box = _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_HEADER_BYTES +
                       index * VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_BYTES;
        if (_boxes[index].label_offset > _label_bytes ||
            _boxes[index].label_bytes > _label_bytes - _boxes[index].label_offset) {
            return vqec_vision_ai_dsp_v1_wire_out_of_range;
        }
        vqec_vision_ai_qcom_d1ovr_write_u32(box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_X_OFFSET,
                                            _boxes[index].x);
        vqec_vision_ai_qcom_d1ovr_write_u32(box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_Y_OFFSET,
                                            _boxes[index].y);
        vqec_vision_ai_qcom_d1ovr_write_u32(box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_WIDTH_OFFSET,
                                            _boxes[index].width);
        vqec_vision_ai_qcom_d1ovr_write_u32(box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_HEIGHT_OFFSET,
                                            _boxes[index].height);
        box[VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_COLOR_Y_OFFSET] = _boxes[index].color_y;
        box[VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_COLOR_U_OFFSET] = _boxes[index].color_u;
        box[VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_COLOR_V_OFFSET] = _boxes[index].color_v;
        vqec_vision_ai_qcom_d1ovr_write_u32(box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_LABEL_OFFSET_OFFSET,
                                            _boxes[index].label_offset);
        vqec_vision_ai_qcom_d1ovr_write_u16(box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_LABEL_BYTES_OFFSET,
                                            _boxes[index].label_bytes);
    }
    if (_label_bytes != 0U) {
        memcpy(_descriptor + labels_offset, _labels, _label_bytes);
    }
    *_descriptor_bytes = total_bytes;
    return vqec_vision_ai_dsp_v1_wire_ok;
}

vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_qcom_d1ovr_execute(
    const vqec_vision_ai_dsp_v1_capabilities* _capabilities, const uint8_t* _descriptor,
    size_t _descriptor_bytes, const uint8_t* _input, size_t _input_bytes, uint8_t* _output,
    size_t _output_bytes, uint32_t* _written_bytes) {
    vqec_vision_ai_dsp_v1_request request;
    uint32_t width;
    uint32_t height;
    uint32_t source_required;
    uint32_t destination_required;
    uint32_t box_count;
    uint32_t boxes_offset;
    uint32_t labels_offset;
    uint32_t label_bytes;
    uint16_t border;
    uint16_t font_scale;
    uint32_t boxes_end;
    uint32_t labels_end;
    uint32_t index;
    if (_capabilities == NULL || _descriptor == NULL || _input == NULL || _output == NULL ||
        _written_bytes == NULL) {
        return vqec_vision_ai_dsp_v1_wire_malformed;
    }
    if (vqec_vision_ai_qcom_dvwir_validate_request(
            _capabilities, _descriptor, _descriptor_bytes, _input_bytes, _output_bytes,
            &request) != vqec_vision_ai_dsp_v1_wire_ok ||
        request.operation != VQEC_VISION_AI_DSP_V1_OVERLAY_COMPOSE ||
        _descriptor_bytes < VQEC_VISION_AI_DSP_V1_OVERLAY_HEADER_BYTES) {
        return vqec_vision_ai_dsp_v1_wire_malformed;
    }
    if (vqec_vision_ai_qcom_d1ovr_read_u16(
            _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_PAYLOAD_MAJOR_OFFSET) !=
            VQEC_VISION_AI_BASELINE_ABI_MAJOR ||
        vqec_vision_ai_qcom_d1ovr_read_u16(
            _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_PAYLOAD_MINOR_OFFSET) !=
            VQEC_VISION_AI_BASELINE_ABI_MINOR ||
        vqec_vision_ai_qcom_d1ovr_read_u16(
            _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_PIXEL_FORMAT_OFFSET) !=
            VQEC_VISION_AI_DSP_V1_OVERLAY_PIXEL_NV12 ||
        vqec_vision_ai_qcom_d1ovr_read_u16(
            _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_MATRIX_OFFSET) !=
            VQEC_VISION_AI_DSP_V1_OVERLAY_MATRIX_BT709 ||
        vqec_vision_ai_qcom_d1ovr_read_u16(
            _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_RANGE_OFFSET) !=
            VQEC_VISION_AI_DSP_V1_OVERLAY_RANGE_LIMITED) {
        return vqec_vision_ai_dsp_v1_wire_unsupported;
    }
    if (vqec_vision_ai_qcom_d1ovr_read_u16(
            _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_RESERVED_42_OFFSET) != 0U) {
        return vqec_vision_ai_dsp_v1_wire_malformed;
    }
    for (index = 0U; index < VQEC_VISION_AI_DSP_V1_OVERLAY_RESERVED_BYTES; ++index) {
        if (_descriptor[VQEC_VISION_AI_DSP_V1_OVERLAY_RESERVED_OFFSET + index] != 0U) {
            return vqec_vision_ai_dsp_v1_wire_malformed;
        }
    }
    width = vqec_vision_ai_qcom_d1ovr_read_u32(
        _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_WIDTH_OFFSET);
    height = vqec_vision_ai_qcom_d1ovr_read_u32(
        _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_HEIGHT_OFFSET);
    if (!vqec_vision_ai_qcom_d1ovr_surface_valid(
            width, height,
            vqec_vision_ai_qcom_d1ovr_read_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_SOURCE_Y_OFFSET),
            vqec_vision_ai_qcom_d1ovr_read_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_SOURCE_Y_STRIDE_OFFSET),
            vqec_vision_ai_qcom_d1ovr_read_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_SOURCE_UV_OFFSET),
            vqec_vision_ai_qcom_d1ovr_read_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_SOURCE_UV_STRIDE_OFFSET),
            _input_bytes, &source_required) ||
        !vqec_vision_ai_qcom_d1ovr_surface_valid(
            width, height,
            vqec_vision_ai_qcom_d1ovr_read_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_DESTINATION_Y_OFFSET),
            vqec_vision_ai_qcom_d1ovr_read_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_DESTINATION_Y_STRIDE_OFFSET),
            vqec_vision_ai_qcom_d1ovr_read_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_DESTINATION_UV_OFFSET),
            vqec_vision_ai_qcom_d1ovr_read_u32(_descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_DESTINATION_UV_STRIDE_OFFSET),
            _output_bytes, &destination_required)) {
        return vqec_vision_ai_dsp_v1_wire_out_of_range;
    }
    (void)source_required;
    box_count = vqec_vision_ai_qcom_d1ovr_read_u32(
        _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_COUNT_OFFSET);
    boxes_offset = vqec_vision_ai_qcom_d1ovr_read_u32(
        _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_BOXES_OFFSET_OFFSET);
    labels_offset = vqec_vision_ai_qcom_d1ovr_read_u32(
        _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_LABELS_OFFSET_OFFSET);
    label_bytes = vqec_vision_ai_qcom_d1ovr_read_u32(
        _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_LABEL_BYTES_OFFSET);
    border = vqec_vision_ai_qcom_d1ovr_read_u16(
        _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_BORDER_OFFSET);
    font_scale = vqec_vision_ai_qcom_d1ovr_read_u16(
        _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_FONT_SCALE_OFFSET);
    if (box_count > VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_BOXES ||
        label_bytes > VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_LABEL_BYTES ||
        boxes_offset != VQEC_VISION_AI_DSP_V1_OVERLAY_HEADER_BYTES ||
        border == 0U || border > VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_BORDER ||
        font_scale == 0U || font_scale > VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_FONT_SCALE ||
        !vqec_vision_ai_qcom_d1ovr_add_u32(
            boxes_offset, box_count * VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_BYTES, &boxes_end) ||
        labels_offset != boxes_end ||
        !vqec_vision_ai_qcom_d1ovr_add_u32(labels_offset, label_bytes, &labels_end) ||
        labels_end != _descriptor_bytes) {
        return vqec_vision_ai_dsp_v1_wire_malformed;
    }
    for (index = 0U; index < box_count; ++index) {
        const uint8_t* box = _descriptor + boxes_offset +
                             index * VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_BYTES;
        uint32_t x;
        uint32_t y;
        uint32_t box_width;
        uint32_t box_height;
        uint32_t x2;
        uint32_t y2;
        uint32_t box_label_offset;
        uint16_t box_label_bytes;
        uint32_t reserved;
        if (box[VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_RESERVED_19_OFFSET] != 0U) {
            return vqec_vision_ai_dsp_v1_wire_malformed;
        }
        for (reserved = 0U; reserved < VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_RESERVED_BYTES;
             ++reserved) {
            if (box[VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_RESERVED_OFFSET + reserved] != 0U) {
                return vqec_vision_ai_dsp_v1_wire_malformed;
            }
        }
        x = vqec_vision_ai_qcom_d1ovr_read_u32(box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_X_OFFSET);
        y = vqec_vision_ai_qcom_d1ovr_read_u32(box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_Y_OFFSET);
        box_width = vqec_vision_ai_qcom_d1ovr_read_u32(box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_WIDTH_OFFSET);
        box_height = vqec_vision_ai_qcom_d1ovr_read_u32(box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_HEIGHT_OFFSET);
        box_label_offset = vqec_vision_ai_qcom_d1ovr_read_u32(
            box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_LABEL_OFFSET_OFFSET);
        box_label_bytes = vqec_vision_ai_qcom_d1ovr_read_u16(
            box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_LABEL_BYTES_OFFSET);
        if (box_width == 0U || box_height == 0U ||
            !vqec_vision_ai_qcom_d1ovr_add_u32(x, box_width, &x2) ||
            !vqec_vision_ai_qcom_d1ovr_add_u32(y, box_height, &y2) ||
            x2 > width || y2 > height || box_width <= border * 2U ||
            box_height <= border * 2U || box_label_offset > label_bytes ||
            box_label_bytes > label_bytes - box_label_offset) {
            return vqec_vision_ai_dsp_v1_wire_out_of_range;
        }
        for (reserved = 0U; reserved < box_label_bytes; ++reserved) {
            const uint8_t character = _descriptor[labels_offset + box_label_offset + reserved];
            if (character < 32U || character > 126U) {
                return vqec_vision_ai_dsp_v1_wire_malformed;
            }
        }
    }
    {
        const uint32_t source_y_offset = vqec_vision_ai_qcom_d1ovr_read_u32(
            _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_SOURCE_Y_OFFSET);
        const uint32_t source_y_stride = vqec_vision_ai_qcom_d1ovr_read_u32(
            _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_SOURCE_Y_STRIDE_OFFSET);
        const uint32_t source_uv_offset = vqec_vision_ai_qcom_d1ovr_read_u32(
            _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_SOURCE_UV_OFFSET);
        const uint32_t source_uv_stride = vqec_vision_ai_qcom_d1ovr_read_u32(
            _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_SOURCE_UV_STRIDE_OFFSET);
        const uint32_t destination_y_offset = vqec_vision_ai_qcom_d1ovr_read_u32(
            _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_DESTINATION_Y_OFFSET);
        const uint32_t destination_y_stride = vqec_vision_ai_qcom_d1ovr_read_u32(
            _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_DESTINATION_Y_STRIDE_OFFSET);
        const uint32_t destination_uv_offset = vqec_vision_ai_qcom_d1ovr_read_u32(
            _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_DESTINATION_UV_OFFSET);
        const uint32_t destination_uv_stride = vqec_vision_ai_qcom_d1ovr_read_u32(
            _descriptor + VQEC_VISION_AI_DSP_V1_OVERLAY_DESTINATION_UV_STRIDE_OFFSET);
        uint32_t row;
        for (row = 0U; row < height; ++row) {
            memcpy(_output + destination_y_offset + (size_t)row * destination_y_stride,
                   _input + source_y_offset + (size_t)row * source_y_stride, width);
        }
        for (row = 0U; row < height / 2U; ++row) {
            memcpy(_output + destination_uv_offset + (size_t)row * destination_uv_stride,
                   _input + source_uv_offset + (size_t)row * source_uv_stride, width);
        }
        for (index = 0U; index < box_count; ++index) {
            const uint8_t* box = _descriptor + boxes_offset +
                                 index * VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_BYTES;
            const uint32_t x = vqec_vision_ai_qcom_d1ovr_read_u32(
                box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_X_OFFSET);
            const uint32_t y = vqec_vision_ai_qcom_d1ovr_read_u32(
                box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_Y_OFFSET);
            const uint32_t box_width = vqec_vision_ai_qcom_d1ovr_read_u32(
                box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_WIDTH_OFFSET);
            const uint32_t box_height = vqec_vision_ai_qcom_d1ovr_read_u32(
                box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_HEIGHT_OFFSET);
            const uint32_t box_label_offset = vqec_vision_ai_qcom_d1ovr_read_u32(
                box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_LABEL_OFFSET_OFFSET);
            const uint16_t box_label_bytes = vqec_vision_ai_qcom_d1ovr_read_u16(
                box + VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_LABEL_BYTES_OFFSET);
            const int32_t text_height =
                (int32_t)(VQEC_VISION_AI_DSP_V1_OVERLAY_FONT_HEIGHT * font_scale);
            const int32_t text_y = y > (uint32_t)(text_height + 2) ?
                (int32_t)y - text_height - 2 : (int32_t)y + 2;
            vqec_vision_ai_qcom_d1ovr_draw_box(
                _output + destination_y_offset, _output + destination_uv_offset,
                destination_y_stride, destination_uv_stride, width, height, x, y,
                box_width, box_height, border,
                box[VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_COLOR_Y_OFFSET],
                box[VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_COLOR_U_OFFSET],
                box[VQEC_VISION_AI_DSP_V1_OVERLAY_BOX_COLOR_V_OFFSET]);
            vqec_vision_ai_qcom_d1ovr_draw_label(
                _output + destination_y_offset, destination_y_stride, width, height,
                (int32_t)x, text_y,
                _descriptor + labels_offset + box_label_offset, box_label_bytes, font_scale);
        }
    }
    *_written_bytes = destination_required;
    return vqec_vision_ai_dsp_v1_wire_ok;
}
