#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "vqec_vision_reference_processor.hpp"

using namespace vqec::vision::ai;

namespace {

inference_plan vqec_vision_ai_unit_rptst_plan() {
    inference_plan plan;
    plan.source_width_ = 16;
    plan.source_height_ = 16;
    plan.fps_numerator_ = 25;
    plan.fps_denominator_ = 1;
    plan.tensor_width_ = 8;
    plan.tensor_height_ = 8;
    plan.input_type_ = tensor_element_type::uint16;
    plan.channel_order_ = channel_order::rgb;
    plan.placement_ = image_placement::centre;
    plan.mean_ = {0.0, 0.0, 0.0};
    plan.sigma_ = {1.0, 1.0, 1.0};
    plan.model_path_ = "/opt/vqec/models/model.bin";
    plan.backend_path_ = "/usr/lib/libQnnHtp.so";
    plan.system_path_ = "/usr/lib/libQnnSystem.so";
    plan.input_queue_bytes_ = 4096;
    plan.output_queue_buffers_ = 1;
    return plan;
}

}  // namespace

int main() {
    // Gray NV12 frame in a memfd: Y=128, U=V=128 -> BT.601 limited luma maps to ~130.
    constexpr std::uint64_t allocation_bytes = 16U * 16U * 3U / 2U;
    const int fd = ::memfd_create("vqec_vision_ai_rptst", MFD_CLOEXEC);
    assert(fd >= 0);
    assert(::ftruncate(fd, static_cast<off_t>(allocation_bytes)) == 0);
    void* base = ::mmap(nullptr, allocation_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(base != MAP_FAILED);
    std::memset(base, 128, allocation_bytes);
    ::munmap(base, allocation_bytes);

    raw_frame frame;
    frame.descriptor_.width_ = 16;
    frame.descriptor_.height_ = 16;
    frame.descriptor_.offsets_ = {0, 256};
    frame.descriptor_.strides_ = {16, 16};
    frame.descriptor_.view_size_bytes_ = allocation_bytes;
    frame.descriptor_.memory_offset_bytes_ = 0;
    frame.descriptor_.allocation_size_bytes_ = allocation_bytes;
    frame.native_handle_ = fd;
    frame.owner_ = std::make_shared<int>(0);

    const auto plan = vqec_vision_ai_unit_rptst_plan();
    reference_image_processor processor;

    tensor_spec float_target;
    float_target.name_ = "input";
    float_target.dimensions_ = {1, 8, 8, 3};
    float_target.dtype_ = tensor_element_type::float32;
    assert(processor.vqec_vision_ai_ports_imgpr_validate(frame, plan, float_target).code_ ==
           status_code::ok);
    std::vector<tensor_blob> outputs;
    assert(processor.vqec_vision_ai_ports_imgpr_preprocess(
               frame, plan, float_target, outputs).code_ == status_code::ok);
    assert(outputs.size() == 1 && outputs[0].bytes_.size() == 8U * 8U * 3U * 4U);
    float first = 0.0F;
    std::memcpy(&first, outputs[0].bytes_.data(), sizeof(first));
    assert(first > 129.0F && first < 131.0F);

    tensor_spec quantized_target = float_target;
    quantized_target.dtype_ = tensor_element_type::uint16;
    quantized_target.quantization_ = {true, 1.0F, 0};
    std::vector<tensor_blob> quantized_outputs;
    assert(processor.vqec_vision_ai_ports_imgpr_preprocess(
               frame, plan, quantized_target, quantized_outputs).code_ == status_code::ok);
    assert(quantized_outputs.size() == 1 &&
           quantized_outputs[0].bytes_.size() == 8U * 8U * 3U * 2U);
    std::uint16_t stored = 0;
    std::memcpy(&stored, quantized_outputs[0].bytes_.data(), sizeof(stored));
    assert(stored >= 129U && stored <= 131U);

    tensor_spec bad_target = float_target;
    bad_target.dimensions_ = {3, 8, 8};
    assert(processor.vqec_vision_ai_ports_imgpr_validate(frame, plan, bad_target).code_ ==
           status_code::unsupported);
    auto bad_frame = frame;
    bad_frame.native_handle_ = -1;
    assert(processor.vqec_vision_ai_ports_imgpr_validate(
               bad_frame, plan, float_target).code_ == status_code::unsupported);

    (void)::close(fd);
    return 0;
}
