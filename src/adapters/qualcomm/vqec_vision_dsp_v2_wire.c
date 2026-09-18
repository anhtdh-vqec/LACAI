#include "vqec_vision_dsp_v2_wire.h"

#include <limits.h>
#include <string.h>

static const uint8_t g_vqec_vision_ai_dsp_v2_magic[4] = {'V', 'Q', '2', '!'};

static uint16_t vqec_vision_ai_qcom_dvwir_read_u16(const uint8_t* _bytes) {
    return (uint16_t)((uint16_t)_bytes[0] | ((uint16_t)_bytes[1] << 8));
}

static uint32_t vqec_vision_ai_qcom_dvwir_read_u32(const uint8_t* _bytes) {
    return (uint32_t)_bytes[0] | ((uint32_t)_bytes[1] << 8) | ((uint32_t)_bytes[2] << 16) |
           ((uint32_t)_bytes[3] << 24);
}

static void vqec_vision_ai_qcom_dvwir_write_u16(uint8_t* _bytes, uint16_t _value) {
    _bytes[0] = (uint8_t)_value;
    _bytes[1] = (uint8_t)(_value >> 8);
}

static void vqec_vision_ai_qcom_dvwir_write_u32(uint8_t* _bytes, uint32_t _value) {
    _bytes[0] = (uint8_t)_value;
    _bytes[1] = (uint8_t)(_value >> 8);
    _bytes[2] = (uint8_t)(_value >> 16);
    _bytes[3] = (uint8_t)(_value >> 24);
}

static int vqec_vision_ai_qcom_dvwir_capabilities_valid(
    const vqec_vision_ai_dsp_v2_capabilities* _capabilities) {
    return _capabilities != NULL && _capabilities->operations_mask != 0 &&
           (_capabilities->operations_mask & ~((uint32_t)VQEC_VISION_AI_DSP_V2_ALL_OPERATIONS)) ==
               0 &&
           _capabilities->max_descriptor_bytes >= VQEC_VISION_AI_DSP_V2_ENVELOPE_BYTES &&
           _capabilities->max_input_bytes != 0 && _capabilities->max_output_bytes != 0 &&
           _capabilities->domain_generation != 0;
}

static int vqec_vision_ai_qcom_dvwir_operation_valid(uint16_t _operation) {
    return _operation >= VQEC_VISION_AI_DSP_V2_IMAGE_TRANSFORM &&
           _operation <= VQEC_VISION_AI_DSP_V2_ROI_ALIGN;
}

