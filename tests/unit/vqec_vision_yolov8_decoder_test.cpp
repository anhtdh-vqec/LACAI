// Device-free tests for the YOLOv8 decoder: channel-first xywh dequant, inverse letterbox,
// clipping, per-class NMS, threshold and negative cases.

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "vqec_vision_yolov8_decoder.hpp"

using namespace vqec::vision::ai;

namespace {

tensor_blob make_quantized(const std::string& _name, const std::vector<std::uint32_t>& _dims,
    float _scale, std::int32_t _zero_point, const std::vector<float>& _values) {
    tensor_blob blob;
    blob.spec_.name_ = _name;
    blob.spec_.dimensions_ = _dims;
    blob.spec_.dtype_ = tensor_element_type::uint16;
    blob.spec_.quantization_ = {true, _scale, _zero_point};
    blob.bytes_.resize(_values.size() * sizeof(std::uint16_t));
    for (std::size_t index = 0; index < _values.size(); ++index) {
        const float encoded = _values[index] / _scale + static_cast<float>(_zero_point);
        const auto clamped = static_cast<std::int32_t>(std::lround(encoded));
        const auto stored = static_cast<std::uint16_t>(
            clamped < 0 ? 0 : (clamped > 65535 ? 65535 : clamped));
        std::memcpy(blob.bytes_.data() + index * sizeof(std::uint16_t), &stored,
            sizeof(stored));
    }
    return blob;
}

tensor_blob make_float(const std::string& _name, const std::vector<std::uint32_t>& _dims,
    const std::vector<float>& _values) {
    tensor_blob blob;
    blob.spec_.name_ = _name;
    blob.spec_.dimensions_ = _dims;
    blob.spec_.dtype_ = tensor_element_type::float32;
    blob.bytes_.resize(_values.size() * sizeof(float));
    std::memcpy(blob.bytes_.data(), _values.data(), blob.bytes_.size());
    return blob;
}

yolov8_decoder_config make_config(std::uint32_t _sw, std::uint32_t _sh) {
    yolov8_decoder_config config;
    config.source_width_ = _sw;
    config.source_height_ = _sh;
    config.tensor_width_ = 640;
    config.tensor_height_ = 640;
    config.placement_ = image_placement::centre;
    config.box_tensor_ = "boxes_out";
    config.score_tensor_ = "conf_out";
    config.class_count_ = 1;
    config.class_names_ = {"person"};
    config.confidence_threshold_ = 0.25F;
    config.iou_threshold_ = 0.45F;
    config.box_convention_ = coordinate_convention::tensor_pixels_xywh;
    return config;
}

model_outputs make_manifest() {
    model_outputs outputs;
    outputs.model_id_ = "yolov8n_person";
    outputs.model_version_ = "1.0";
    outputs.artifact_sha256_ = std::string(64, 'a');
    outputs.decoder_contract_ = "yolo.v8.detect";
    outputs.max_output_bytes_ = 1U << 22;
    tensor_spec box;
    box.name_ = "boxes_out";
    box.dtype_ = tensor_element_type::uint16;
    tensor_spec score;
    score.name_ = "conf_out";
    score.dtype_ = tensor_element_type::uint16;
    outputs.outputs_ = {box, score};
    return outputs;
}

const observation* find_x(const observation_batch& _batch, float _x) {
    for (const auto& item : _batch.observations_) {
        if (std::abs(item.box_.x_ - _x) < 1.0F) {
            return &item;
        }
    }
    return nullptr;
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    // Channel-first xywh, threshold and NMS in a 1:1 letterbox.
    {
        yolov8_decoder decoder(make_config(640, 640));
        check(decoder.vqec_vision_ai_cntr_mddec_validate(make_manifest()).code_ ==
              status_code::ok);
        // anchors=4: box plane order x[0..3], y[0..3], w[0..3], h[0..3].
        const std::vector<float> boxes{
            100, 104, 500, 300,  // x
            100, 104, 500, 300,  // y
            40, 40, 20, 10,      // w
            40, 40, 20, 10};     // h
        const std::vector<float> scores{0.9F, 0.8F, 0.6F, 0.1F};
        tensor_result result;
        result.tensors_.push_back(make_quantized("boxes_out", {1, 4, 4}, 0.01038312166929245F, 0,
            boxes));
        result.tensors_.push_back(make_quantized("conf_out", {1, 1, 4}, 1.52587890625e-05F, 0,
            scores));
        observation_batch observations;
        check(decoder.vqec_vision_ai_cntr_mddec_decode(
                  result, preview_frame_key{1, 0, 1, 1, 1000}, observations).code_ ==
              status_code::ok);
        check(observations.observations_.size() == 2);
        const auto* first = find_x(observations, 80.0F);
        const auto* second = find_x(observations, 490.0F);
        check(first != nullptr && second != nullptr);
        if (first != nullptr) {
            check(first->class_id_ == "person" && std::abs(first->box_.y_ - 80.0F) < 0.5F &&
                  std::abs(first->box_.width_ - 40.0F) < 0.5F &&
                  std::abs(first->confidence_ - 0.9F) < 0.01F);
        }
    }

    // Inverse letterbox: 640 into 1280x720 adds 140px top/bottom padding.
    {
        yolov8_decoder decoder(make_config(1280, 720));
        const std::vector<float> boxes{320, 320, 100, 100};
        const std::vector<float> scores{0.9F};
        tensor_result result;
        result.tensors_.push_back(make_quantized("boxes_out", {1, 4, 1}, 0.01038312166929245F, 0,
            boxes));
        result.tensors_.push_back(make_quantized("conf_out", {1, 1, 1}, 1.52587890625e-05F, 0,
            scores));
        observation_batch observations;
        check(decoder.vqec_vision_ai_cntr_mddec_decode(
                  result, preview_frame_key{1, 0, 1, 1, 1000}, observations).code_ ==
              status_code::ok);
        check(observations.observations_.size() == 1);
        if (!observations.observations_.empty()) {
            const auto& box = observations.observations_[0].box_;
            check(std::abs(box.x_ - 540.0F) < 1.0F && std::abs(box.y_ - 260.0F) < 1.0F &&
                  std::abs(box.width_ - 200.0F) < 1.0F &&
                  std::abs(box.height_ - 200.0F) < 1.0F);
        }
    }

    // Clipping must remain valid after float subtraction near the right/bottom boundary.
    {
        yolov8_decoder decoder(make_config(1280, 720));
        tensor_result result;
        result.tensors_.push_back(make_float(
            "boxes_out", {1, 4, 1}, {650.15F, 510.15F, 100.0F, 100.0F}));
        result.tensors_.push_back(make_float("conf_out", {1, 1, 1}, {0.9F}));
        observation_batch observations;
        check(decoder.vqec_vision_ai_cntr_mddec_decode(
                  result, preview_frame_key{1, 0, 1, 1, 1000}, observations).code_ ==
              status_code::ok);
        check(vqec_vision_ai_core_obval_validate_detections(
                  observations, observations.frame_, observations.geometry_).code_ ==
              status_code::ok);
    }

    // Negative cases.
    {
        yolov8_decoder decoder(make_config(640, 640));
        // Missing score tensor.
        tensor_result missing;
        missing.tensors_.push_back(make_quantized("boxes_out", {1, 4, 1}, 0.01F, 0,
            {10, 10, 5, 5}));
        observation_batch observations;
        check(decoder.vqec_vision_ai_cntr_mddec_decode(
                  missing, preview_frame_key{1, 0, 1, 1, 1000}, observations).code_ ==
              status_code::invalid_argument);
        // Score length inconsistent with box anchors.
        tensor_result inconsistent;
        inconsistent.tensors_.push_back(make_quantized("boxes_out", {1, 4, 2}, 0.01F, 0,
            {10, 10, 5, 5, 10, 10, 5, 5}));
        inconsistent.tensors_.push_back(make_quantized("conf_out", {1, 1, 3}, 1.5e-05F, 0,
            {0.0F, 0.0F, 0.0F}));
        check(decoder.vqec_vision_ai_cntr_mddec_decode(
                  inconsistent, preview_frame_key{1, 0, 1, 1, 1000}, observations).code_ ==
              status_code::unsupported);
        // Non-quantized uint8 is rejected.
        tensor_blob bad_dtype;
        bad_dtype.spec_.name_ = "boxes_out";
        bad_dtype.spec_.dimensions_ = {1, 4, 1};
        bad_dtype.spec_.dtype_ = tensor_element_type::uint8;
        bad_dtype.bytes_.assign(4, 0);
        tensor_result bad;
        bad.tensors_.push_back(bad_dtype);
        bad.tensors_.push_back(make_quantized("conf_out", {1, 1, 1}, 1.5e-05F, 0, {0.9F}));
        check(decoder.vqec_vision_ai_cntr_mddec_decode(
                  bad, preview_frame_key{1, 0, 1, 1, 1000}, observations).code_ ==
              status_code::unsupported);
        // NaN score (float32) is dropped, not fatal.
        tensor_result nan_result;
        nan_result.tensors_.push_back(make_float("boxes_out", {1, 4, 1}, {10, 10, 5, 5}));
        nan_result.tensors_.push_back(make_float("conf_out", {1, 1, 1},
            {std::numeric_limits<float>::quiet_NaN()}));
        check(decoder.vqec_vision_ai_cntr_mddec_decode(
                  nan_result, preview_frame_key{1, 0, 1, 1, 1000}, observations).code_ ==
              status_code::ok);
        check(observations.observations_.empty());
    }

    // Detection bound fails closed instead of truncating.
    {
        yolov8_decoder decoder(make_config(640, 640));
        const std::size_t anchors = 400;
        std::vector<float> boxes(anchors * 4, 0.0F);
        std::vector<float> scores(anchors, 0.9F);
        for (std::size_t index = 0; index < anchors; ++index) {
            boxes[0 * anchors + index] =
                static_cast<float>(index) * 1.5F + 1.0F;
            boxes[1 * anchors + index] = 1.0F;
            boxes[2 * anchors + index] = 1.0F;
            boxes[3 * anchors + index] = 1.0F;
        }
        tensor_result result;
        result.tensors_.push_back(make_quantized("boxes_out",
            {1, 4, static_cast<std::uint32_t>(anchors)}, 1.0F, 0, boxes));
        result.tensors_.push_back(make_quantized("conf_out",
            {1, 1, static_cast<std::uint32_t>(anchors)}, 1.52587890625e-05F, 0, scores));
        observation_batch observations;
        check(decoder.vqec_vision_ai_cntr_mddec_decode(
                  result, preview_frame_key{1, 0, 1, 1, 1000}, observations).code_ ==
              status_code::resource_exhausted);
    }

    std::cout << "yolov8 decoder failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
