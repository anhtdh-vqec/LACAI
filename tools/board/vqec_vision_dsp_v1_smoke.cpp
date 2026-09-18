#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "vqec_vision_dsp_v1_client.hpp"
#include "vqec_vision_rpcmem_pool.hpp"

extern "C" {
#include "vqec_vision_dsp_v1_dense.h"
#include "vqec_vision_dsp_v1_image.h"
#include "vqec_vision_dsp_v1_overlay.h"
}

namespace {

constexpr int g_exit_success = 0;
constexpr int g_exit_usage = 2;
constexpr int g_exit_failure = 1;
constexpr std::uint32_t g_fixture_source_side = 100U;
constexpr std::uint16_t g_fixture_box_center = 20U;
constexpr std::uint16_t g_fixture_box_side = 10U;
constexpr std::uint16_t g_fixture_score = 1U;
constexpr float g_fixture_threshold = 0.5F;
constexpr std::uint32_t g_overlay_side = 64U;
constexpr std::uint32_t g_overlay_y_bytes = g_overlay_side * g_overlay_side;
constexpr std::uint32_t g_overlay_surface_bytes =
    g_overlay_y_bytes + g_overlay_y_bytes / 2U;
constexpr std::uint32_t g_image_source_side = 64U;
constexpr std::uint32_t g_image_tensor_side = 32U;
constexpr std::uint32_t g_image_input_bytes =
    g_image_source_side * g_image_source_side * 3U / 2U;
constexpr std::uint32_t g_image_output_bytes =
    g_image_tensor_side * g_image_tensor_side * 3U * sizeof(std::uint16_t);

void vqec_vision_ai_tools_d1smk_write_u16(std::uint8_t* _output, std::uint16_t _value) {
    _output[0] = static_cast<std::uint8_t>(_value);
    _output[1] = static_cast<std::uint8_t>(_value >> 8U);
}

float vqec_vision_ai_tools_d1smk_read_f32(const std::uint8_t* _input) {
    float value = 0.0F;
    std::uint32_t bits = static_cast<std::uint32_t>(_input[0]) |
                         (static_cast<std::uint32_t>(_input[1]) << 8U) |
                         (static_cast<std::uint32_t>(_input[2]) << 16U) |
                         (static_cast<std::uint32_t>(_input[3]) << 24U);
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

}  // namespace

int main(int _argc, char** _argv) {
    using namespace vqec::vision::ai;
    if (_argc != 3 || std::string(_argv[1]) != "--skel-dir" || _argv[2][0] != '/') {
        std::cerr << "usage: vqec_vision_dsp_v1_smoke --skel-dir <absolute-directory>\n";
        return g_exit_usage;
    }

    dsp_v1_client client;
    dsp_v1_client_config client_config{};
    client_config.skel_dir_ = _argv[2];
    client_config.enable_unsigned_pd_ = true;
    const auto opened = client.vqec_vision_ai_qcom_d1cli_open(client_config);
    if (opened.code_ != status_code::ok) {
        std::cerr << opened.message_ << '\n';
        return g_exit_failure;
    }

    const auto capabilities = client.vqec_vision_ai_qcom_d1cli_capabilities();
    const std::uint32_t dense_mask = 1U << (VQEC_VISION_AI_DSP_V1_DENSE_DECODE - 1U);
    if ((capabilities.operations_mask & dense_mask) == 0U) {
        std::cerr << "DSP v1 service does not advertise dense_decode\n";
        client.vqec_vision_ai_qcom_d1cli_close();
        return g_exit_failure;
    }

    constexpr std::size_t g_fixture_box_bytes = 4U * sizeof(std::uint16_t);
    constexpr std::size_t g_fixture_input_bytes = g_fixture_box_bytes + sizeof(std::uint16_t);
    std::array<std::uint8_t, g_fixture_input_bytes> input{};
    vqec_vision_ai_tools_d1smk_write_u16(input.data() + 0U, g_fixture_box_center);
    vqec_vision_ai_tools_d1smk_write_u16(input.data() + 2U, g_fixture_box_center);
    vqec_vision_ai_tools_d1smk_write_u16(input.data() + 4U, g_fixture_box_side);
    vqec_vision_ai_tools_d1smk_write_u16(input.data() + 6U, g_fixture_box_side);
    vqec_vision_ai_tools_d1smk_write_u16(input.data() + g_fixture_box_bytes, g_fixture_score);

    vqec_vision_ai_dsp_v1_dense_config dense_config{};
    dense_config.flags = VQEC_VISION_AI_DSP_V1_DENSE_CLASS_AWARE_NMS;
    dense_config.prediction_count = 1U;
    dense_config.class_count = 1U;
    dense_config.box_offset = 0U;
    dense_config.score_offset = static_cast<std::uint32_t>(g_fixture_box_bytes);
    dense_config.source_width = g_fixture_source_side;
    dense_config.source_height = g_fixture_source_side;
    dense_config.candidate_capacity = 1U;
    dense_config.output_capacity = 1U;
    dense_config.box_scale = 1.0F;
    dense_config.score_scale = 1.0F;
    dense_config.confidence_threshold = g_fixture_threshold;
    dense_config.iou_threshold = g_fixture_threshold;
    dense_config.scale_x = 1.0F;
    dense_config.scale_y = 1.0F;

    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_DENSE_DESCRIPTOR_BYTES> descriptor{};
    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_DENSE_RECORD_BYTES> output{};
    const auto encoded = vqec_vision_ai_qcom_d1dns_encode_descriptor(
        &dense_config, capabilities.domain_generation, input.size(), output.size(),
        descriptor.data(), descriptor.size());
    if (encoded != vqec_vision_ai_dsp_v1_wire_ok) {
        std::cerr << "Cannot encode the generic dense descriptor\n";
        client.vqec_vision_ai_qcom_d1cli_close();
        return g_exit_failure;
    }

    const auto executed = client.vqec_vision_ai_qcom_d1cli_execute(
        descriptor.data(), descriptor.size(), input.data(), input.size(), output.data(),
        output.size());
    if (executed.status_.code_ != status_code::ok ||
        executed.completion_ != dsp_v1_completion::completed ||
        executed.output_bytes_ != output.size()) {
        std::cerr << executed.status_.message_ << '\n';
        client.vqec_vision_ai_qcom_d1cli_close();
        return g_exit_failure;
    }

    const std::uint32_t image_mask =
        1U << (VQEC_VISION_AI_DSP_V1_IMAGE_TRANSFORM - 1U);
    if ((capabilities.operations_mask & image_mask) == 0U) {
        std::cerr << "DSP v1 service does not advertise image_transform\n";
        client.vqec_vision_ai_qcom_d1cli_close();
        return g_exit_failure;
    }
    rpcmem_pool image_pool;
    const auto image_allocated = image_pool.vqec_vision_ai_qcom_rpcm_allocate(
        std::vector<std::size_t>{g_image_input_bytes, g_image_output_bytes});
    if (image_allocated.code_ != status_code::ok) {
        std::cerr << image_allocated.message_ << '\n';
        client.vqec_vision_ai_qcom_d1cli_close();
        return g_exit_failure;
    }
    const auto& image_input = image_pool.vqec_vision_ai_qcom_rpcm_slot(0U);
    const auto& image_output = image_pool.vqec_vision_ai_qcom_rpcm_slot(1U);
    std::memset(image_input.data_, 128, g_image_input_bytes);
    std::memset(image_output.data_, 0, g_image_output_bytes);
    vqec_vision_ai_dsp_v1_image_config image_config{};
    image_config.pixel_format = VQEC_VISION_AI_DSP_V1_IMAGE_PIXEL_NV12;
    image_config.matrix = VQEC_VISION_AI_DSP_V1_IMAGE_MATRIX_BT709;
    image_config.range = VQEC_VISION_AI_DSP_V1_IMAGE_RANGE_LIMITED;
    image_config.interpolation = VQEC_VISION_AI_DSP_V1_IMAGE_INTERPOLATION_BILINEAR;
    image_config.resize = VQEC_VISION_AI_DSP_V1_IMAGE_RESIZE_LETTERBOX;
    image_config.placement = VQEC_VISION_AI_DSP_V1_IMAGE_PLACEMENT_CENTRE;
    image_config.channel_order = VQEC_VISION_AI_DSP_V1_IMAGE_CHANNEL_RGB;
    image_config.normalization = VQEC_VISION_AI_DSP_V1_IMAGE_NORMALIZATION_OFFSET_SCALE;
    image_config.dtype = VQEC_VISION_AI_DSP_V1_IMAGE_DTYPE_UINT16;
    image_config.source_width = g_image_source_side;
    image_config.source_height = g_image_source_side;
    image_config.source_y_stride = g_image_source_side;
    image_config.source_uv_offset = g_image_source_side * g_image_source_side;
    image_config.source_uv_stride = g_image_source_side;
    image_config.crop_width = g_image_source_side;
    image_config.crop_height = g_image_source_side;
    image_config.tensor_width = g_image_tensor_side;
    image_config.tensor_height = g_image_tensor_side;
    image_config.destination_width = g_image_tensor_side;
    image_config.destination_height = g_image_tensor_side;
    image_config.pad[0] = image_config.pad[1] = image_config.pad[2] = 114.0F;
    image_config.scale[0] = image_config.scale[1] = image_config.scale[2] = 1.0F / 255.0F;
    image_config.quantization_scale = 1.0F / 65535.0F;
    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_IMAGE_DESCRIPTOR_BYTES>
        image_descriptor{};
    const auto image_encoded = vqec_vision_ai_qcom_d1img_encode_descriptor(
        &image_config, capabilities.domain_generation, g_image_input_bytes,
        g_image_output_bytes, image_descriptor.data(), image_descriptor.size());
    const auto image_executed = client.vqec_vision_ai_qcom_d1cli_execute(
        image_descriptor.data(), image_descriptor.size(),
        static_cast<const std::uint8_t*>(image_input.data_), g_image_input_bytes,
        static_cast<std::uint8_t*>(image_output.data_), g_image_output_bytes);
    std::uint16_t image_first_value = 0U;
    std::memcpy(&image_first_value, image_output.data_, sizeof(image_first_value));
    if (image_encoded != vqec_vision_ai_dsp_v1_wire_ok ||
        image_executed.status_.code_ != status_code::ok ||
        image_executed.completion_ != dsp_v1_completion::completed ||
        image_executed.output_bytes_ != g_image_output_bytes || image_first_value == 0U) {
        std::cerr << "Registered image transform failed: "
                  << image_executed.status_.message_ << '\n';
        client.vqec_vision_ai_qcom_d1cli_close();
        return g_exit_failure;
    }

    const std::uint32_t overlay_mask =
        1U << (VQEC_VISION_AI_DSP_V1_OVERLAY_COMPOSE - 1U);
    if ((capabilities.operations_mask & overlay_mask) == 0U) {
        std::cerr << "DSP v1 service does not advertise overlay_compose\n";
        client.vqec_vision_ai_qcom_d1cli_close();
        return g_exit_failure;
    }
    rpcmem_pool overlay_pool;
    const auto allocated = overlay_pool.vqec_vision_ai_qcom_rpcm_allocate(
        std::vector<std::size_t>{g_overlay_surface_bytes, g_overlay_surface_bytes});
    if (allocated.code_ != status_code::ok) {
        std::cerr << allocated.message_ << '\n';
        client.vqec_vision_ai_qcom_d1cli_close();
        return g_exit_failure;
    }
    const auto& overlay_input = overlay_pool.vqec_vision_ai_qcom_rpcm_slot(0U);
    const auto& overlay_output = overlay_pool.vqec_vision_ai_qcom_rpcm_slot(1U);
    std::memset(overlay_input.data_, 128, g_overlay_surface_bytes);
    std::memset(overlay_output.data_, 0, g_overlay_surface_bytes);
    vqec_vision_ai_dsp_v1_overlay_frame overlay_frame{};
    overlay_frame.width = g_overlay_side;
    overlay_frame.height = g_overlay_side;
    overlay_frame.source_y_stride = g_overlay_side;
    overlay_frame.source_uv_offset = g_overlay_y_bytes;
    overlay_frame.source_uv_stride = g_overlay_side;
    overlay_frame.destination_y_stride = g_overlay_side;
    overlay_frame.destination_uv_offset = g_overlay_y_bytes;
    overlay_frame.destination_uv_stride = g_overlay_side;
    overlay_frame.border_thickness = 2U;
    overlay_frame.font_scale = 1U;
    vqec_vision_ai_dsp_v1_overlay_box overlay_box{};
    overlay_box.x = 16U;
    overlay_box.y = 16U;
    overlay_box.width = 32U;
    overlay_box.height = 32U;
    overlay_box.color_y = 145U;
    overlay_box.color_u = 54U;
    overlay_box.color_v = 34U;
    constexpr std::array<std::uint8_t, 6U> overlay_label{'p', 'e', 'r', 's', 'o', 'n'};
    overlay_box.label_bytes = static_cast<std::uint16_t>(overlay_label.size());
    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_DESCRIPTOR_BYTES>
        overlay_descriptor{};
    std::size_t overlay_descriptor_bytes = 0U;
    const auto overlay_encoded = vqec_vision_ai_qcom_d1ovr_encode_descriptor(
        &capabilities, &overlay_frame, &overlay_box, 1U, overlay_label.data(),
        overlay_label.size(), g_overlay_surface_bytes, g_overlay_surface_bytes,
        overlay_descriptor.data(), overlay_descriptor.size(), &overlay_descriptor_bytes);
    const auto overlay_executed = client.vqec_vision_ai_qcom_d1cli_execute(
        overlay_descriptor.data(), overlay_descriptor_bytes,
        static_cast<const std::uint8_t*>(overlay_input.data_), g_overlay_surface_bytes,
        static_cast<std::uint8_t*>(overlay_output.data_), g_overlay_surface_bytes);
    const auto* overlay_bytes = static_cast<const std::uint8_t*>(overlay_output.data_);
    if (overlay_encoded != vqec_vision_ai_dsp_v1_wire_ok ||
        overlay_executed.status_.code_ != status_code::ok ||
        overlay_executed.completion_ != dsp_v1_completion::completed ||
        overlay_bytes[16U * g_overlay_side + 16U] != overlay_box.color_y ||
        overlay_bytes[32U * g_overlay_side + 32U] != 128U) {
        std::cerr << "Registered overlay compose failed: "
                  << overlay_executed.status_.message_ << '\n';
        client.vqec_vision_ai_qcom_d1cli_close();
        return g_exit_failure;
    }

    std::cout << "domain_generation=" << capabilities.domain_generation
              << " operations_mask=" << capabilities.operations_mask
              << " output_bytes=" << executed.output_bytes_
              << " detail=" << executed.detail_
              << " first_box=" << vqec_vision_ai_tools_d1smk_read_f32(output.data()) << ','
              << vqec_vision_ai_tools_d1smk_read_f32(output.data() + sizeof(float)) << ','
              << vqec_vision_ai_tools_d1smk_read_f32(output.data() + 2U * sizeof(float)) << ','
              << vqec_vision_ai_tools_d1smk_read_f32(output.data() + 3U * sizeof(float))
              << " image_bytes=" << image_executed.output_bytes_
              << " image_first_value=" << image_first_value
              << " overlay_bytes=" << overlay_executed.output_bytes_
              << " overlay_pixel="
              << static_cast<unsigned>(overlay_bytes[16U * g_overlay_side + 16U]) << '\n';
    client.vqec_vision_ai_qcom_d1cli_close();
    return g_exit_success;
}
