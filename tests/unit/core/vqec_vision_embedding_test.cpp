#include <cmath>
#include <iostream>
#include <limits>

#include "vqec/vision/ai/contracts/vqec_vision_embedding.hpp"

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    const preview_frame_key frame{0, 0, 1, 10, 20};
    const float component = 1.0F / std::sqrt(2.0F);
    embedding_result result{frame, 7, "face_embedding", "1.0.0",
        {component, component}, true};
    check(vqec_vision_ai_core_embct_validate_result(result, frame).code_ ==
        status_code::ok);
    result.values_[0] = std::numeric_limits<float>::quiet_NaN();
    check(vqec_vision_ai_core_embct_validate_result(result, frame).code_ ==
        status_code::invalid_argument);
    result.values_ = {0.0F, 0.0F};
    result.is_l2_normalized_ = false;
    check(vqec_vision_ai_core_embct_validate_result(result, frame).code_ ==
        status_code::invalid_argument);
    result.values_ = {1.0F, 1.0F};
    result.is_l2_normalized_ = true;
    check(vqec_vision_ai_core_embct_validate_result(result, frame).code_ ==
        status_code::invalid_argument);
    result.values_ = {component, component};
    result.track_id_ = 0;
    check(vqec_vision_ai_core_embct_validate_result(result, frame).code_ ==
        status_code::invalid_argument);
    result.track_id_ = 7;
    auto wrong_frame = frame;
    ++wrong_frame.frame_id_;
    check(vqec_vision_ai_core_embct_validate_result(result, wrong_frame).code_ ==
        status_code::invalid_argument);
    std::cout << "embedding failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
