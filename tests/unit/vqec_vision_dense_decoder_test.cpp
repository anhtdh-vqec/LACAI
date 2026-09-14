// Device-free tests for the dense anchor-free detector decoder: manifest validation,
// thresholding, per-class NMS, letterbox inverse mapping, clipping and the detection bound.

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "vqec_vision_dense_decoder.hpp"

using namespace vqec::vision::ai;

namespace {

tensor_blob make_blob(const std::string& _name, const std::vector<std::uint32_t>& _dims,
    const std::vector<float>& _values) {
    tensor_blob blob;
    blob.spec_.name_ = _name;
    blob.spec_.dimensions_ = _dims;
    blob.spec_.dtype_ = tensor_element_type::float32;
    blob.bytes_.resize(_values.size() * sizeof(float));
    std::memcpy(blob.bytes_.data(), _values.data(), blob.bytes_.size());
    return blob;
}

model_outputs make_manifest(const std::vector<std::string>& _names) {
    model_outputs outputs;
    outputs.model_id_ = "model";
    outputs.model_version_ = "1";
    outputs.artifact_sha256_ = std::string(64, 'a');
    outputs.decoder_contract_ = "dense.v1";
    outputs.max_output_bytes_ = 1U << 20;
    for (const auto& name : _names) {
        tensor_spec spec;
        spec.name_ = name;
        spec.dtype_ = tensor_element_type::float32;
        outputs.outputs_.push_back(spec);
    }
    return outputs;
}

dense_decoder_config make_config(std::uint32_t _sw, std::uint32_t _sh, std::uint32_t _tw,
    std::uint32_t _th, std::uint32_t _grid, std::size_t _classes) {
    dense_decoder_config config;
    config.source_width_ = _sw;
    config.source_height_ = _sh;
    config.tensor_width_ = _tw;
    config.tensor_height_ = _th;
    config.placement_ = image_placement::centre;
    config.class_count_ = _classes;
    config.confidence_threshold_ = 0.5F;
    config.iou_threshold_ = 0.5F;
    dense_decoder_stage stage;
    stage.box_tensor_ = "boxes";
    stage.score_tensor_ = "scores";
    stage.grid_width_ = _grid;
    stage.grid_height_ = _grid;
    stage.stride_ = _tw / _grid;
    config.stages_.push_back(std::move(stage));
    return config;
}

const observation* find_observation(const observation_batch& _batch,
    const std::string& _class_id) {
    for (const auto& item : _batch.observations_) {
        if (item.class_id_ == _class_id) {
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

    // Manifest validation.
    {
        dense_decoder decoder(make_config(64, 64, 64, 64, 2, 2));
        check(decoder.vqec_vision_ai_cntr_mddec_validate(
                  make_manifest({"boxes", "scores"})).code_ == status_code::ok);
        check(decoder.vqec_vision_ai_cntr_mddec_validate(
                  make_manifest({"boxes"})).code_ == status_code::unsupported);
    }

    // Thresholding + per-class NMS.
    {
        dense_decoder decoder(make_config(64, 64, 64, 64, 2, 2));
        std::vector<float> boxes{
            16, 16, 16, 16,  // cell 0, class 0 score 0.9
            18, 18, 16, 16,  // cell 1, overlapping cell 0
            48, 48, 16, 16,  // cell 2, class 1 score 0.95
            12, 12, 8, 8};   // cell 3, low score
        std::vector<float> scores{
            0.9F, 0.1F,
            0.8F, 0.1F,
            0.05F, 0.95F,
            0.2F, 0.2F};
        tensor_result result;
        result.tensors_.push_back(make_blob("boxes", {1, 2, 2, 4}, boxes));
        result.tensors_.push_back(make_blob("scores", {1, 2, 2, 2}, scores));
        observation_batch observations;
        check(decoder.vqec_vision_ai_cntr_mddec_decode(
                  result, preview_frame_key{1, 0, 1, 1, 1000}, observations).code_ ==
              status_code::ok);
        check(observations.observations_.size() == 2);
        const auto* class0 = find_observation(observations, "0");
        const auto* class1 = find_observation(observations, "1");
        check(class0 != nullptr && class1 != nullptr);
        if (class0 != nullptr) {
            check(class0->confidence_ > 0.89F && class0->box_.x_ == 8.0F &&
                  class0->box_.y_ == 8.0F && class0->box_.width_ == 16.0F);
        }
        if (class1 != nullptr) {
            check(class1->confidence_ > 0.94F && class1->box_.x_ == 40.0F);
        }
    }

    // Letterbox inverse mapping: source 32x64 into 64x64 adds 16px left/right padding.
    {
        dense_decoder decoder(make_config(32, 64, 64, 64, 1, 1));
        std::vector<float> boxes{32, 32, 16, 16};
        std::vector<float> scores{0.9F};
        tensor_result result;
        result.tensors_.push_back(make_blob("boxes", {1, 1, 1, 4}, boxes));
        result.tensors_.push_back(make_blob("scores", {1, 1, 1, 1}, scores));
        observation_batch observations;
        check(decoder.vqec_vision_ai_cntr_mddec_decode(
                  result, preview_frame_key{1, 0, 1, 1, 1000}, observations).code_ ==
              status_code::ok);
        check(observations.observations_.size() == 1);
        if (!observations.observations_.empty()) {
            const auto& box = observations.observations_[0].box_;
            check(box.x_ == 8.0F && box.y_ == 24.0F && box.width_ == 16.0F &&
                  box.height_ == 16.0F);
        }
    }

    // Wrong dtype is rejected, not reinterpreted.
    {
        dense_decoder decoder(make_config(64, 64, 64, 64, 1, 1));
        tensor_result result;
        result.tensors_.push_back(make_blob("boxes", {1, 1, 1, 4}, {16, 16, 8, 8}));
        auto scores = make_blob("scores", {1, 1, 1, 1}, {0.9F});
        scores.spec_.dtype_ = tensor_element_type::uint8;
        result.tensors_.push_back(std::move(scores));
        observation_batch observations;
        check(decoder.vqec_vision_ai_cntr_mddec_decode(
                  result, preview_frame_key{1, 0, 1, 1, 1000}, observations).code_ ==
              status_code::unsupported);
    }

    // Detection bound fails closed instead of silently truncating.
    {
        dense_decoder_config config = make_config(64, 64, 64, 64, 20, 1);
        config.confidence_threshold_ = 0.5F;
        config.iou_threshold_ = 0.9F;
        dense_decoder decoder(config);
        std::vector<float> boxes;
        std::vector<float> scores;
        for (std::uint32_t cell = 0; cell < 400; ++cell) {
            const float cx = static_cast<float>((cell % 20) * 3) + 1.5F;
            const float cy = static_cast<float>((cell / 20) * 3) + 1.5F;
            boxes.insert(boxes.end(), {cx, cy, 1.0F, 1.0F});
            scores.push_back(0.9F);
        }
        tensor_result result;
        result.tensors_.push_back(make_blob("boxes", {1, 20, 20, 4}, boxes));
        result.tensors_.push_back(make_blob("scores", {1, 20, 20, 1}, scores));
        observation_batch observations;
        check(decoder.vqec_vision_ai_cntr_mddec_decode(
                  result, preview_frame_key{1, 0, 1, 1, 1000}, observations).code_ ==
              status_code::resource_exhausted);
    }

    std::cout << "dense decoder failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
