#include <sys/mman.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

#include "vqec_vision_dsp_preprocessor.hpp"
#include "vqec_vision_dsp_v1_service.h"

namespace vqec::vision::ai {
namespace {

vqec_vision_ai_dsp_v1_service g_service{};

void vqec_vision_ai_unit_dsppt_scale_luma(
    const std::uint8_t* _source, std::uint32_t _source_width,
    std::uint32_t _source_height, std::uint32_t _source_stride,
    std::uint8_t* _destination, std::uint32_t _destination_width,
    std::uint32_t _destination_height, std::uint32_t _destination_stride) {
    for (std::uint32_t row = 0U; row < _destination_height; ++row) {
        const auto source_row = row * _source_height / _destination_height;
        for (std::uint32_t column = 0U; column < _destination_width; ++column) {
            const auto source_column = column * _source_width / _destination_width;
            _destination[row * _destination_stride + column] =
                _source[source_row * _source_stride + source_column];
        }
    }
}

void vqec_vision_ai_unit_dsppt_scale_chroma(
    const std::uint8_t* _source, std::uint32_t _source_width,
    std::uint32_t _source_height, std::uint32_t _source_stride,
    std::uint8_t* _destination, std::uint32_t _destination_width,
    std::uint32_t _destination_height, std::uint32_t _destination_stride) {
    for (std::uint32_t row = 0U; row < _destination_height; ++row) {
        const auto source_row = row * _source_height / _destination_height;
        for (std::uint32_t column = 0U; column < _destination_width; ++column) {
            const auto source_column = column * _source_width / _destination_width;
            const auto* input = _source + source_row * _source_stride + source_column * 2U;
            auto* output = _destination + row * _destination_stride + column * 2U;
            output[0] = input[0];
            output[1] = input[1];
        }
    }
}

void vqec_vision_ai_unit_dsppt_convert_color(
    const std::uint8_t* _source_y, const std::uint8_t*, std::uint32_t _width,
    std::uint32_t _height, std::uint32_t _y_stride, std::uint32_t,
    std::uint8_t* _destination, std::uint32_t _destination_stride) {
    for (std::uint32_t row = 0U; row < _height; ++row) {
        for (std::uint32_t column = 0U; column < _width; ++column) {
            const auto value = _source_y[row * _y_stride + column];
            auto* output = _destination + row * _destination_stride + column * 3U;
            output[0] = value;
            output[1] = value;
            output[2] = value;
        }
    }
}

int vqec_vision_ai_unit_dsppt_open(const char*, std::uint64_t* _handle) {
    *_handle = 1U;
    return 0;
}

int vqec_vision_ai_unit_dsppt_close(std::uint64_t) {
    return 0;
}

int vqec_vision_ai_unit_dsppt_query(
    std::uint64_t, std::uint8_t* _response, int _response_bytes) {
    return vqec_vision_ai_qcom_d1svc_query_capabilities(
        &g_service, _response, static_cast<std::size_t>(_response_bytes)) ==
        vqec_vision_ai_dsp_v1_wire_ok ? 0 : -1;
}

int vqec_vision_ai_unit_dsppt_execute(
    std::uint64_t, const std::uint8_t* _descriptor, int _descriptor_bytes,
    const std::uint8_t* _input, int _input_bytes, std::uint8_t* _output,
    int _output_bytes, std::uint8_t* _response, int _response_bytes) {
    return vqec_vision_ai_qcom_d1svc_execute(
        &g_service, _descriptor, static_cast<std::size_t>(_descriptor_bytes), _input,
        static_cast<std::size_t>(_input_bytes), _output,
        static_cast<std::size_t>(_output_bytes), _response,
        static_cast<std::size_t>(_response_bytes)) == vqec_vision_ai_dsp_v1_wire_ok ? 0 : -1;
}

inference_plan vqec_vision_ai_unit_dsppt_plan(
    std::uint32_t _source_width, std::uint32_t _source_height) {
    inference_plan plan;
    plan.source_width_ = _source_width;
    plan.source_height_ = _source_height;
    plan.fps_numerator_ = 30U;
    plan.fps_denominator_ = 1U;
    plan.tensor_width_ = 32U;
    plan.tensor_height_ = 32U;
    plan.input_type_ = tensor_element_type::uint16;
    plan.placement_ = image_placement::centre;
    plan.preprocess_.source_format_ = source_pixel_format::nv12;
    plan.preprocess_.matrix_ = color_matrix::bt709;
    plan.preprocess_.range_ = color_range::limited;
    plan.preprocess_.resize_ = resize_mode::letterbox;
    plan.preprocess_.interpolation_ = interpolation_mode::bilinear;
    plan.preprocess_.placement_ = image_placement::centre;
    plan.preprocess_.pad_value_ = {114.0F, 114.0F, 114.0F};
    plan.preprocess_.normalization_ = normalization_formula::offset_scale;
    plan.preprocess_.scale_ = {1.0F / 255.0F, 1.0F / 255.0F, 1.0F / 255.0F};
    plan.model_path_ = "/model.bin";
    plan.backend_path_ = "/libQnnHtp.so";
    plan.system_path_ = "/libQnnSystem.so";
    plan.input_queue_bytes_ =
        static_cast<std::uint64_t>(_source_width) * _source_height * 3U / 2U;
    plan.output_queue_buffers_ = 1U;
    return plan;
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    using namespace vqec::vision::ai;
    constexpr std::uint32_t generation = 29U;
    if (vqec_vision_ai_qcom_d1svc_initialize(&g_service, generation) !=
        vqec_vision_ai_dsp_v1_wire_ok) {
        return 1;
    }
    const vqec_vision_ai_dsp_v1_image_backend backend{
        vqec_vision_ai_unit_dsppt_scale_luma,
        vqec_vision_ai_unit_dsppt_scale_chroma,
        vqec_vision_ai_unit_dsppt_convert_color};
    if (vqec_vision_ai_qcom_d1svc_configure_image_backend(&g_service, &backend) !=
        vqec_vision_ai_dsp_v1_wire_ok) {
        return 1;
    }
    dsp_v1_rpc_api rpc{
        vqec_vision_ai_unit_dsppt_open, vqec_vision_ai_unit_dsppt_close,
        vqec_vision_ai_unit_dsppt_query, vqec_vision_ai_unit_dsppt_execute, nullptr};
    auto client = std::make_shared<dsp_v1_client>(rpc);
    if (client->vqec_vision_ai_qcom_d1cli_open({"/fixture", false}).code_ !=
        status_code::ok) {
        return 1;
    }
    auto cache = std::make_shared<dsp_buffer_cache>(dsp_buffer_cache_config{4U, false});
    dsp_preprocessor processor({client, cache});

    constexpr std::uint32_t width = 64U;
    constexpr std::uint32_t height = 32U;
    constexpr std::size_t frame_bytes = width * height * 3U / 2U;
    const int fd = ::memfd_create("dsp_v1_preprocess_test", 0);
    if (fd < 0 || ::ftruncate(fd, static_cast<off_t>(frame_bytes)) != 0) {
        return 1;
    }
    void* mapped = ::mmap(nullptr, frame_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        ::close(fd);
        return 1;
    }
    std::memset(mapped, 128, frame_bytes);
    ::munmap(mapped, frame_bytes);
    raw_frame frame;
    frame.owner_ = std::make_shared<int>(fd);
    frame.native_handle_ = fd;
    frame.descriptor_.width_ = width;
    frame.descriptor_.height_ = height;
    frame.descriptor_.strides_[0] = static_cast<std::int32_t>(width);
    frame.descriptor_.offsets_[1] = width * height;
    frame.descriptor_.strides_[1] = static_cast<std::int32_t>(width);
    frame.descriptor_.allocation_size_bytes_ = frame_bytes;
    frame.descriptor_.view_size_bytes_ = frame_bytes;
    const auto plan = vqec_vision_ai_unit_dsppt_plan(width, height);
    tensor_spec target;
    target.dtype_ = tensor_element_type::uint16;
    target.layout_ = tensor_layout::nhwc;
    target.dimensions_ = {1U, 32U, 32U, 3U};
    target.quantization_ = {true, 1.0F / 65535.0F, 0};

    std::vector<tensor_blob> outputs;
    const auto transformed = processor.vqec_vision_ai_ports_imgpr_preprocess(
        frame, plan, target, outputs);
    if (transformed.code_ != status_code::ok || outputs.size() != 1U ||
        outputs[0].bytes_.size() != 32U * 32U * 3U * sizeof(std::uint16_t)) {
        std::cerr << transformed.message_ << '\n';
        ::close(fd);
        return 1;
    }
    const auto* tensor = reinterpret_cast<const std::uint16_t*>(outputs[0].bytes_.data());
    if (tensor[0] != 114U * 257U || tensor[16U * 32U * 3U] != 128U * 257U) {
        ::close(fd);
        return 1;
    }
    const auto accepted = outputs[0].bytes_;
    auto invalid = target;
    invalid.quantization_.scale_ *= 2.0F;
    if (processor.vqec_vision_ai_ports_imgpr_preprocess(
            frame, plan, invalid, outputs).code_ == status_code::ok ||
        outputs[0].bytes_ != accepted) {
        ::close(fd);
        return 1;
    }
    auto overlapping = frame;
    overlapping.descriptor_.offsets_[1] = 1U;
    if (processor.vqec_vision_ai_ports_imgpr_validate(
            overlapping, plan, target).code_ == status_code::ok) {
        ::close(fd);
        return 1;
    }
    ::close(fd);
    vqec_vision_ai_qcom_d1svc_cleanup(&g_service);
    std::cout << "DSP v1 preprocessor adapter tests passed.\n";
    return 0;
}
