// Device-free tests for the model package resolver: catalog identity, graph agreement,
// single-input base, preprocess validity and trusted-path identity.

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/inference/vqec_vision_model_package.hpp"

using namespace vqec::vision::ai;

namespace {

model_catalog_entry make_model() {
    model_catalog_entry model;
    model.model_id_ = "yolov8n_person";
    model.model_version_ = "1.0";
    model.target_id_ = "qcs6490";
    model.artifact_ref_ = "yolov8n_person_w8a16.so";
    model.artifact_sha256_ = std::string(64, 'a');
    model.decoder_contract_ = "yolo.v8.detect";
    model.graph_name_ = "yolov8n_person_w8a16";
    return model;
}

resolved_model_paths make_paths() {
    resolved_model_paths paths;
    paths.model_id_ = "yolov8n_person";
    paths.target_id_ = "qcs6490";
    paths.artifact_ref_ = "yolov8n_person_w8a16.so";
    paths.model_path_ = "/run/models/yolov8n_person_w8a16.so";
    paths.backend_path_ = "/usr/lib/libQnnHtp.so";
    paths.system_path_ = "/usr/lib/libQnnSystem.so";
    return paths;
}

model_io_manifest make_io() {
    model_io_manifest io;
    tensor_spec input;
    input.name_ = "images";
    input.dimensions_ = {1, 640, 640, 3};
    input.dtype_ = tensor_element_type::uint16;
    input.layout_ = tensor_layout::nhwc;
    input.quantization_ = {true, 1.5259021893143654e-05F, 0};
    tensor_spec output;
    output.name_ = "boxes_out";
    output.dimensions_ = {1, 4, 8400};
    output.dtype_ = tensor_element_type::uint16;
    output.layout_ = tensor_layout::flat;
    output.quantization_ = {true, 0.01038312166929245F, 0};
    io.inputs_ = {input};
    io.outputs_ = {output};
    return io;
}

preprocess_spec make_preprocess() {
    preprocess_spec spec;
    spec.source_format_ = source_pixel_format::nv12;
    spec.matrix_ = color_matrix::bt709;
    spec.range_ = color_range::limited;
    spec.resize_ = resize_mode::letterbox;
    spec.interpolation_ = interpolation_mode::bilinear;
    spec.placement_ = image_placement::centre;
    spec.pad_value_ = {114.0F, 114.0F, 114.0F};
    spec.channels_ = channel_order::rgb;
    spec.normalization_ = normalization_formula::offset_scale;
    spec.offset_ = {0.0F, 0.0F, 0.0F};
    spec.scale_ = {1.0F / 255.0F, 1.0F / 255.0F, 1.0F / 255.0F};
    spec.coordinates_ = coordinate_convention::tensor_pixels_xywh;
    return spec;
}

model_package_inputs make_inputs() {
    model_package_inputs inputs;
    inputs.model_ = make_model();
    inputs.graph_name_ = "yolov8n_person_w8a16";
    inputs.paths_ = make_paths();
    inputs.io_ = make_io();
    inputs.preprocess_ = make_preprocess();
    inputs.class_labels_ = {"person"};
    return inputs;
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    resolved_model_package package;
    check(vqec_vision_ai_core_mpkg_resolve(make_inputs(), package).code_ == status_code::ok);
    check(package.model_id_ == "yolov8n_person" && package.graph_name_ == "yolov8n_person_w8a16" &&
          package.decoder_contract_ == "yolo.v8.detect" && package.class_labels_.size() == 1 &&
          package.paths_.model_path_ == "/run/models/yolov8n_person_w8a16.so");

    // Trusted-path identity mismatch.
    check(vqec_vision_ai_core_mpkg_resolve(
              [] { auto i = make_inputs(); i.paths_.artifact_ref_ = "other.so"; return i; }(),
              package).code_ == status_code::invalid_argument);
    check(vqec_vision_ai_core_mpkg_resolve(
              [] { auto i = make_inputs(); i.paths_.backend_path_.clear(); return i; }(),
              package).code_ == status_code::invalid_argument);
    // Graph agreement.
    check(vqec_vision_ai_core_mpkg_resolve(
              [] { auto i = make_inputs(); i.graph_name_ = "other_graph"; return i; }(),
              package).code_ == status_code::unsupported);
    // Single input base.
    check(vqec_vision_ai_core_mpkg_resolve(
              [] { auto i = make_inputs(); auto second = i.io_.inputs_[0];
                   second.name_ = "images2"; i.io_.inputs_.push_back(second); return i; }(),
              package).code_ == status_code::unsupported);
    // Preprocess validity.
    check(vqec_vision_ai_core_mpkg_resolve(
              [] { auto i = make_inputs(); i.preprocess_.matrix_ = color_matrix::unspecified;
                   return i; }(),
              package).code_ == status_code::invalid_argument);
    // Catalog identity.
    check(vqec_vision_ai_core_mpkg_resolve(
              [] { auto i = make_inputs(); i.model_.artifact_sha256_ = "short"; return i; }(),
              package).code_ == status_code::invalid_argument);

    std::cout << "model package failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
