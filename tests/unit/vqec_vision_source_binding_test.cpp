#include <iostream>

#include "vqec/vision/ai/contracts/vqec_vision_source_binding.hpp"

int main() {
    using namespace vqec::vision::ai;
    inference_plan plan;
    plan.source_width_ = 1920;
    plan.source_height_ = 1080;
    plan.fps_numerator_ = 25;
    plan.fps_denominator_ = 1;
    plan.tensor_width_ = 640;
    plan.tensor_height_ = 640;
    plan.placement_ = image_placement::centre;
    plan.model_path_ = "/opt/models/test.bin";
    plan.backend_path_ = "/usr/lib/libQnnHtp.so";
    plan.system_path_ = "/usr/lib/libQnnSystem.so";
    plan.input_queue_bytes_ = 8U * 1024U * 1024U;
    plan.output_queue_buffers_ = 2;
    source_binding baseline;
    baseline.width_ = 1920;
    baseline.height_ = 1080;
    baseline.fps_numerator_ = 25;
    baseline.fps_denominator_ = 1;
    baseline.memory_kind_ = source_memory_kind::dmabuf;
    baseline.layout_ = source_memory_layout::linear_nv12;
    baseline.sync_mode_ = source_sync_mode::implicit_ready;
    baseline.color_profile_ = source_color_profile::bt709_limited;
    baseline.chroma_site_ = source_chroma_site::mpeg2;
    // Synthetic references only, never deployment approval.
    baseline.fw_memory_contract_ = "fixture:fw";
    baseline.backend_memory_contract_ = "fixture:backend";
    baseline.preprocess_contract_ = "fixture:golden";
    unsigned failures = 0;
    const auto check = [&](const source_binding& _binding, status_code _expected) {
        const auto result = vqec_vision_ai_core_srcbd_validate_binding(_binding, plan);
        if (result.code_ != _expected) {
            ++failures;
            std::cerr << result.message_ << '\n';
        }
    };
    check(baseline, status_code::ok);
    check({}, status_code::invalid_argument);
    auto candidate = baseline;
    candidate.fps_numerator_ = 50;
    candidate.fps_denominator_ = 2;
    check(candidate, status_code::ok);
    candidate.fps_denominator_ = 0;
    check(candidate, status_code::invalid_argument);
    candidate = baseline;
    candidate.width_ = 3840;
    check(candidate, status_code::invalid_argument);
    candidate = baseline;
    candidate.fps_numerator_ = UINT32_MAX;
    candidate.fps_denominator_ = UINT32_MAX;
    check(candidate, status_code::invalid_argument);
    candidate = baseline;
    candidate.memory_kind_ = source_memory_kind::opaque_fd;
    check(candidate, status_code::unsupported);
    candidate = baseline;
    candidate.layout_ = source_memory_layout::ubwc;
    check(candidate, status_code::unsupported);
    candidate = baseline;
    candidate.sync_mode_ = source_sync_mode::explicit_fence;
    check(candidate, status_code::unsupported);
    candidate = baseline;
    candidate.color_profile_ = source_color_profile::unspecified;
    check(candidate, status_code::unsupported);
    candidate = baseline;
    candidate.chroma_site_ = static_cast<source_chroma_site>(999);
    check(candidate, status_code::unsupported);
    candidate = baseline;
    candidate.color_profile_ = source_color_profile::bt601_limited;
    candidate.chroma_site_ = source_chroma_site::jpeg;
    check(candidate, status_code::ok);
    candidate = baseline;
    candidate.fw_memory_contract_.clear();
    check(candidate, status_code::invalid_argument);
    candidate = baseline;
    candidate.backend_memory_contract_ = std::string(257, 'x');
    check(candidate, status_code::invalid_argument);
    candidate.backend_memory_contract_ = std::string(256, 'x');
    check(candidate, status_code::ok);
    candidate = baseline;
    candidate.preprocess_contract_ = std::string("fixture\0bad", 11);
    check(candidate, status_code::invalid_argument);
    candidate.preprocess_contract_ = " ";
    check(candidate, status_code::invalid_argument);
    plan.tensor_width_ = 0;
    check(baseline, status_code::invalid_argument);
    std::cout << "source binding failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
