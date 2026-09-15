#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "vqec_vision_anchor_distance_decoder.hpp"

namespace {

using namespace vqec::vision::ai;

tensor_blob vqec_vision_ai_unit_addtst_make_tensor(
    const std::string& _name, const std::vector<std::uint32_t>& _shape,
    const std::vector<float>& _values) {
    constexpr float g_scale = 0.01F;
    tensor_blob blob;
    blob.spec_.name_ = _name;
    blob.spec_.dimensions_ = _shape;
    blob.spec_.dtype_ = tensor_element_type::uint16;
    blob.spec_.quantization_ = {true, g_scale, 0};
    blob.bytes_.resize(_values.size() * sizeof(std::uint16_t));
    for (std::size_t index = 0; index < _values.size(); ++index) {
        const auto stored = static_cast<std::uint16_t>(std::lround(_values[index] / g_scale));
        std::memcpy(blob.bytes_.data() + index * sizeof(stored), &stored, sizeof(stored));
    }
    return blob;
}

anchor_distance_decoder_config vqec_vision_ai_unit_addtst_make_config() {
    anchor_distance_decoder_config config;
    config.source_width_ = 16;
    config.source_height_ = 8;
    config.tensor_width_ = 16;
    config.tensor_height_ = 8;
    config.placement_ = image_placement::centre;
    config.class_id_ = "face";
    config.landmark_schema_id_ = "human.face.landmarks5";
    config.landmark_schema_version_ = "1";
    config.landmark_count_ = 5;
    config.anchor_offset_cells_ = 0.5F;
    config.confidence_threshold_ = 0.5F;
    config.iou_threshold_ = 0.4F;
    config.max_candidates_ = 8;
    config.stages_.push_back({"score_8", "bbox_8", "kps_8", 8, 2, 1, 2});
    return config;
}

model_outputs vqec_vision_ai_unit_addtst_make_outputs() {
    model_outputs outputs;
    outputs.model_id_ = "face_detector";
    outputs.model_version_ = "1";
    outputs.artifact_sha256_ = std::string(64, 'a');
    outputs.decoder_contract_ = "anchor_distance.detect.v1";
    outputs.outputs_ = {
        {"score_8", {1, 4, 1}, tensor_element_type::uint16, {true, 0.01F, 0}},
        {"bbox_8", {1, 4, 4}, tensor_element_type::uint16, {true, 0.01F, 0}},
        {"kps_8", {1, 4, 10}, tensor_element_type::uint16, {true, 0.01F, 0}}};
    return outputs;
}

}  // namespace

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    anchor_distance_decoder decoder(vqec_vision_ai_unit_addtst_make_config());
    auto outputs = vqec_vision_ai_unit_addtst_make_outputs();
    check(decoder.vqec_vision_ai_cntr_mddec_validate(outputs).code_ == status_code::ok);
    outputs.outputs_[2].dimensions_[2] = 8;
    check(decoder.vqec_vision_ai_cntr_mddec_validate(outputs).code_ ==
        status_code::unsupported);

    tensor_result tensors;
    tensors.tensors_.push_back(vqec_vision_ai_unit_addtst_make_tensor(
        "score_8", {1, 4, 1}, {0.9F, 0.8F, 0.1F, 0.1F}));
    tensors.tensors_.push_back(vqec_vision_ai_unit_addtst_make_tensor(
        "bbox_8", {1, 4, 4}, {
            0.5F, 0.5F, 0.5F, 0.5F,
            0.5F, 0.5F, 0.5F, 0.5F,
            0.5F, 0.5F, 0.5F, 0.5F,
            0.5F, 0.5F, 0.5F, 0.5F}));
    std::vector<float> landmarks(40, 0.0F);
    tensors.tensors_.push_back(vqec_vision_ai_unit_addtst_make_tensor(
        "kps_8", {1, 4, 10}, landmarks));
    const preview_frame_key frame{0, 0, 1, 1, 10};
    observation_batch observations;
    check(decoder.vqec_vision_ai_cntr_mddec_decode(
              tensors, frame, observations).code_ == status_code::ok);
    check(observations.observations_.size() == 1);
    if (!observations.observations_.empty()) {
        const auto& face = observations.observations_.front();
        check(face.class_id_ == "face" && face.landmarks_.points_.size() == 5);
        check(std::abs(face.box_.x_) < 0.01F && std::abs(face.box_.y_) < 0.01F &&
            std::abs(face.box_.width_ - 8.0F) < 0.01F &&
            std::abs(face.box_.height_ - 8.0F) < 0.01F);
        check(std::abs(face.landmarks_.points_[0].x_ - 4.0F) < 0.01F &&
            std::abs(face.landmarks_.points_[0].y_ - 4.0F) < 0.01F);
    }
    // The source is twice the tensor size: stretch must restore both box and landmarks.
    auto stretch_config = vqec_vision_ai_unit_addtst_make_config();
    stretch_config.placement_ = image_placement::stretch;
    stretch_config.source_width_ *= 2;
    stretch_config.source_height_ *= 2;
    anchor_distance_decoder stretch_decoder(stretch_config);
    observation_batch stretched;
    check(stretch_decoder.vqec_vision_ai_cntr_mddec_decode(
        tensors, frame, stretched).code_ == status_code::ok);
    check(stretched.observations_.size() == 1);
    if (stretched.observations_.size() == 1) {
        check(std::abs(stretched.observations_[0].box_.width_ - 16.0F) < 0.01F);
        check(std::abs(stretched.observations_[0].landmarks_.points_[0].x_ - 8.0F) < 0.01F);
        check(std::abs(stretched.observations_[0].landmarks_.points_[0].y_ - 8.0F) < 0.01F);
    }
    // A malformed score must fail atomically, preserving the previously decoded batch.
    auto saved_score = tensors.tensors_.front();
    auto& score = tensors.tensors_.front();
    score.spec_.dtype_ = tensor_element_type::float32;
    score.spec_.quantization_ = {};
    const float invalid_score = std::numeric_limits<float>::quiet_NaN();
    score.bytes_.resize(4 * sizeof(float));
    std::memcpy(score.bytes_.data(), &invalid_score, sizeof(float));
    check(decoder.vqec_vision_ai_cntr_mddec_decode(
        tensors, frame, observations).code_ == status_code::protocol_error);
    check(observations.observations_.size() == 1);
    tensors.tensors_.front() = saved_score;
    auto bounded_config = vqec_vision_ai_unit_addtst_make_config();
    bounded_config.max_candidates_ = 1;
    anchor_distance_decoder bounded_decoder(bounded_config);
    check(bounded_decoder.vqec_vision_ai_cntr_mddec_decode(
        tensors, frame, observations).code_ == status_code::resource_exhausted);
    check(observations.observations_.size() == 1);
    auto invalid_config = vqec_vision_ai_unit_addtst_make_config();
    invalid_config.stages_[0].anchors_per_cell_ = 0;
    anchor_distance_decoder invalid_decoder(invalid_config);
    check(invalid_decoder.vqec_vision_ai_cntr_mddec_decode(
        tensors, frame, observations).code_ == status_code::invalid_argument);
    tensors.tensors_.pop_back();
    check(decoder.vqec_vision_ai_cntr_mddec_decode(
              tensors, frame, observations).code_ == status_code::invalid_argument);
    std::cout << "anchor-distance decoder failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
