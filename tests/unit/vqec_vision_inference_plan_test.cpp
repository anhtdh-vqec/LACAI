#include <vqec/vision/ai/contracts/vqec_vision_inference_plan.hpp>

#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace vqec::vision::ai {
namespace {

inference_plan vqec_vision_ai_unit_iptst_make_valid_plan() {
    inference_plan plan;
    plan.source_width_ = 3840;
    plan.source_height_ = 2160;
    plan.fps_numerator_ = 25;
    plan.fps_denominator_ = 1;
    plan.tensor_width_ = 640;
    plan.tensor_height_ = 640;
    plan.placement_ = image_placement::centre;
    plan.model_path_ = "/opt/vqec/models/person.bin";
    plan.backend_path_ = "/usr/lib/libQnnHtp.so";
    plan.system_path_ = "/usr/lib/libQnnSystem.so";
    plan.input_queue_bytes_ = 32U * 1024U * 1024U;
    plan.output_queue_buffers_ = 2;
    return plan;
}

void vqec_vision_ai_unit_iptst_require_status(
    const inference_plan& _plan, status_code _expected, const char* _case_name) {
    const auto result = vqec_vision_ai_core_infpl_validate_plan(_plan);
    if (result.code_ != _expected) {
        throw std::runtime_error(std::string(_case_name) + ": " + result.message_);
    }
}

void vqec_vision_ai_unit_iptst_check_valid_plans() {
    auto plan = vqec_vision_ai_unit_iptst_make_valid_plan();
    vqec_vision_ai_unit_iptst_require_status(plan, status_code::ok, "valid UINT8");
    if (vqec_vision_ai_core_infpl_get_packed_frame_bytes(plan) != 12441600ULL) {
        throw std::runtime_error("Incorrect packed 4K NV12 byte count");
    }
    plan.input_type_ = tensor_type::float32;
    plan.channel_order_ = channel_order::bgr;
    plan.mean_ = {0.5, 0.5, 0.5};
    plan.sigma_ = {2.0, 2.0, 2.0};
    plan.model_path_ = "/opt/vqec/models/person.so";
    vqec_vision_ai_unit_iptst_require_status(plan, status_code::ok, "valid FLOAT32");
}

void vqec_vision_ai_unit_iptst_check_geometry() {
    auto plan = vqec_vision_ai_unit_iptst_make_valid_plan();
    plan.source_width_ = 3839;
    vqec_vision_ai_unit_iptst_require_status(
        plan, status_code::invalid_argument, "odd NV12 width");
    plan = vqec_vision_ai_unit_iptst_make_valid_plan();
    plan.source_height_ = std::numeric_limits<std::uint32_t>::max();
    vqec_vision_ai_unit_iptst_require_status(
        plan, status_code::invalid_argument, "unbounded source dimensions");
    plan = vqec_vision_ai_unit_iptst_make_valid_plan();
    plan.tensor_width_ = 0;
    vqec_vision_ai_unit_iptst_require_status(
        plan, status_code::invalid_argument, "zero tensor width");
    plan = vqec_vision_ai_unit_iptst_make_valid_plan();
    plan.fps_denominator_ = 0;
    vqec_vision_ai_unit_iptst_require_status(
        plan, status_code::invalid_argument, "zero rate denominator");
}

void vqec_vision_ai_unit_iptst_check_preprocessing() {
    auto plan = vqec_vision_ai_unit_iptst_make_valid_plan();
    plan.placement_ = image_placement::unspecified;
    vqec_vision_ai_unit_iptst_require_status(
        plan, status_code::invalid_argument, "implicit placement");
    plan = vqec_vision_ai_unit_iptst_make_valid_plan();
    plan.sigma_[0] = std::numeric_limits<double>::quiet_NaN();
    vqec_vision_ai_unit_iptst_require_status(
        plan, status_code::invalid_argument, "NaN coefficient");
    plan = vqec_vision_ai_unit_iptst_make_valid_plan();
    plan.sigma_[0] = 0.0;
    vqec_vision_ai_unit_iptst_require_status(
        plan, status_code::invalid_argument, "zero coefficient");
    plan = vqec_vision_ai_unit_iptst_make_valid_plan();
    plan.mean_[0] = 1.0;
    vqec_vision_ai_unit_iptst_require_status(
        plan, status_code::unsupported, "unverified UINT8 quantization");
    plan = vqec_vision_ai_unit_iptst_make_valid_plan();
    plan.input_type_ = static_cast<tensor_type>(999);
    vqec_vision_ai_unit_iptst_require_status(
        plan, status_code::unsupported, "unknown tensor type");
}

void vqec_vision_ai_unit_iptst_check_paths_and_budgets() {
    auto plan = vqec_vision_ai_unit_iptst_make_valid_plan();
    plan.model_path_ = "person.bin";
    vqec_vision_ai_unit_iptst_require_status(
        plan, status_code::invalid_argument, "relative model path");
    plan.model_path_ = "/opt/../private/person.bin";
    vqec_vision_ai_unit_iptst_require_status(
        plan, status_code::invalid_argument, "traversal path");
    plan.model_path_ = std::string("/opt/model") + '\0' + ".bin";
    vqec_vision_ai_unit_iptst_require_status(
        plan, status_code::invalid_argument, "embedded NUL");
    plan = vqec_vision_ai_unit_iptst_make_valid_plan();
    plan.model_path_ = "/opt/person.tflite";
    vqec_vision_ai_unit_iptst_require_status(
        plan, status_code::invalid_argument, "wrong model format");
    plan = vqec_vision_ai_unit_iptst_make_valid_plan();
    plan.input_queue_bytes_ = 1;
    vqec_vision_ai_unit_iptst_require_status(
        plan, status_code::invalid_argument, "queue below frame size");
    plan = vqec_vision_ai_unit_iptst_make_valid_plan();
    plan.output_queue_buffers_ = 0;
    vqec_vision_ai_unit_iptst_require_status(
        plan, status_code::invalid_argument, "unbounded sink queue");
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    try {
        vqec::vision::ai::vqec_vision_ai_unit_iptst_check_valid_plans();
        vqec::vision::ai::vqec_vision_ai_unit_iptst_check_geometry();
        vqec::vision::ai::vqec_vision_ai_unit_iptst_check_preprocessing();
        vqec::vision::ai::vqec_vision_ai_unit_iptst_check_paths_and_budgets();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