vqec_vision_ai_dsp_v2_wire_status vqec_vision_ai_qcom_dvwir_encode_capabilities(
    const vqec_vision_ai_dsp_v2_capabilities* _capabilities, uint8_t* _wire, size_t _wire_bytes) {
    if (_wire == NULL || _wire_bytes != VQEC_VISION_AI_DSP_V2_ENVELOPE_BYTES ||
        !vqec_vision_ai_qcom_dvwir_capabilities_valid(_capabilities)) {
        return vqec_vision_ai_dsp_v2_wire_malformed;
    }
    memset(_wire, 0, _wire_bytes);
    memcpy(_wire, g_vqec_vision_ai_dsp_v2_magic, sizeof(g_vqec_vision_ai_dsp_v2_magic));
    vqec_vision_ai_qcom_dvwir_write_u16(_wire + VQEC_VISION_AI_DSP_V2_MAJOR_OFFSET,
                                        VQEC_VISION_AI_DSP_V2_MAJOR);
    vqec_vision_ai_qcom_dvwir_write_u16(_wire + VQEC_VISION_AI_DSP_V2_MINOR_OFFSET,
                                        VQEC_VISION_AI_DSP_V2_MINOR);
    vqec_vision_ai_qcom_dvwir_write_u16(_wire + VQEC_VISION_AI_DSP_V2_HEADER_BYTES_OFFSET,
                                        VQEC_VISION_AI_DSP_V2_ENVELOPE_BYTES);
    vqec_vision_ai_qcom_dvwir_write_u16(_wire + VQEC_VISION_AI_DSP_V2_KIND_OFFSET,
                                        VQEC_VISION_AI_DSP_V2_SYNC_COMPLETION);
    vqec_vision_ai_qcom_dvwir_write_u32(_wire + VQEC_VISION_AI_DSP_V2_FIELD_12_OFFSET,
                                        _capabilities->operations_mask);
    vqec_vision_ai_qcom_dvwir_write_u32(_wire + VQEC_VISION_AI_DSP_V2_FIELD_16_OFFSET,
                                        _capabilities->max_descriptor_bytes);
    vqec_vision_ai_qcom_dvwir_write_u32(_wire + VQEC_VISION_AI_DSP_V2_FIELD_20_OFFSET,
                                        _capabilities->max_input_bytes);
    vqec_vision_ai_qcom_dvwir_write_u32(_wire + VQEC_VISION_AI_DSP_V2_FIELD_24_OFFSET,
                                        _capabilities->max_output_bytes);
    vqec_vision_ai_qcom_dvwir_write_u32(_wire + VQEC_VISION_AI_DSP_V2_FIELD_28_OFFSET,
                                        _capabilities->domain_generation);
    return vqec_vision_ai_dsp_v2_wire_ok;
}

vqec_vision_ai_dsp_v2_wire_status
vqec_vision_ai_qcom_dvwir_decode_capabilities(const uint8_t* _wire, size_t _wire_bytes,
                                              vqec_vision_ai_dsp_v2_capabilities* _capabilities) {
    if (_wire == NULL || _capabilities == NULL ||
        _wire_bytes != VQEC_VISION_AI_DSP_V2_ENVELOPE_BYTES) {
        return vqec_vision_ai_dsp_v2_wire_malformed;
    }
    if (memcmp(_wire, g_vqec_vision_ai_dsp_v2_magic, sizeof(g_vqec_vision_ai_dsp_v2_magic)) != 0 ||
        vqec_vision_ai_qcom_dvwir_read_u16(_wire + VQEC_VISION_AI_DSP_V2_HEADER_BYTES_OFFSET) !=
            VQEC_VISION_AI_DSP_V2_ENVELOPE_BYTES) {
        return vqec_vision_ai_dsp_v2_wire_malformed;
    }
    if (vqec_vision_ai_qcom_dvwir_read_u16(_wire + VQEC_VISION_AI_DSP_V2_MAJOR_OFFSET) !=
            VQEC_VISION_AI_DSP_V2_MAJOR ||
        vqec_vision_ai_qcom_dvwir_read_u16(_wire + VQEC_VISION_AI_DSP_V2_MINOR_OFFSET) !=
            VQEC_VISION_AI_DSP_V2_MINOR) {
        return vqec_vision_ai_dsp_v2_wire_incompatible;
    }
    if (vqec_vision_ai_qcom_dvwir_read_u16(_wire + VQEC_VISION_AI_DSP_V2_KIND_OFFSET) !=
        VQEC_VISION_AI_DSP_V2_SYNC_COMPLETION) {
        return vqec_vision_ai_dsp_v2_wire_unsupported;
    }
    vqec_vision_ai_dsp_v2_capabilities decoded;
    decoded.operations_mask =
        vqec_vision_ai_qcom_dvwir_read_u32(_wire + VQEC_VISION_AI_DSP_V2_FIELD_12_OFFSET);
    decoded.max_descriptor_bytes =
        vqec_vision_ai_qcom_dvwir_read_u32(_wire + VQEC_VISION_AI_DSP_V2_FIELD_16_OFFSET);
    decoded.max_input_bytes =
        vqec_vision_ai_qcom_dvwir_read_u32(_wire + VQEC_VISION_AI_DSP_V2_FIELD_20_OFFSET);
    decoded.max_output_bytes =
        vqec_vision_ai_qcom_dvwir_read_u32(_wire + VQEC_VISION_AI_DSP_V2_FIELD_24_OFFSET);
    decoded.domain_generation =
        vqec_vision_ai_qcom_dvwir_read_u32(_wire + VQEC_VISION_AI_DSP_V2_FIELD_28_OFFSET);
    if (!vqec_vision_ai_qcom_dvwir_capabilities_valid(&decoded)) {
        return vqec_vision_ai_dsp_v2_wire_malformed;
    }
    *_capabilities = decoded;
    return vqec_vision_ai_dsp_v2_wire_ok;
}

