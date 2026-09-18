// Device-free conformance for the fixed v1 image_transform descriptor and kernel boundary.

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

extern "C" {
#include "vqec_vision_dsp_v1_image.h"
}

namespace {

void vqec_vision_ai_unit_d1imt_copy_luma(
    const std::uint8_t* _source, std::uint32_t _source_width,
    std::uint32_t _source_height, std::uint32_t _source_stride,
    std::uint8_t* _destination, std::uint32_t _destination_width,
    std::uint32_t _destination_height, std::uint32_t _destination_stride) {
    assert(_source_width == _destination_width);
    assert(_source_height == _destination_height);
    for (std::uint32_t row = 0U; row < _source_height; ++row) {
        std::memcpy(_destination + row * _destination_stride,
            _source + row * _source_stride, _source_width);
    }
}

void vqec_vision_ai_unit_d1imt_copy_chroma(
    const std::uint8_t* _source, std::uint32_t _source_width,
    std::uint32_t _source_height, std::uint32_t _source_stride,
    std::uint8_t* _destination, std::uint32_t _destination_width,
    std::uint32_t _destination_height, std::uint32_t _destination_stride) {
    assert(_source_width == _destination_width);
    assert(_source_height == _destination_height);
    for (std::uint32_t row = 0U; row < _source_height; ++row) {
        std::memcpy(_destination + row * _destination_stride,
            _source + row * _source_stride, _source_width * 2U);
    }
}

void vqec_vision_ai_unit_d1imt_gray_color(
    const std::uint8_t* _source_y, const std::uint8_t*, std::uint32_t _width,
    std::uint32_t _height, std::uint32_t _y_stride, std::uint32_t,
    std::uint8_t* _destination, std::uint32_t _destination_stride) {
    for (std::uint32_t row = 0U; row < _height; ++row) {
        for (std::uint32_t column = 0U; column < _width; ++column) {
            const auto value = _source_y[row * _y_stride + column];
            auto* pixel = _destination + row * _destination_stride + column * 3U;
            pixel[0] = value;
            pixel[1] = value;
            pixel[2] = value;
        }
    }
}

}  // namespace

int main() {
    constexpr std::uint32_t generation = 23U;
    constexpr std::uint32_t source_width = 4U;
    constexpr std::uint32_t source_height = 2U;
    constexpr std::uint32_t tensor_side = 4U;
    constexpr std::uint32_t input_bytes = 12U;
    constexpr std::uint32_t output_bytes = tensor_side * tensor_side * 3U * 2U;
    vqec_vision_ai_dsp_v1_capabilities capabilities{
        1U << (VQEC_VISION_AI_DSP_V1_IMAGE_TRANSFORM - 1U),
        VQEC_VISION_AI_DSP_V1_IMAGE_DESCRIPTOR_BYTES,
        VQEC_VISION_AI_DSP_V1_IMAGE_MAX_INPUT_BYTES,
        VQEC_VISION_AI_DSP_V1_IMAGE_MAX_OUTPUT_BYTES,
        generation};
    vqec_vision_ai_dsp_v1_image_config config{};
    config.pixel_format = VQEC_VISION_AI_DSP_V1_IMAGE_PIXEL_NV12;
    config.matrix = VQEC_VISION_AI_DSP_V1_IMAGE_MATRIX_BT709;
    config.range = VQEC_VISION_AI_DSP_V1_IMAGE_RANGE_LIMITED;
    config.interpolation = VQEC_VISION_AI_DSP_V1_IMAGE_INTERPOLATION_BILINEAR;
    config.resize = VQEC_VISION_AI_DSP_V1_IMAGE_RESIZE_LETTERBOX;
    config.placement = VQEC_VISION_AI_DSP_V1_IMAGE_PLACEMENT_CENTRE;
    config.channel_order = VQEC_VISION_AI_DSP_V1_IMAGE_CHANNEL_RGB;
    config.normalization = VQEC_VISION_AI_DSP_V1_IMAGE_NORMALIZATION_OFFSET_SCALE;
    config.dtype = VQEC_VISION_AI_DSP_V1_IMAGE_DTYPE_UINT16;
    config.source_width = source_width;
    config.source_height = source_height;
    config.source_y_stride = source_width;
    config.source_uv_offset = source_width * source_height;
    config.source_uv_stride = source_width;
    config.crop_width = source_width;
    config.crop_height = source_height;
    config.tensor_width = tensor_side;
    config.tensor_height = tensor_side;
    config.destination_y = 1U;
    config.destination_width = source_width;
    config.destination_height = source_height;
    config.pad[0] = config.pad[1] = config.pad[2] = 114.0F;
    config.scale[0] = config.scale[1] = config.scale[2] = 1.0F / 255.0F;
    config.quantization_scale = 1.0F / 65535.0F;

    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_IMAGE_DESCRIPTOR_BYTES> descriptor{};
    assert(vqec_vision_ai_qcom_d1img_encode_descriptor(
               &config, generation, input_bytes, output_bytes, descriptor.data(),
               descriptor.size()) == vqec_vision_ai_dsp_v1_wire_ok);
    std::array<std::uint8_t, input_bytes> input{
        10U, 20U, 30U, 40U, 50U, 60U, 70U, 80U, 128U, 128U, 128U, 128U};
    alignas(std::uint16_t) std::array<std::uint8_t, output_bytes> output{};
    vqec_vision_ai_dsp_v1_image_backend backend{
        vqec_vision_ai_unit_d1imt_copy_luma,
        vqec_vision_ai_unit_d1imt_copy_chroma,
        vqec_vision_ai_unit_d1imt_gray_color};
    vqec_vision_ai_dsp_v1_image_scratch scratch{};
    std::uint32_t written = 0U;
    assert(vqec_vision_ai_qcom_d1img_execute(
               &capabilities, &backend, &scratch, descriptor.data(), descriptor.size(),
               input.data(), input.size(), output.data(), output.size(), &written) ==
           vqec_vision_ai_dsp_v1_wire_ok);
    assert(written == output_bytes);
    const auto* tensor = reinterpret_cast<const std::uint16_t*>(output.data());
    assert(tensor[0] == 114U * 257U);
    assert(tensor[tensor_side * 3U] == 10U * 257U);
    assert(tensor[(tensor_side * 3U) + 3U] == 20U * 257U);
    assert(tensor[(tensor_side * tensor_side - 1U) * 3U] == 114U * 257U);

    auto malformed = descriptor;
    malformed.back() = 1U;
    assert(vqec_vision_ai_qcom_d1img_execute(
               &capabilities, &backend, &scratch, malformed.data(), malformed.size(),
               input.data(), input.size(), output.data(), output.size(), &written) ==
           vqec_vision_ai_dsp_v1_wire_malformed);
    config.channel_order = VQEC_VISION_AI_DSP_V1_IMAGE_CHANNEL_BGR;
    assert(vqec_vision_ai_qcom_d1img_encode_descriptor(
               &config, generation, input_bytes, output_bytes, descriptor.data(),
               descriptor.size()) == vqec_vision_ai_dsp_v1_wire_unsupported);
    vqec_vision_ai_qcom_d1img_release_scratch(&scratch);
    std::cout << "DSP v1 image transform tests passed.\n";
    return 0;
}
