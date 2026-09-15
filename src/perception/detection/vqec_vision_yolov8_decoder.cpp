#include "vqec_vision_yolov8_decoder.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

#include "vqec_vision_tensor_reader.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

struct yolov8_candidate {
    float score_{0.0F};
    std::size_t class_index_{0};
    float x_{0};
    float y_{0};
    float width_{0};
    float height_{0};
};

// Reads one typed element as a real value. Quantized integral tensors are dequantized with
// real = (stored - zero_point) * scale; float32 passes through. Any other combination is
// rejected by the caller before the loop.
status vqec_vision_ai_detec_y8dec_element(
    const tensor_blob& _blob, std::size_t _index, float& _value) {
    const auto& spec = _blob.spec_;
    const auto element_bytes = vqec_vision_ai_core_tnctr_element_size(spec.dtype_);
    if (element_bytes == 0 || (_index + 1U) * element_bytes > _blob.bytes_.size()) {
        return {status_code::protocol_error, "tensor element index is out of range"};
    }
    const auto* base = _blob.bytes_.data() + _index * element_bytes;
    if (!spec.quantization_.is_quantized_) {
        if (spec.dtype_ != tensor_element_type::float32) {
            return {status_code::unsupported, "non-quantized tensor is not float32"};
        }
        float value = 0.0F;
        std::memcpy(&value, base, sizeof(value));
        _value = value;
        return {};
    }
    float stored = 0.0F;
    switch (spec.dtype_) {
        case tensor_element_type::uint8: {
            std::uint8_t value = 0;
            std::memcpy(&value, base, sizeof(value));
            stored = static_cast<float>(value);
            break;
        }
        case tensor_element_type::int8: {
            std::int8_t value = 0;
            std::memcpy(&value, base, sizeof(value));
            stored = static_cast<float>(value);
            break;
        }
        case tensor_element_type::uint16: {
            std::uint16_t value = 0;
            std::memcpy(&value, base, sizeof(value));
            stored = static_cast<float>(value);
            break;
        }
        case tensor_element_type::int16: {
            std::int16_t value = 0;
            std::memcpy(&value, base, sizeof(value));
            stored = static_cast<float>(value);
            break;
        }
        case tensor_element_type::int32: {
            std::int32_t value = 0;
            std::memcpy(&value, base, sizeof(value));
            stored = static_cast<float>(value);
            break;
        }
        default:
            return {status_code::unsupported, "quantized tensor dtype is unsupported"};
    }
    _value = (stored - static_cast<float>(spec.quantization_.zero_point_)) *
        spec.quantization_.scale_;
    return {};
}

float vqec_vision_ai_detec_y8dec_iou(
    const yolov8_candidate& _left, const yolov8_candidate& _right) {
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

float vqec_vision_ai_detec_y8dec_fit_extent(
    float _origin, float _extent, std::uint32_t _limit) noexcept {
    const double available = static_cast<double>(_limit) - static_cast<double>(_origin);
    if (!(available > 0.0) || !std::isfinite(_extent)) {
        return 0.0F;
    }
    float fitted = static_cast<float>(std::min(static_cast<double>(_extent), available));
    // The preview contract adds in double precision. A float-rounded extent can therefore
    // exceed the exact remaining span even though its source corner was clamped to the limit.
    if (static_cast<double>(fitted) > available) {
        fitted = std::nextafter(fitted, 0.0F);
    }
    return fitted;
}

}  // namespace

yolov8_decoder::yolov8_decoder(yolov8_decoder_config _config) : config_(std::move(_config)) {}

status yolov8_decoder::vqec_vision_ai_cntr_mddec_validate(
    const model_outputs& _outputs) const {
    if (config_.source_width_ == 0 || config_.source_height_ == 0 ||
        config_.tensor_width_ == 0 || config_.tensor_height_ == 0 ||
        config_.class_count_ == 0 || config_.confidence_threshold_ <= 0.0F ||
        config_.confidence_threshold_ > 1.0F || config_.iou_threshold_ <= 0.0F ||
        config_.iou_threshold_ > 1.0F) {
        return {status_code::invalid_argument, "invalid YOLOv8 decoder configuration"};
    }
    if (config_.box_convention_ != coordinate_convention::tensor_pixels_xywh) {
        return {status_code::unsupported, "YOLOv8 decoder requires xywh in tensor pixels"};
    }
    if (!config_.class_names_.empty() && config_.class_names_.size() != config_.class_count_) {
        return {status_code::invalid_argument, "YOLOv8 decoder class names do not match count"};
    }
    bool box_found = false;
    bool score_found = false;
    for (const auto& output : _outputs.outputs_) {
        box_found = box_found || output.name_ == config_.box_tensor_;
        score_found = score_found || output.name_ == config_.score_tensor_;
    }
    if (!box_found || !score_found) {
        return {status_code::unsupported, "YOLOv8 decoder tensor is absent from the manifest"};
    }
    return {};
}

