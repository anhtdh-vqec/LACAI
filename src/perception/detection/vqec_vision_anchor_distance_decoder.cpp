#include "vqec_vision_anchor_distance_decoder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "vqec_vision_tensor_reader.hpp"
#include "vqec/vision/ai/contracts/base/vqec_vision_identifier.hpp"
#include "vqec/vision/ai/contracts/inference/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

std::size_t vqec_vision_ai_detec_addcd_stage_anchors(
    const anchor_distance_stage_config& _stage) noexcept {
    const std::uint64_t count = static_cast<std::uint64_t>(_stage.grid_width_) *
        _stage.grid_height_ * _stage.anchors_per_cell_;
    return count <= std::numeric_limits<std::size_t>::max() ?
        static_cast<std::size_t>(count) : 0;
}

const tensor_spec* vqec_vision_ai_detec_addcd_find_spec(
    const model_outputs& _outputs, const std::string& _name) noexcept {
    for (const auto& output : _outputs.outputs_) {
        if (output.name_ == _name) {
            return &output;
        }
    }
    return nullptr;
}

bool vqec_vision_ai_detec_addcd_is_shape(
    const tensor_spec& _spec, std::size_t _anchors, std::size_t _channels) noexcept {
    return _spec.dimensions_.size() == 3 && _spec.dimensions_[0] == 1 &&
        _spec.dimensions_[1] == _anchors && _spec.dimensions_[2] == _channels;
}

template <typename candidate_type>
float vqec_vision_ai_detec_addcd_iou(
    const candidate_type& _left,
    const candidate_type& _right) noexcept {
    const float x1 = std::max(_left.x_, _right.x_);
    const float y1 = std::max(_left.y_, _right.y_);
    const float x2 = std::min(_left.x_ + _left.width_, _right.x_ + _right.width_);
    const float y2 = std::min(_left.y_ + _left.height_, _right.y_ + _right.height_);
    if (x2 <= x1 || y2 <= y1) {
        return 0.0F;
    }
    const float intersection = (x2 - x1) * (y2 - y1);
    const float union_area = _left.width_ * _left.height_ +
        _right.width_ * _right.height_ - intersection;
    return union_area > 0.0F ? intersection / union_area : 0.0F;
}

float vqec_vision_ai_detec_addcd_clip_coordinate(
    float _value, std::uint32_t _limit) noexcept {
    return std::min(std::max(0.0F, _value),
        std::nextafter(static_cast<float>(_limit), 0.0F));
}

float vqec_vision_ai_detec_addcd_fit_extent(
    float _origin, float _extent, std::uint32_t _limit) noexcept {
    const double available = static_cast<double>(_limit) - static_cast<double>(_origin);
    if (!(available > 0.0) || !std::isfinite(_extent)) {
        return 0.0F;
    }
    float result = static_cast<float>(std::min(static_cast<double>(_extent), available));
    while (result > 0.0F &&
        static_cast<double>(_origin) + static_cast<double>(result) > _limit) {
        result = std::nextafter(result, 0.0F);
    }
    return result;
}

}  // namespace

anchor_distance_decoder::anchor_distance_decoder(anchor_distance_decoder_config _config)
    : config_(std::move(_config)) {
    if (config_.max_candidates_ <= anchor_distance_decoder_limits::g_max_candidates) {
        candidates_.reserve(config_.max_candidates_);
    }
}

