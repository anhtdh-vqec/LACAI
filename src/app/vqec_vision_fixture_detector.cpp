#include "vqec_vision_fixture_detector.hpp"

#include <utility>

namespace vqec::vision::ai {

fixture_detector::fixture_detector(fixture_detector_config _config) noexcept
    : config_(std::move(_config)) {}

status fixture_detector::vqec_vision_ai_cntr_mddec_validate(
    const model_outputs& _outputs) const {
    return _outputs.outputs_.empty() ?
        status{status_code::unsupported, "fixture decoder requires an output"} : status{};
}

status fixture_detector::vqec_vision_ai_cntr_mddec_decode(
    const tensor_result& _result, const preview_frame_key& _expected_frame,
    observation_batch& _observations) {
    (void)_result;
    if (config_.width_ == 0 || config_.height_ == 0) {
        return {status_code::invalid_argument, "fixture detector requires a nonzero source"};
    }
    observation_batch candidate;
    candidate.frame_ = _expected_frame;
    candidate.geometry_ = {config_.width_, config_.height_};
    observation item;
    item.frame_ = _expected_frame;
    item.class_id_ = config_.class_id_;
    item.box_ = {static_cast<float>(config_.width_) * fixture_detector_limits::g_box_x_fraction,
        static_cast<float>(config_.height_) * fixture_detector_limits::g_box_y_fraction,
        static_cast<float>(config_.width_) * fixture_detector_limits::g_box_width_fraction,
        static_cast<float>(config_.height_) * fixture_detector_limits::g_box_height_fraction,
        0xffffffffU, config_.class_id_};
    item.confidence_ = config_.confidence_;
    item.quality_ = observation_quality::low;
    candidate.observations_.push_back(std::move(item));
    _observations = std::move(candidate);
    return {};
}

}  // namespace vqec::vision::ai