vqec_vision_ai_dsp_v2_wire_status
vqec_vision_ai_qcom_dvwir_encode_request(const vqec_vision_ai_dsp_v2_request* _request,
                                         uint8_t* _descriptor, size_t _descriptor_bytes) {
    if (_request == NULL || _descriptor == NULL ||
        !vqec_vision_ai_qcom_dvwir_operation_valid(_request->operation) ||
        _request->descriptor_bytes < VQEC_VISION_AI_DSP_V2_ENVELOPE_BYTES ||
        _request->descriptor_bytes != _descriptor_bytes || _request->domain_generation == 0 ||
        _request->input_bytes == 0 || _request->output_capacity_bytes == 0) {
        return vqec_vision_ai_dsp_v2_wire_malformed;
    }
    memcpy(_descriptor, g_vqec_vision_ai_dsp_v2_magic, sizeof(g_vqec_vision_ai_dsp_v2_magic));
    vqec_vision_ai_qcom_dvwir_write_u16(_descriptor + VQEC_VISION_AI_DSP_V2_MAJOR_OFFSET,
                                        VQEC_VISION_AI_DSP_V2_MAJOR);
    vqec_vision_ai_qcom_dvwir_write_u16(_descriptor + VQEC_VISION_AI_DSP_V2_MINOR_OFFSET,
                                        VQEC_VISION_AI_DSP_V2_MINOR);
    vqec_vision_ai_qcom_dvwir_write_u16(_descriptor + VQEC_VISION_AI_DSP_V2_HEADER_BYTES_OFFSET,
                                        VQEC_VISION_AI_DSP_V2_ENVELOPE_BYTES);
    vqec_vision_ai_qcom_dvwir_write_u16(_descriptor + VQEC_VISION_AI_DSP_V2_KIND_OFFSET,
                                        _request->operation);
    vqec_vision_ai_qcom_dvwir_write_u32(_descriptor + VQEC_VISION_AI_DSP_V2_FIELD_12_OFFSET,
                                        _request->descriptor_bytes);
    vqec_vision_ai_qcom_dvwir_write_u32(_descriptor + VQEC_VISION_AI_DSP_V2_FIELD_16_OFFSET,
                                        _request->input_bytes);
    vqec_vision_ai_qcom_dvwir_write_u32(_descriptor + VQEC_VISION_AI_DSP_V2_FIELD_20_OFFSET,
                                        _request->output_capacity_bytes);
    vqec_vision_ai_qcom_dvwir_write_u32(_descriptor + VQEC_VISION_AI_DSP_V2_FIELD_24_OFFSET,
                                        _request->domain_generation);
    vqec_vision_ai_qcom_dvwir_write_u32(_descriptor + VQEC_VISION_AI_DSP_V2_FIELD_28_OFFSET, 0);
    return vqec_vision_ai_dsp_v2_wire_ok;
}