status anchor_distance_decoder::vqec_vision_ai_cntr_mddec_validate(
    const model_outputs& _outputs) const {
    if (config_.source_width_ == 0 || config_.source_height_ == 0 ||
        config_.tensor_width_ == 0 || config_.tensor_height_ == 0 ||
        (config_.placement_ != image_placement::top_left &&
            config_.placement_ != image_placement::centre &&
            config_.placement_ != image_placement::stretch) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            config_.class_id_, observation_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            config_.landmark_schema_id_, observation_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            config_.landmark_schema_version_, observation_limits::g_max_identifier_bytes) ||
        config_.landmark_count_ == 0 ||
        config_.landmark_count_ > anchor_distance_decoder_limits::g_max_landmarks ||
        !std::isfinite(config_.anchor_offset_cells_) ||
        config_.anchor_offset_cells_ < 0.0F || config_.anchor_offset_cells_ > 1.0F ||
        !std::isfinite(config_.confidence_threshold_) ||
        config_.confidence_threshold_ < 0.0F || config_.confidence_threshold_ > 1.0F ||
        !std::isfinite(config_.iou_threshold_) || config_.iou_threshold_ <= 0.0F ||
        config_.iou_threshold_ > 1.0F || config_.max_candidates_ == 0 ||
        config_.max_candidates_ > anchor_distance_decoder_limits::g_max_candidates ||
        config_.stages_.empty() ||
        config_.stages_.size() > anchor_distance_decoder_limits::g_max_stages) {
        return {status_code::invalid_argument, "anchor-distance decoder config is invalid"};
    }
    for (std::size_t index = 0; index < config_.stages_.size(); ++index) {
        const auto& stage = config_.stages_[index];
        const auto anchors = vqec_vision_ai_detec_addcd_stage_anchors(stage);
        if (stage.stride_ == 0 ||
            stage.stride_ > anchor_distance_decoder_limits::g_max_stride ||
            anchors == 0 || stage.score_tensor_.empty() || stage.box_tensor_.empty() ||
            stage.landmark_tensor_.empty() || stage.score_tensor_ == stage.box_tensor_ ||
            stage.score_tensor_ == stage.landmark_tensor_ ||
            stage.box_tensor_ == stage.landmark_tensor_ ||
            static_cast<std::uint64_t>(stage.grid_width_) * stage.stride_ !=
                config_.tensor_width_ ||
            static_cast<std::uint64_t>(stage.grid_height_) * stage.stride_ !=
                config_.tensor_height_) {
            return {status_code::invalid_argument, "anchor-distance stage is invalid"};
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (config_.stages_[previous].stride_ == stage.stride_) {
                return {status_code::invalid_argument, "anchor-distance stride is duplicated"};
            }
        }
        const auto* score = vqec_vision_ai_detec_addcd_find_spec(_outputs, stage.score_tensor_);
        const auto* box = vqec_vision_ai_detec_addcd_find_spec(_outputs, stage.box_tensor_);
        const auto* landmarks =
            vqec_vision_ai_detec_addcd_find_spec(_outputs, stage.landmark_tensor_);
        if (score == nullptr || box == nullptr || landmarks == nullptr ||
            !vqec_vision_ai_detec_addcd_is_shape(*score, anchors, 1) ||
            !vqec_vision_ai_detec_addcd_is_shape(*box, anchors, 4) ||
            !vqec_vision_ai_detec_addcd_is_shape(
                *landmarks, anchors, config_.landmark_count_ * 2U)) {
            return {status_code::unsupported,
                "anchor-distance tensor schema differs from package config"};
        }
    }
    return {};
}

