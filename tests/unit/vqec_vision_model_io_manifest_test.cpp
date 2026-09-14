// Device-free tests for the symmetric model IO manifest: validation and fail-closed
// declared-versus-actual identity matching (name/shape/dtype/layout/quantization).

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_model_io_manifest.hpp"

using namespace vqec::vision::ai;

namespace {

tensor_spec input_spec() {
    tensor_spec spec;
    spec.name_ = "images";
    spec.dimensions_ = {1, 640, 640, 3};
    spec.dtype_ = tensor_element_type::uint16;
    spec.layout_ = tensor_layout::nhwc;
    spec.quantization_ = {true, 1.5259021893143654e-05F, 0};
    return spec;
}

tensor_spec output_spec() {
    tensor_spec spec;
    spec.name_ = "boxes_out";
    spec.dimensions_ = {1, 4, 8400};
    spec.dtype_ = tensor_element_type::uint16;
    spec.layout_ = tensor_layout::flat;
    spec.quantization_ = {true, 0.01038312166929245F, 0};
    return spec;
}

model_io_manifest make_manifest() {
    model_io_manifest manifest;
    manifest.inputs_ = {input_spec()};
    manifest.outputs_ = {output_spec()};
    return manifest;
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    const auto manifest = make_manifest();
    check(vqec_vision_ai_core_ioman_validate(manifest).code_ == status_code::ok);
    check(vqec_vision_ai_core_ioman_matches(manifest, manifest).code_ == status_code::ok);

    // Structural rejections.
    check(vqec_vision_ai_core_ioman_validate(
              [] { auto m = make_manifest(); m.inputs_.clear(); return m; }()).code_ ==
          status_code::invalid_argument);
    check(vqec_vision_ai_core_ioman_validate(
              [] { auto m = make_manifest(); m.outputs_.clear(); return m; }()).code_ ==
          status_code::invalid_argument);
    check(vqec_vision_ai_core_ioman_validate(
              [] { auto m = make_manifest(); m.inputs_[0].dimensions_[1] = 0; return m; }()).code_ ==
          status_code::invalid_argument);
    check(vqec_vision_ai_core_ioman_validate(
              [] { auto m = make_manifest(); m.inputs_.push_back(m.inputs_[0]); return m; }()).code_ ==
          status_code::invalid_argument);
    check(vqec_vision_ai_core_ioman_validate(
              [] { auto m = make_manifest(); m.inputs_[0].quantization_.scale_ = 0.0F; return m; }())
              .code_ == status_code::invalid_argument);
    check(vqec_vision_ai_core_ioman_validate(
              [] { auto m = make_manifest(); m.inputs_[0].dtype_ = tensor_element_type::float32;
                   m.inputs_[0].quantization_ = {true, 1.0F, 0}; return m; }())
              .code_ == status_code::invalid_argument);

    // Declared-versus-actual fail-closed identity checks.
    check(vqec_vision_ai_core_ioman_matches(
              manifest, [] { auto a = make_manifest(); a.inputs_[0].name_ = "input_0"; return a; }())
              .code_ == status_code::unsupported);
    check(vqec_vision_ai_core_ioman_matches(
              manifest, [] { auto a = make_manifest();
                   a.inputs_[0].quantization_.scale_ = 0.0041F; return a; }()).code_ ==
          status_code::unsupported);
    check(vqec_vision_ai_core_ioman_matches(
              manifest, [] { auto a = make_manifest();
                   a.inputs_[0].layout_ = tensor_layout::nchw; return a; }()).code_ ==
          status_code::unsupported);
    check(vqec_vision_ai_core_ioman_matches(
              manifest, [] { auto a = make_manifest(); a.outputs_.clear(); return a; }()).code_ ==
          status_code::unsupported);

    std::cout << "model IO manifest failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
