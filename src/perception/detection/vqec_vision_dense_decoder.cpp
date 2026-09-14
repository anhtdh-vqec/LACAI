#include "vqec_vision_dense_decoder.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

#include "vqec_vision_tensor_reader.hpp"

namespace vqec::vision::ai {
namespace {

struct dense_candidate {
    float score_{0.0F};
    std::size_t class_index_{0};
    float x_{0};
    float y_{0};
    float width_{0};
    float height_{0};
};

float vqec_vision_ai_detec_dnsdc_float_at(const tensor_blob& _blob, std::size_t _index) {
    float value = 0.0F;
    std::memcpy(&value, _blob.bytes_.data() + _index * sizeof(float), sizeof(value));
    return value;
}

float vqec_vision_ai_detec_dnsdc_iou(const dense_candidate& _left,
    const dense_candidate& _right) {
    if (_left.width_ <= 0.0F || _left.height_ <= 0.0F || _right.width_ <= 0.0F ||
        _right.height_ <= 0.0F) {
        return 0.0F;
    }
    const float lx2 = _left.x_ + _left.width_;
    const float ly2 = _left.y_ + _left.height_;
    const float rx2 = _right.x_ + _right.width_;
    const float ry2 = _right.y_ + _right.height_;
    const float ix1 = std::max(_left.x_, _right.x_);
    const float iy1 = std::max(_left.y_, _right.y_);
    const float ix2 = std::min(lx2, rx2);
    const float iy2 = std::min(ly2, ry2);
    if (ix2 <= ix1 || iy2 <= iy1) {
        return 0.0F;
    }
    const float intersection = (ix2 - ix1) * (iy2 - iy1);
    const float union_area =
        _left.width_ * _left.height_ + _right.width_ * _right.height_ - intersection;
    return union_area > 0.0F ? intersection / union_area : 0.0F;
}

}  // namespace

dense_decoder::dense_decoder(dense_decoder_config _config) : config_(std::move(_config)) {}

status dense_decoder::vqec_vision_ai_cntr_mddec_validate(
    const model_outputs& _outputs) const {
    if (config_.source_width_ == 0 || config_.source_height_ == 0 ||
        config_.tensor_width_ == 0 || config_.tensor_height_ == 0 ||
        config_.class_count_ == 0 ||
        config_.class_count_ > dense_decoder_limits::g_max_class_count ||
        config_.stages_.empty() || config_.stages_.size() > dense_decoder_limits::g_max_stages ||
        config_.confidence_threshold_ <= 0.0F || config_.confidence_threshold_ > 1.0F ||
        config_.iou_threshold_ <= 0.0F || config_.iou_threshold_ > 1.0F) {
        return {status_code::invalid_argument, "invalid dense decoder configuration"};
    }
    if (!config_.class_names_.empty() && config_.class_names_.size() != config_.class_count_) {
        return {status_code::invalid_argument, "dense decoder class names do not match count"};
    }
    std::uint64_t total_anchors = 0;
    for (const auto& stage : config_.stages_) {
        if (stage.grid_width_ == 0 || stage.grid_height_ == 0 ||
            stage.box_tensor_.empty() || stage.score_tensor_.empty()) {
            return {status_code::invalid_argument, "invalid dense decoder stage"};
        }
        total_anchors += static_cast<std::uint64_t>(stage.grid_width_) * stage.grid_height_;
        bool box_found = false;
        bool score_found = false;
        for (const auto& output : _outputs.outputs_) {
            box_found = box_found || output.name_ == stage.box_tensor_;
            score_found = score_found || output.name_ == stage.score_tensor_;
        }
        if (!box_found || !score_found) {
            return {status_code::unsupported, "dense decoder stage tensor is absent from the manifest"};
        }
    }
    if (total_anchors > dense_decoder_limits::g_max_anchors) {
        return {status_code::invalid_argument, "dense decoder anchor count exceeds the bound"};
    }
    return {};
}

status dense_decoder::vqec_vision_ai_cntr_mddec_decode(
    const tensor_result& _result, const preview_frame_key& _expected_frame,
    observation_batch& _observations) {
    if (config_.source_width_ == 0 || config_.source_height_ == 0 ||
        config_.tensor_width_ == 0 || config_.tensor_height_ == 0 ||
        config_.class_count_ == 0 || config_.stages_.empty()) {
        return {status_code::invalid_state, "dense decoder is not configured"};
    }
    const float scale = config_.placement_ == image_placement::stretch ?
        0.0F :
        std::min(static_cast<float>(config_.tensor_width_) /
                     static_cast<float>(config_.source_width_),
            static_cast<float>(config_.tensor_height_) /
                static_cast<float>(config_.source_height_));
    const float stretch_x = static_cast<float>(config_.tensor_width_) /
        static_cast<float>(config_.source_width_);
    const float stretch_y = static_cast<float>(config_.tensor_height_) /
        static_cast<float>(config_.source_height_);
    const float pad_left = config_.placement_ == image_placement::stretch ?
        0.0F :
        (static_cast<float>(config_.tensor_width_) -
            static_cast<float>(config_.source_width_) * scale) / 2.0F;
    const float pad_top = config_.placement_ == image_placement::stretch ?
        0.0F :
        (static_cast<float>(config_.tensor_height_) -
            static_cast<float>(config_.source_height_) * scale) / 2.0F;

    std::vector<dense_candidate> candidates;
    for (const auto& stage : config_.stages_) {
        const tensor_blob* box = nullptr;
        const tensor_blob* score = nullptr;
        const auto box_found = vqec_vision_ai_detec_tnrd_find_tensor(
            _result, stage.box_tensor_, box);
        const auto score_found = vqec_vision_ai_detec_tnrd_find_tensor(
            _result, stage.score_tensor_, score);
        if (box_found.code_ != status_code::ok || score_found.code_ != status_code::ok) {
            return {status_code::invalid_argument, "dense decoder tensor is missing from result"};
        }
        const std::size_t cells =
            static_cast<std::size_t>(stage.grid_width_) * stage.grid_height_;
        if (box->spec_.dtype_ != tensor_element_type::float32 ||
            score->spec_.dtype_ != tensor_element_type::float32 ||
            box->bytes_.size() != cells * 4U * sizeof(float) ||
            score->bytes_.size() != cells * config_.class_count_ * sizeof(float)) {
            return {status_code::unsupported,
                "dense decoder requires float32 box/score tensors of the configured shape"};
        }
        for (std::size_t cell = 0; cell < cells; ++cell) {
            const std::size_t gy = cell / stage.grid_width_;
            const std::size_t gx = cell % stage.grid_width_;
            for (std::size_t class_index = 0; class_index < config_.class_count_;
                 ++class_index) {
                const float confidence = vqec_vision_ai_detec_dnsdc_float_at(
                    *score, cell * config_.class_count_ + class_index);
                if (confidence < config_.confidence_threshold_) {
                    continue;
                }
                const float centre_x = vqec_vision_ai_detec_dnsdc_float_at(*box, cell * 4U);
                const float centre_y = vqec_vision_ai_detec_dnsdc_float_at(*box, cell * 4U + 1U);
                const float width = vqec_vision_ai_detec_dnsdc_float_at(*box, cell * 4U + 2U);
                const float height = vqec_vision_ai_detec_dnsdc_float_at(*box, cell * 4U + 3U);
                (void)gx;
                (void)gy;
                const float source_cx = config_.placement_ == image_placement::stretch ?
                    centre_x / stretch_x : (centre_x - pad_left) / scale;
                const float source_cy = config_.placement_ == image_placement::stretch ?
                    centre_y / stretch_y : (centre_y - pad_top) / scale;
                const float source_w = config_.placement_ == image_placement::stretch ?
                    width / stretch_x : width / scale;
                const float source_h = config_.placement_ == image_placement::stretch ?
                    height / stretch_y : height / scale;
                float x1 = source_cx - source_w / 2.0F;
                float y1 = source_cy - source_h / 2.0F;
                float x2 = source_cx + source_w / 2.0F;
                float y2 = source_cy + source_h / 2.0F;
                x1 = std::max(0.0F, x1);
                y1 = std::max(0.0F, y1);
                x2 = std::min(static_cast<float>(config_.source_width_), x2);
                y2 = std::min(static_cast<float>(config_.source_height_), y2);
                if (x2 <= x1 || y2 <= y1) {
                    continue;
                }
                dense_candidate candidate;
                candidate.score_ = confidence;
                candidate.class_index_ = class_index;
                candidate.x_ = x1;
                candidate.y_ = y1;
                candidate.width_ = x2 - x1;
                candidate.height_ = y2 - y1;
                candidates.push_back(candidate);
                if (candidates.size() > observation_limits::g_max_observations * 16U) {
                    return {status_code::resource_exhausted,
                        "dense decoder candidate count exceeds the bound"};
                }
            }
        }
    }

    // Greedy per-class NMS, highest confidence first, stable by class then score.
    std::vector<std::size_t> order(candidates.size());
    for (std::size_t index = 0; index < order.size(); ++index) {
        order[index] = index;
    }
    std::stable_sort(order.begin(), order.end(),
        [&candidates](std::size_t _left, std::size_t _right) {
            if (candidates[_left].score_ != candidates[_right].score_) {
                return candidates[_left].score_ > candidates[_right].score_;
            }
            return candidates[_left].class_index_ < candidates[_right].class_index_;
        });

    observation_batch batch;
    batch.frame_ = _expected_frame;
    batch.geometry_ = {config_.source_width_, config_.source_height_};
    std::vector<bool> suppressed(candidates.size(), false);
    for (const auto selected : order) {
        if (suppressed[selected]) {
            continue;
        }
        for (std::size_t other = 0; other < candidates.size(); ++other) {
            if (other == selected || suppressed[other] ||
                candidates[other].class_index_ != candidates[selected].class_index_) {
                continue;
            }
            if (vqec_vision_ai_detec_dnsdc_iou(candidates[selected], candidates[other]) >
                config_.iou_threshold_) {
                suppressed[other] = true;
            }
        }
        observation item;
        item.frame_ = _expected_frame;
        item.class_id_ = config_.class_names_.empty() ?
            std::to_string(candidates[selected].class_index_) :
            config_.class_names_[candidates[selected].class_index_];
        item.box_ = {candidates[selected].x_, candidates[selected].y_,
            candidates[selected].width_, candidates[selected].height_, 0xffffffffU,
            item.class_id_};
        item.confidence_ = candidates[selected].score_;
        item.quality_ = item.confidence_ >= 0.75F ? observation_quality::high :
            (item.confidence_ >= 0.5F ? observation_quality::medium :
                observation_quality::low);
        batch.observations_.push_back(std::move(item));
        if (batch.observations_.size() > observation_limits::g_max_observations) {
            return {status_code::resource_exhausted,
                "dense decoder detections exceed the observation limit"};
        }
    }
    _observations = std::move(batch);
    return {};
}

}  // namespace vqec::vision::ai