status anchor_distance_decoder::vqec_vision_ai_cntr_mddec_decode(
    const tensor_result& _result, const preview_frame_key& _expected_frame,
    observation_batch& _observations) {
    if (config_.source_width_ == 0 || config_.source_height_ == 0 ||
        config_.tensor_width_ == 0 || config_.tensor_height_ == 0 ||
        config_.landmark_count_ == 0 ||
        config_.landmark_count_ > anchor_distance_decoder_limits::g_max_landmarks ||
        config_.max_candidates_ == 0 ||
        config_.max_candidates_ > anchor_distance_decoder_limits::g_max_candidates) {
        return {status_code::invalid_argument, "anchor-distance config is invalid"};
    }
    const float scale = config_.placement_ == image_placement::stretch ? 1.0F :
        std::min(static_cast<float>(config_.tensor_width_) / config_.source_width_,
            static_cast<float>(config_.tensor_height_) / config_.source_height_);
    const float pad_left = config_.placement_ == image_placement::centre ?
        (config_.tensor_width_ - config_.source_width_ * scale) / 2.0F : 0.0F;
    const float pad_top = config_.placement_ == image_placement::centre ?
        (config_.tensor_height_ - config_.source_height_ * scale) / 2.0F : 0.0F;
    const float stretch_x = static_cast<float>(config_.tensor_width_) / config_.source_width_;
    const float stretch_y = static_cast<float>(config_.tensor_height_) / config_.source_height_;
    auto& candidates = candidates_;
    candidates.clear();

    for (const auto& stage : config_.stages_) {
        if (stage.anchors_per_cell_ == 0 || stage.grid_width_ == 0 ||
            stage.grid_height_ == 0 || stage.stride_ == 0) {
            return {status_code::invalid_argument, "anchor-distance grid is invalid"};
        }
        const tensor_blob* score = nullptr;
        const tensor_blob* box = nullptr;
        const tensor_blob* landmarks = nullptr;
        if (vqec_vision_ai_detec_tnrd_find_tensor(
                _result, stage.score_tensor_, score).code_ != status_code::ok ||
            vqec_vision_ai_detec_tnrd_find_tensor(
                _result, stage.box_tensor_, box).code_ != status_code::ok ||
            vqec_vision_ai_detec_tnrd_find_tensor(
                _result, stage.landmark_tensor_, landmarks).code_ != status_code::ok) {
            return {status_code::invalid_argument, "anchor-distance tensor is missing"};
        }
        const std::size_t anchors = vqec_vision_ai_detec_addcd_stage_anchors(stage);
        if (!vqec_vision_ai_detec_addcd_is_shape(score->spec_, anchors, 1) ||
            !vqec_vision_ai_detec_addcd_is_shape(box->spec_, anchors, 4) ||
            !vqec_vision_ai_detec_addcd_is_shape(
                landmarks->spec_, anchors, config_.landmark_count_ * 2U)) {
            return {status_code::unsupported, "anchor-distance result shape is invalid"};
        }
        const bool is_float32 = score->spec_.dtype_ == tensor_element_type::float32 &&
            box->spec_.dtype_ == tensor_element_type::float32 &&
            landmarks->spec_.dtype_ == tensor_element_type::float32;
        const float* score_f32 = is_float32 ?
            reinterpret_cast<const float*>(score->bytes_.data()) : nullptr;
        const float* box_f32 = is_float32 ?
            reinterpret_cast<const float*>(box->bytes_.data()) : nullptr;
        const float* landmarks_f32 = is_float32 ?
            reinterpret_cast<const float*>(landmarks->bytes_.data()) : nullptr;

        for (std::size_t anchor = 0; anchor < anchors; ++anchor) {
            float confidence = 0.0F;
            if (is_float32) {
                confidence = score_f32[anchor];
            } else {
                if (vqec_vision_ai_detec_tnrd_read_scalar(
                        *score, anchor, confidence).code_ != status_code::ok) {
                    return {status_code::protocol_error, "cannot read anchor score"};
                }
            }
            if (!std::isfinite(confidence) || confidence < 0.0F || confidence > 1.0F) {
                return {status_code::protocol_error, "anchor score is not a probability"};
            }
            if (!(confidence >= config_.confidence_threshold_)) {
                continue;
            }
            if (candidates.size() == config_.max_candidates_) {
                return {status_code::resource_exhausted,
                    "anchor-distance candidate bound is exceeded"};
            }
            const std::size_t cell = anchor / stage.anchors_per_cell_;
            const float centre_x =
                (static_cast<float>(cell % stage.grid_width_) +
                    config_.anchor_offset_cells_) * stage.stride_;
            const float centre_y =
                (static_cast<float>(cell / stage.grid_width_) +
                    config_.anchor_offset_cells_) * stage.stride_;
            std::array<float, 4> distance{};
            for (std::size_t component = 0; component < distance.size(); ++component) {
                if (is_float32) {
                    distance[component] = box_f32[anchor * distance.size() + component];
                } else {
                    if (vqec_vision_ai_detec_tnrd_read_scalar(
                            *box, anchor * distance.size() + component,
                            distance[component]).code_ != status_code::ok) {
                        return {status_code::protocol_error, "cannot read anchor box"};
                    }
                }
                distance[component] *= stage.stride_;
                if (!std::isfinite(distance[component]) || distance[component] < 0.0F) {
                    return {status_code::protocol_error, "anchor distance is invalid"};
                }
            }
            float tensor_x1 = centre_x - distance[0];
            float tensor_y1 = centre_y - distance[1];
            float tensor_x2 = centre_x + distance[2];
            float tensor_y2 = centre_y + distance[3];
            candidate candidate;
            candidate.order_ = candidates.size();
            candidate.score_ = confidence;
            candidate.x_ = config_.placement_ == image_placement::stretch ?
                tensor_x1 / stretch_x : (tensor_x1 - pad_left) / scale;
            candidate.y_ = config_.placement_ == image_placement::stretch ?
                tensor_y1 / stretch_y : (tensor_y1 - pad_top) / scale;
            const float source_x2 = config_.placement_ == image_placement::stretch ?
                tensor_x2 / stretch_x : (tensor_x2 - pad_left) / scale;
            const float source_y2 = config_.placement_ == image_placement::stretch ?
                tensor_y2 / stretch_y : (tensor_y2 - pad_top) / scale;
            candidate.x_ = std::max(0.0F, candidate.x_);
            candidate.y_ = std::max(0.0F, candidate.y_);
            candidate.width_ = vqec_vision_ai_detec_addcd_fit_extent(
                candidate.x_, std::min(static_cast<float>(config_.source_width_), source_x2) -
                    candidate.x_, config_.source_width_);
            candidate.height_ = vqec_vision_ai_detec_addcd_fit_extent(
                candidate.y_, std::min(static_cast<float>(config_.source_height_), source_y2) -
                    candidate.y_, config_.source_height_);
            if (!(candidate.width_ > 0.0F) || !(candidate.height_ > 0.0F)) {
                continue;
            }
            for (std::size_t point = 0; point < config_.landmark_count_; ++point) {
                float offset_x = 0.0F;
                float offset_y = 0.0F;
                if (is_float32) {
                    offset_x = landmarks_f32[anchor * config_.landmark_count_ * 2U + point * 2U];
                    offset_y = landmarks_f32[anchor * config_.landmark_count_ * 2U + point * 2U + 1U];
                } else {
                    if (vqec_vision_ai_detec_tnrd_read_scalar(
                            *landmarks, anchor * config_.landmark_count_ * 2U + point * 2U,
                            offset_x).code_ != status_code::ok ||
                        vqec_vision_ai_detec_tnrd_read_scalar(
                            *landmarks, anchor * config_.landmark_count_ * 2U + point * 2U + 1U,
                            offset_y).code_ != status_code::ok) {
                        return {status_code::protocol_error, "cannot read anchor landmark"};
                    }
                }
                if (!std::isfinite(offset_x) || !std::isfinite(offset_y)) {
                    return {status_code::protocol_error, "anchor landmark is not finite"};
                }
                const float tensor_x = centre_x + offset_x * stage.stride_;
                const float tensor_y = centre_y + offset_y * stage.stride_;
                candidate.landmarks_[point].x_ = vqec_vision_ai_detec_addcd_clip_coordinate(
                    config_.placement_ == image_placement::stretch ? tensor_x / stretch_x :
                        (tensor_x - pad_left) / scale, config_.source_width_);
                candidate.landmarks_[point].y_ = vqec_vision_ai_detec_addcd_clip_coordinate(
                    config_.placement_ == image_placement::stretch ? tensor_y / stretch_y :
                        (tensor_y - pad_top) / scale, config_.source_height_);
            }
            candidates.push_back(candidate);
        }
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const candidate& _left,
           const candidate& _right) {
            return _left.score_ != _right.score_ ? _left.score_ > _right.score_ :
                _left.order_ < _right.order_;
        });
    observation_batch batch;
    batch.frame_ = _expected_frame;
    batch.geometry_ = {config_.source_width_, config_.source_height_};
    auto& suppressed = suppressed_;
    suppressed.fill(false);
    for (std::size_t selected = 0; selected < candidates.size(); ++selected) {
        if (suppressed[selected]) {
            continue;
        }
        if (batch.observations_.size() == observation_limits::g_max_observations) {
            return {status_code::resource_exhausted,
                "anchor-distance detections exceed the observation limit"};
        }
        observation item;
        item.frame_ = _expected_frame;
        item.class_id_ = config_.class_id_;
        item.box_ = {candidates[selected].x_, candidates[selected].y_,
            candidates[selected].width_, candidates[selected].height_,
            0xffffffffU, config_.class_id_};
        item.confidence_ = candidates[selected].score_;
        item.quality_ = observation_quality::unknown;
        item.landmarks_.schema_id_ = config_.landmark_schema_id_;
        item.landmarks_.schema_version_ = config_.landmark_schema_version_;
        item.landmarks_.points_.assign(candidates[selected].landmarks_.begin(),
            candidates[selected].landmarks_.begin() + config_.landmark_count_);
        batch.observations_.push_back(std::move(item));
        for (std::size_t other = selected + 1U; other < candidates.size(); ++other) {
            if (!suppressed[other] &&
                vqec_vision_ai_detec_addcd_iou(
                    candidates[selected], candidates[other]) > config_.iou_threshold_) {
                suppressed[other] = true;
            }
        }
    }
    _observations = std::move(batch);
    return {};
}

}  // namespace vqec::vision::ai
