#include <iostream>

#include "vqec/vision/ai/contracts/vqec_vision_observation.hpp"

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    const preview_frame_key frame{0, 0, 1, 10, 20};
    const preview_geometry geometry{640, 360};
    observation_batch batch{frame, geometry, {
        {frame, 1, "person", {10, 20, 30, 40, 0xffffffff, "person"}, 0.9F,
            observation_quality::high, {{"human.age", "1", "adult", 0.8F,
                observation_quality::medium, 20, 100}}}}};
    check(vqec_vision_ai_core_obval_validate_batch(batch, frame, geometry).code_ ==
        status_code::ok);
    batch.observations_[0].confidence_ = 1.1F;
    check(vqec_vision_ai_core_obval_validate_batch(batch, frame, geometry).code_ ==
        status_code::invalid_argument);
    batch.observations_[0].confidence_ = 0.9F;
    batch.observations_[0].attributes_[0].schema_id_ = "human age";
    check(vqec_vision_ai_core_obval_validate_batch(batch, frame, geometry).code_ ==
        status_code::invalid_argument);
    batch.observations_[0].attributes_[0].schema_id_ = "human.age";
    batch.observations_[0].attributes_[0].expires_at_ns_ = 19;
    check(vqec_vision_ai_core_obval_validate_batch(batch, frame, geometry).code_ ==
        status_code::invalid_argument);
    batch.observations_[0].attributes_[0].expires_at_ns_ = 100;
    batch.observations_[0].attributes_[0].value_ =
        std::string(observation_limits::g_max_attribute_value_bytes + 1, 'x');
    check(vqec_vision_ai_core_obval_validate_batch(batch, frame, geometry).code_ ==
        status_code::invalid_argument);
    batch.observations_[0].attributes_[0].value_ = "adult";
    batch.observations_[0].attributes_[0].expires_at_ns_ = UINT64_MAX;
    check(vqec_vision_ai_core_obval_validate_batch(batch, frame, geometry).code_ ==
        status_code::invalid_argument);
    batch.observations_[0].attributes_[0].expires_at_ns_ = 100;
    batch.observations_[0].attributes_.push_back(batch.observations_[0].attributes_[0]);
    check(vqec_vision_ai_core_obval_validate_batch(batch, frame, geometry).code_ ==
        status_code::invalid_argument);
    batch.observations_[0].attributes_.pop_back();
    batch.observations_[0].box_.width_ = 1000;
    check(vqec_vision_ai_core_obval_validate_batch(batch, frame, geometry).code_ ==
        status_code::invalid_argument);
    batch.observations_[0].box_.width_ = 30;
    batch.observations_[0].frame_.frame_id_++;
    check(vqec_vision_ai_core_obval_validate_batch(batch, frame, geometry).code_ ==
        status_code::invalid_state);
    batch.observations_[0].frame_ = frame;
    batch.observations_.push_back(batch.observations_[0]);
    check(vqec_vision_ai_core_obval_validate_batch(batch, frame, geometry).code_ ==
        status_code::invalid_argument);
    batch.observations_.pop_back();
    auto wrong = batch;
    wrong.frame_.frame_id_++;
    check(vqec_vision_ai_core_obval_validate_batch(wrong, frame, geometry).code_ ==
        status_code::invalid_state);
    batch.observations_[0].track_id_ = 0;
    check(vqec_vision_ai_core_obval_validate_detections(batch, frame, geometry).code_ ==
        status_code::ok);
    check(vqec_vision_ai_core_obval_validate_batch(batch, frame, geometry).code_ ==
        status_code::invalid_argument);
    std::cout << "observation failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
