#include <array>
#include <iostream>
#include <vector>

#include "vqec_vision_tensor_output.hpp"

int main() {
    using vqec::vision::ai::float_tensor_spec;
    using vqec::vision::ai::status_code;
    using vqec::vision::ai::tensor_result;
    using vqec::vision::ai::vqec_vision_ai_qcom_tnout_copy_sample;
    gst_init(nullptr, nullptr);
    auto* caps = gst_caps_from_string(
        "neural-network/tensors,type=(string)FLOAT32,dimensions=<<(int)1,(int)4>>");
    if (caps == nullptr) {
        return 1;
    }
    auto* buffer = gst_buffer_new_allocate(nullptr, 16, nullptr);
    if (buffer == nullptr) {
        gst_caps_unref(caps);
        return 1;
    }
    const std::array<float, 4> values{1.0F, -2.0F, 3.5F, 0.0F};
    gst_buffer_fill(buffer, 0, values.data(), 16);
    GST_BUFFER_PTS(buffer) = 1234;
    auto* sample = gst_sample_new(buffer, caps, nullptr, nullptr);
    gst_buffer_unref(buffer);
    gst_caps_unref(caps);
    if (sample == nullptr) {
        return 1;
    }
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    std::vector<float_tensor_spec> expected{{"scores", {1, 4}}};
    tensor_result result;
    check(vqec_vision_ai_qcom_tnout_copy_sample(sample, expected, 16, result).code_ ==
          status_code::ok);
    check(result.pipeline_pts_ns_ == 1234 && result.tensors_.size() == 1);
    if (result.tensors_.size() == 1) {
        check(result.tensors_[0].values_ == std::vector<float>(values.begin(), values.end()));
    }
    check(vqec_vision_ai_qcom_tnout_copy_sample(sample, expected, 15, result).code_ ==
          status_code::resource_exhausted);
    expected[0].dimensions_ = {1, 5};
    check(vqec_vision_ai_qcom_tnout_copy_sample(sample, expected, 64, result).code_ ==
          status_code::protocol_error);
    check(result.pipeline_pts_ns_ == 1234 && result.tensors_.size() == 1);
    expected[0].dimensions_ = {1, 4};
    expected.push_back({"second", {1, 4}});
    check(vqec_vision_ai_qcom_tnout_copy_sample(sample, expected, 64, result).code_ ==
          status_code::unsupported);
    check(vqec_vision_ai_qcom_tnout_copy_sample(nullptr, expected, 64, result).code_ ==
          status_code::invalid_argument);
    gst_sample_unref(sample);
    // Copied output remains usable after the vendor sample and memory are gone.
    if (result.tensors_.size() == 1) {
        check(result.tensors_[0].values_.size() == 4 && result.tensors_[0].values_[1] == -2.0F);
    }
    std::cout << "tensor output failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