status yolov8_decoder::vqec_vision_ai_cntr_mddec_decode(
    const tensor_result& _result, const preview_frame_key& _expected_frame,
    observation_batch& _observations) {
    if (config_.source_width_ == 0 || config_.source_height_ == 0 ||
        config_.tensor_width_ == 0 || config_.tensor_height_ == 0 ||
        config_.class_count_ == 0) {
        return {status_code::invalid_state, "YOLOv8 decoder is not configured"};
    }
    if (config_.box_convention_ != coordinate_convention::tensor_pixels_xywh) {
        return {status_code::unsupported, "YOLOv8 decoder requires xywh in tensor pixels"};
    }
    const tensor_blob* box = nullptr;
    const tensor_blob* score = nullptr;
    if (vqec_vision_ai_detec_tnrd_find_tensor(_result, config_.box_tensor_, box).code_ !=
            status_code::ok ||
        vqec_vision_ai_detec_tnrd_find_tensor(_result, config_.score_tensor_, score).code_ !=
            status_code::ok) {
        return {status_code::invalid_argument, "YOLOv8 decoder tensor is missing from result"};
    }
    const auto box_element_bytes = vqec_vision_ai_core_tnctr_element_size(box->spec_.dtype_);
    const auto score_element_bytes = vqec_vision_ai_core_tnctr_element_size(score->spec_.dtype_);
    if (box_element_bytes == 0 || score_element_bytes == 0) {
        return {status_code::unsupported, "YOLOv8 tensor dtype is unsupported"};
    }
    if (box->bytes_.size() % (4U * box_element_bytes) != 0) {
        return {status_code::invalid_argument, "YOLOv8 box tensor length is not 4*A"};
    }
    const std::size_t anchors = box->bytes_.size() / (4U * box_element_bytes);
    if (anchors == 0 || score->bytes_.size() != anchors * config_.class_count_ *
            score_element_bytes) {
        return {status_code::unsupported, "YOLOv8 head shapes are inconsistent"};
    }

    const float scale = config_.placement_ == image_placement::stretch ? 0.0F :
        std::min(static_cast<float>(config_.tensor_width_) /
                     static_cast<float>(config_.source_width_),
            static_cast<float>(config_.tensor_height_) /
                static_cast<float>(config_.source_height_));
    const float pad_left = config_.placement_ == image_placement::stretch ? 0.0F :
        (static_cast<float>(config_.tensor_width_) -
            static_cast<float>(config_.source_width_) * scale) / 2.0F;
    const float pad_top = config_.placement_ == image_placement::stretch ? 0.0F :
        (static_cast<float>(config_.tensor_height_) -
            static_cast<float>(config_.source_height_) * scale) / 2.0F;
    const float stretch_x = static_cast<float>(config_.tensor_width_) /
        static_cast<float>(config_.source_width_);
    const float stretch_y = static_cast<float>(config_.tensor_height_) /
        static_cast<float>(config_.source_height_);

    std::vector<yolov8_candidate> candidates;
    for (std::size_t anchor = 0; anchor < anchors; ++anchor) {
        for (std::size_t class_index = 0; class_index < config_.class_count_; ++class_index) {
            float confidence = 0.0F;
            if (vqec_vision_ai_detec_y8dec_element(
                    *score, class_index * anchors + anchor, confidence).code_ !=
                status_code::ok) {
                return {status_code::unsupported, "YOLOv8 score element is unsupported"};
            }
            if (!(confidence >= config_.confidence_threshold_)) {
                continue;  // also drops NaN
            }
            float centre_x = 0.0F;
            float centre_y = 0.0F;
            float width = 0.0F;
            float height = 0.0F;
            if (vqec_vision_ai_detec_y8dec_element(*box, 0U * anchors + anchor, centre_x).code_ !=
                    status_code::ok ||
                vqec_vision_ai_detec_y8dec_element(*box, 1U * anchors + anchor, centre_y).code_ !=
                    status_code::ok ||
                vqec_vision_ai_detec_y8dec_element(*box, 2U * anchors + anchor, width).code_ !=
                    status_code::ok ||
                vqec_vision_ai_detec_y8dec_element(*box, 3U * anchors + anchor, height).code_ !=
                    status_code::ok) {
                return {status_code::unsupported, "YOLOv8 box element is unsupported"};
            }
            const float source_cx = config_.placement_ == image_placement::stretch ?
                centre_x / stretch_x : (centre_x - pad_left) / scale;
            const float source_cy = config_.placement_ == image_placement::stretch ?
                centre_y / stretch_y : (centre_y - pad_top) / scale;
            const float source_w = config_.placement_ == image_placement::stretch ?
                width / stretch_x : width / scale;
            const float source_h = config_.placement_ == image_placement::stretch ?
                height / stretch_y : height / scale;
            if (!(source_w > 0.0F) || !(source_h > 0.0F)) {
                continue;
            }
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
            const float clipped_width = vqec_vision_ai_detec_y8dec_fit_extent(
                x1, x2 - x1, config_.source_width_);
            const float clipped_height = vqec_vision_ai_detec_y8dec_fit_extent(
                y1, y2 - y1, config_.source_height_);
            if (!(clipped_width > 0.0F) || !(clipped_height > 0.0F)) {
                continue;
            }
            yolov8_candidate candidate;
            candidate.score_ = confidence;
            candidate.class_index_ = class_index;
            candidate.x_ = x1;
            candidate.y_ = y1;
            candidate.width_ = clipped_width;
            candidate.height_ = clipped_height;
            candidates.push_back(candidate);
            if (candidates.size() > observation_limits::g_max_observations * 16U) {
                return {status_code::resource_exhausted,
                    "YOLOv8 decoder candidate count exceeds the bound"};
            }
        }
    }

    std::vector<std::size_t> order(candidates.size());
    for (std::size_t index = 0; index < order.size(); ++index) {
        order[index] = index;
    }
    std::stable_sort(order.begin(), order.end(),
        [&candidates](std::size_t _left, std::size_t _right) {
            return candidates[_left].score_ > candidates[_right].score_;
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
            if (vqec_vision_ai_detec_y8dec_iou(candidates[selected], candidates[other]) >
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
                "YOLOv8 detections exceed the observation limit"};
        }
    }
    _observations = std::move(batch);
    return {};
}

}  // namespace vqec::vision::ai