vqec_vision_ai_dsp_v2_wire_status
vqec_vision_ai_qcom_dvwir_validate_request(const vqec_vision_ai_dsp_v2_capabilities* _capabilities,
                                           const uint8_t* _descriptor, size_t _descriptor_bytes,
                                           size_t _input_bytes, size_t _output_capacity_bytes,
                                           vqec_vision_ai_dsp_v2_request* _request) {
    if (!vqec_vision_ai_qcom_dvwir_capabilities_valid(_capabilities) || _descriptor == NULL ||
        _request == NULL || _descriptor_bytes < VQEC_VISION_AI_DSP_V2_ENVELOPE_BYTES) {
        return vqec_vision_ai_dsp_v2_wire_malformed;
    }
    if (memcmp(_descriptor, g_vqec_vision_ai_dsp_v2_magic, sizeof(g_vqec_vision_ai_dsp_v2_magic)) !=
            0 ||
        vqec_vision_ai_qcom_dvwir_read_u16(_descriptor +
                                           VQEC_VISION_AI_DSP_V2_HEADER_BYTES_OFFSET) !=
            VQEC_VISION_AI_DSP_V2_ENVELOPE_BYTES ||
        vqec_vision_ai_qcom_dvwir_read_u32(_descriptor + VQEC_VISION_AI_DSP_V2_FIELD_28_OFFSET) !=
            0) {
        return vqec_vision_ai_dsp_v2_wire_malformed;
    }
    if (vqec_vision_ai_qcom_dvwir_read_u16(_descriptor + VQEC_VISION_AI_DSP_V2_MAJOR_OFFSET) !=
            VQEC_VISION_AI_DSP_V2_MAJOR ||
        vqec_vision_ai_qcom_dvwir_read_u16(_descriptor + VQEC_VISION_AI_DSP_V2_MINOR_OFFSET) !=
            VQEC_VISION_AI_DSP_V2_MINOR) {
        return vqec_vision_ai_dsp_v2_wire_incompatible;
    }
    vqec_vision_ai_dsp_v2_request decoded;
    decoded.operation =
        vqec_vision_ai_qcom_dvwir_read_u16(_descriptor + VQEC_VISION_AI_DSP_V2_KIND_OFFSET);
    decoded.descriptor_bytes =
        vqec_vision_ai_qcom_dvwir_read_u32(_descriptor + VQEC_VISION_AI_DSP_V2_FIELD_12_OFFSET);
    decoded.input_bytes =
        vqec_vision_ai_qcom_dvwir_read_u32(_descriptor + VQEC_VISION_AI_DSP_V2_FIELD_16_OFFSET);
    decoded.output_capacity_bytes =
        vqec_vision_ai_qcom_dvwir_read_u32(_descriptor + VQEC_VISION_AI_DSP_V2_FIELD_20_OFFSET);
    decoded.domain_generation =
        vqec_vision_ai_qcom_dvwir_read_u32(_descriptor + VQEC_VISION_AI_DSP_V2_FIELD_24_OFFSET);
    if (!vqec_vision_ai_qcom_dvwir_operation_valid(decoded.operation) ||
        (_capabilities->operations_mask & (1U << (decoded.operation - 1U))) == 0) {
        return vqec_vision_ai_dsp_v2_wire_unsupported;
    }
    if (decoded.domain_generation != _capabilities->domain_generation) {
        return vqec_vision_ai_dsp_v2_wire_stale_generation;
    }
    if (decoded.descriptor_bytes != _descriptor_bytes || decoded.input_bytes != _input_bytes ||
        decoded.output_capacity_bytes != _output_capacity_bytes || decoded.input_bytes == 0 ||
        decoded.output_capacity_bytes == 0 ||
        _descriptor_bytes > _capabilities->max_descriptor_bytes ||
        _input_bytes > _capabilities->max_input_bytes ||
        _output_capacity_bytes > _capabilities->max_output_bytes || _input_bytes > UINT32_MAX ||
        _output_capacity_bytes > UINT32_MAX) {
        return vqec_vision_ai_dsp_v2_wire_out_of_range;
    }
    *_request = decoded;
    return vqec_vision_ai_dsp_v2_wire_ok;
}
