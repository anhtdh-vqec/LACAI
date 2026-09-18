#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include "vqec_vision_tensor_output.hpp"

int main() {
    using vqec::vision::ai::status_code;
    using vqec::vision::ai::tensor_element_type;
    using vqec::vision::ai::tensor_result;
    using vqec::vision::ai::tensor_spec;
    using vqec::vision::ai::vqec_vision_ai_qcom_tnout_copy_sample;
    gst_init(nullptr, nullptr);
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    // FLOAT32 path: bytes are copied verbatim and survive vendor object release.
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
    tensor_spec scores;
    scores.name_ = "scores";
    scores.dimensions_ = {1, 4};
    scores.dtype_ = tensor_element_type::float32;
    std::vector<tensor_spec> expected{scores};
    tensor_result result;
    check(vqec_vision_ai_qcom_tnout_copy_sample(sample, expected, 16, result).code_ ==
          status_code::ok);
    check(result.pipeline_pts_ns_ == 1234 && result.tensors_.size() == 1);
    if (result.tensors_.size() == 1) {
        check(result.tensors_[0].bytes_.size() == 16);
        std::array<float, 4> copied{};
        if (result.tensors_[0].bytes_.size() == sizeof(copied)) {
            std::memcpy(copied.data(), result.tensors_[0].bytes_.data(), sizeof(copied));
            check(copied == values);
        }
        check(result.tensors_[0].spec_.dtype_ == tensor_element_type::float32);
    }
    check(vqec_vision_ai_qcom_tnout_copy_sample(sample, expected, 15, result).code_ ==
          status_code::resource_exhausted);
    expected[0].dimensions_ = {1, 5};
    check(vqec_vision_ai_qcom_tnout_copy_sample(sample, expected, 64, result).code_ ==
          status_code::protocol_error);
    check(result.pipeline_pts_ns_ == 1234 && result.tensors_.size() == 1);
    expected[0].dimensions_ = {1, 4};
    tensor_spec second = scores;
    second.name_ = "second";
    expected.push_back(second);
    check(vqec_vision_ai_qcom_tnout_copy_sample(sample, expected, 64, result).code_ ==
          status_code::unsupported);
    check(vqec_vision_ai_qcom_tnout_copy_sample(nullptr, expected, 64, result).code_ ==
          status_code::invalid_argument);
    gst_sample_unref(sample);
    // Copied output remains usable after the vendor sample and memory are gone.
    if (result.tensors_.size() == 1) {
        check(result.tensors_[0].bytes_.size() == 16);
    }

    // INT8 quantized path: the same extractor carries a non-float element type.
    auto* int8_caps = gst_caps_from_string(
        "neural-network/tensors,type=(string)INT8,dimensions=<<(int)1,(int)4>>");
    auto* int8_buffer = gst_buffer_new_allocate(nullptr, 4, nullptr);
    if (int8_caps != nullptr && int8_buffer != nullptr) {
        const std::array<std::int8_t, 4> quantized{1, 2, 3, 4};
        gst_buffer_fill(int8_buffer, 0, quantized.data(), 4);
        auto* int8_sample = gst_sample_new(int8_buffer, int8_caps, nullptr, nullptr);
        gst_buffer_unref(int8_buffer);
        gst_caps_unref(int8_caps);
        if (int8_sample != nullptr) {
            tensor_spec q;
            q.name_ = "q";
            q.dimensions_ = {1, 4};
            q.dtype_ = tensor_element_type::int8;
            q.quantization_ = {true, 0.5F, -1};
            tensor_result quantized_result;
            check(vqec_vision_ai_qcom_tnout_copy_sample(
                      int8_sample, {q}, 4, quantized_result).code_ == status_code::ok);
            check(quantized_result.tensors_.size() == 1 &&
                  quantized_result.tensors_[0].bytes_.size() == 4 &&
                  quantized_result.tensors_[0].spec_.dtype_ == tensor_element_type::int8 &&
                  quantized_result.tensors_[0].spec_.quantization_.is_quantized_);
            // A model contract that claims FLOAT32 for an INT8 caps stream is rejected.
            tensor_spec mismatched = q;
            mismatched.dtype_ = tensor_element_type::float32;
            mismatched.quantization_ = {};
            tensor_result mismatched_result;
            check(vqec_vision_ai_qcom_tnout_copy_sample(
                      int8_sample, {mismatched}, 16, mismatched_result).code_ ==
                  status_code::unsupported);
            gst_sample_unref(int8_sample);
        }
    }
    std::cout << "tensor output failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
