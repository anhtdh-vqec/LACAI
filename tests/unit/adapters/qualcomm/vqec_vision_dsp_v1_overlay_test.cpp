#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

extern "C" {
#include "vqec_vision_dsp_v1_overlay.h"
}

namespace {

constexpr std::uint32_t g_width = 64U;
constexpr std::uint32_t g_height = 64U;
constexpr std::uint32_t g_y_bytes = g_width * g_height;
constexpr std::uint32_t g_surface_bytes = g_y_bytes + g_width * g_height / 2U;
constexpr std::uint32_t g_generation = 19U;

vqec_vision_ai_dsp_v1_capabilities vqec_vision_ai_unit_d1ovt_capabilities() {
    return {1U << (VQEC_VISION_AI_DSP_V1_OVERLAY_COMPOSE - 1U),
            VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_DESCRIPTOR_BYTES,
            VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_SURFACE_BYTES,
            VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_SURFACE_BYTES, g_generation};
}

}  // namespace

int main() {
    const auto capabilities = vqec_vision_ai_unit_d1ovt_capabilities();
    vqec_vision_ai_dsp_v1_overlay_frame frame{};
    frame.width = g_width;
    frame.height = g_height;
    frame.source_y_stride = g_width;
    frame.source_uv_offset = g_y_bytes;
    frame.source_uv_stride = g_width;
    frame.destination_y_stride = g_width;
    frame.destination_uv_offset = g_y_bytes;
    frame.destination_uv_stride = g_width;
    frame.border_thickness = 2U;
    frame.font_scale = 1U;

    constexpr std::array<std::uint8_t, 6> label{'p', 'e', 'r', 's', 'o', 'n'};
    vqec_vision_ai_dsp_v1_overlay_box box{};
    box.x = 16U;
    box.y = 16U;
    box.width = 32U;
    box.height = 32U;
    box.color_y = 145U;
    box.color_u = 54U;
    box.color_v = 34U;
    box.label_bytes = static_cast<std::uint16_t>(label.size());

    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_DESCRIPTOR_BYTES> descriptor{};
    std::size_t descriptor_bytes = 0;
    const auto encoded = vqec_vision_ai_qcom_d1ovr_encode_descriptor(
        &capabilities, &frame, &box, 1U, label.data(), label.size(), g_surface_bytes,
        g_surface_bytes, descriptor.data(), descriptor.size(), &descriptor_bytes);
    assert(encoded == vqec_vision_ai_dsp_v1_wire_ok);

    std::array<std::uint8_t, g_surface_bytes> input{};
    std::array<std::uint8_t, g_surface_bytes> output{};
    input.fill(128U);
    output.fill(0U);
    std::uint32_t written = 0;
    const auto executed = vqec_vision_ai_qcom_d1ovr_execute(
        &capabilities, descriptor.data(), descriptor_bytes, input.data(), input.size(),
        output.data(), output.size(), &written);
    assert(executed == vqec_vision_ai_dsp_v1_wire_ok);
    assert(written == g_surface_bytes);
    assert(output[16U * g_width + 16U] == box.color_y);
    assert(output[32U * g_width + 32U] == 128U);
    assert(output[g_y_bytes + 8U * g_width + 16U] == box.color_u);
    bool has_text = false;
    for (std::uint32_t row = 7U; row < 16U && !has_text; ++row) {
        for (std::uint32_t column = 16U; column < 56U; ++column) {
            has_text = has_text || output[row * g_width + column] == 235U;
        }
    }
    assert(has_text);

    auto malformed = descriptor;
    malformed[VQEC_VISION_AI_DSP_V1_OVERLAY_HEADER_BYTES - 1U] = 1U;
    assert(vqec_vision_ai_qcom_d1ovr_execute(
               &capabilities, malformed.data(), descriptor_bytes, input.data(), input.size(),
               output.data(), output.size(), &written) ==
           vqec_vision_ai_dsp_v1_wire_malformed);

    constexpr std::array<std::uint8_t, 1> invalid_label{1U};
    assert(vqec_vision_ai_qcom_d1ovr_encode_descriptor(
               &capabilities, &frame, &box, 1U, invalid_label.data(), invalid_label.size(),
               g_surface_bytes, g_surface_bytes, descriptor.data(), descriptor.size(),
               &descriptor_bytes) == vqec_vision_ai_dsp_v1_wire_malformed);
    std::cout << "DSP v1 overlay tests passed.\n";
    return 0;
}
