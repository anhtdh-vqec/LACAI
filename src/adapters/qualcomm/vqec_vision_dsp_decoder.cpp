#include "vqec_vision_dsp_decoder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <post_person_yolov8n.h>
#include <post_face_scrfd.h>

#include <vqec_dsp_types.h>
#include "vqec_vision_dsp_session.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_observation.hpp"

namespace vqec::vision::ai {

namespace {

static constexpr const char* g_scrfd_names[] = {
    "score_8", "score_16", "score_32", "bbox_8", "bbox_16", "bbox_32",
    "kps_8", "kps_16", "kps_32"};

status vqec_vision_ai_qcom_dspdc_validate_config(const dsp_decoder_config& _config) {
    if (_config.source_width_ == 0 || _config.source_height_ == 0 ||
        _config.source_width_ > VQEC_MAX_FRAME_SIDE ||
        _config.source_height_ > VQEC_MAX_FRAME_SIDE ||
        !std::isfinite(_config.confidence_threshold_) ||
        !std::isfinite(_config.iou_threshold_) || _config.confidence_threshold_ <= 0 ||
        _config.confidence_threshold_ > 1 || _config.iou_threshold_ <= 0 ||
        _config.iou_threshold_ > 1 || _config.class_id_.empty()) {
        return {status_code::invalid_argument, "invalid legacy DSP decoder configuration"};
    }
    if (_config.tensor_width_ != SCRFD_INPUT || _config.tensor_height_ != SCRFD_INPUT ||
        (_config.kind_ != dsp_decoder_kind::yolov8 && _config.kind_ != dsp_decoder_kind::scrfd) ||
        (_config.kind_ == dsp_decoder_kind::yolov8 && _config.placement_ != image_placement::centre) ||
        (_config.kind_ == dsp_decoder_kind::scrfd &&
            (_config.placement_ != image_placement::top_left || _config.landmark_count_ !=
                VQEC_KPS_FLOATS / 2))) {
        return {status_code::unsupported, "decoder exceeds legacy DSP kernel envelope"};
    }
    return {};
}

status vqec_vision_ai_qcom_dspdc_validate_tensor(
    const tensor_spec& _spec, dsp_decoder_kind _kind, std::size_t _role) {
    if (_spec.dtype_ != tensor_element_type::uint16 || !_spec.quantization_.is_quantized_ ||
        !std::isfinite(_spec.quantization_.scale_) || _spec.quantization_.scale_ <= 0 ||
        _spec.dimensions_.size() != 3 || _spec.dimensions_[0] != 1) {
        return {status_code::unsupported, "legacy DSP tensor dtype/quantization/rank mismatch"};
    }
    if (_kind == dsp_decoder_kind::yolov8) {
        if (_spec.dimensions_[1] != (_role == 0 ? 4U : 1U) ||
            _spec.dimensions_[2] != YOLOV8N_PERSON_PREDICTIONS) {
            return {status_code::unsupported, "legacy dense DSP head shape mismatch"};
        }
    } else {
        const std::uint32_t stride = 8U << (_role % 3);
        const auto cells = (SCRFD_INPUT / stride) * (SCRFD_INPUT / stride) * SCRFD_ANCHORS_PER_CELL;
        const std::uint32_t channels = _role < 3 ? 1U : (_role < 6 ? 4U : VQEC_KPS_FLOATS);
        if (_spec.dimensions_[1] != cells || _spec.dimensions_[2] != channels) {
            return {status_code::unsupported, "legacy anchor-distance DSP head shape mismatch"};
        }
    }
    return {};
}

const tensor_blob* vqec_vision_ai_qcom_dspdc_find_tensor(
    const tensor_result& _result, std::string_view _name) noexcept {
    const tensor_blob* matched = nullptr;
    for (const auto& tensor : _result.tensors_) {
        if (tensor.spec_.name_ == _name) {
            if (matched != nullptr) {
                return nullptr;
            }
            matched = &tensor;
        }
    }
    return matched;
}

void vqec_vision_ai_qcom_dspdc_compute_params(
    dsp_decoder_kind _kind,
    std::uint32_t _source_width, std::uint32_t _source_height,
    std::uint32_t _tensor_width, std::uint32_t _tensor_height,
    float _conf_thr, float _nms_thr,
    float _params[VQEC_POST_PARAM_FLOATS]) noexcept {
    const float side_w = static_cast<float>(_tensor_width);
    const float side_h = static_cast<float>(_tensor_height);
    const float src_w = static_cast<float>(_source_width);
    const float src_h = static_cast<float>(_source_height);
    const float scale = std::min(side_w / src_w, side_h / src_h);

    int dst_x = 0;
    int dst_y = 0;
    if (_kind == dsp_decoder_kind::yolov8) {
        int new_w = static_cast<int>(std::lround(src_w * scale)) & ~1;
        int new_h = static_cast<int>(std::lround(src_h * scale)) & ~1;
        dst_x = static_cast<int>(std::lround((side_w - static_cast<float>(new_w)) / 2.0F - 0.1F));
        dst_y = static_cast<int>(std::lround((side_h - static_cast<float>(new_h)) / 2.0F - 0.1F));
    }

    _params[VQEC_PP_CONF] = _conf_thr;
    _params[VQEC_PP_NMS] = _nms_thr;
    _params[VQEC_PP_SCALE] = scale;
    _params[VQEC_PP_PAD_X] = static_cast<float>(dst_x);
    _params[VQEC_PP_PAD_Y] = static_cast<float>(dst_y);
    _params[VQEC_PP_SRC_W] = src_w;
    _params[VQEC_PP_SRC_H] = src_h;
    _params[VQEC_PP_RESERVED] = 0.0F;
}

}  // namespace

dsp_decoder::dsp_decoder(dsp_decoder_config _config)
    : config_(std::move(_config)) {
    if (config_.session_ != nullptr) {
        owned_session_ = config_.session_;
    } else {
        owned_session_ = std::make_shared<dsp_session>();
    }
}

const dsp_decoder_config& dsp_decoder::vqec_vision_ai_qcom_dspdc_config() const noexcept {
    return config_;
}

status dsp_decoder::vqec_vision_ai_cntr_mddec_validate(
    const model_outputs& _outputs) const {
    const auto configured = vqec_vision_ai_qcom_dspdc_validate_config(config_);
    if (configured.code_ != status_code::ok) {
        return configured;
    }
    const std::size_t roles = config_.kind_ == dsp_decoder_kind::yolov8 ? 2U : 9U;
    if (_outputs.outputs_.size() != roles) {
        return {status_code::unsupported, "legacy DSP output count mismatch"};
    }
    for (std::size_t role = 0; role < roles; ++role) {
        const std::string name = config_.kind_ == dsp_decoder_kind::yolov8 ?
            (role == 0 ? config_.box_tensor_ : config_.score_tensor_) : g_scrfd_names[role];
        const tensor_spec* matched = nullptr;
        for (const auto& tensor : _outputs.outputs_) {
            if (tensor.name_ == name) {
                if (matched != nullptr) {
                    return {status_code::invalid_argument, "duplicate DSP tensor role"};
                }
                matched = &tensor;
            }
        }
        if (matched == nullptr || matched->layout_ != tensor_layout::flat) {
            return {status_code::unsupported, "DSP tensor role/declared flat layout mismatch"};
        }
        const auto valid = vqec_vision_ai_qcom_dspdc_validate_tensor(*matched, config_.kind_, role);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
    }
    return {};
}

status dsp_decoder::vqec_vision_ai_cntr_mddec_decode(
    const tensor_result& _result, const preview_frame_key& _expected_frame,
    observation_batch& _observations) {
    // One decoder may be registered for several source sessions. Serialize its
    // reusable workspace; the shared DSP session serializes the RPC independently.
    std::lock_guard<std::mutex> decode_lock(decode_mutex_);
    if (owned_session_ == nullptr) {
        return {status_code::protocol_error, "dsp_decoder has null session"};
    }
    const auto configured = vqec_vision_ai_qcom_dspdc_validate_config(config_);
    if (configured.code_ != status_code::ok) {
        return configured;
    }
    const std::size_t roles = config_.kind_ == dsp_decoder_kind::yolov8 ? 2U : 9U;
    if (_result.tensors_.size() != roles) {
        return {status_code::unsupported, "legacy DSP result count mismatch"};
    }
    for (std::size_t role = 0; role < roles; ++role) {
        const std::string_view name = config_.kind_ == dsp_decoder_kind::yolov8 ?
            std::string_view(role == 0 ? config_.box_tensor_ : config_.score_tensor_) : g_scrfd_names[role];
        const auto* matched = vqec_vision_ai_qcom_dspdc_find_tensor(_result, name);
        if (matched == nullptr) {
            return {status_code::unsupported, "legacy DSP result role mismatch"};
        }
        const auto valid = vqec_vision_ai_qcom_dspdc_validate_tensor(matched->spec_, config_.kind_, role);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        const auto& shape = matched->spec_.dimensions_;
        const auto bytes = static_cast<std::size_t>(shape[1]) * shape[2] * sizeof(std::uint16_t);
        if (matched->bytes_.size() != bytes) {
            return {status_code::invalid_argument, "legacy DSP result byte count mismatch"};
        }
    }

    float params[VQEC_POST_PARAM_FLOATS]{};
    vqec_vision_ai_qcom_dspdc_compute_params(
        config_.kind_, config_.source_width_, config_.source_height_,
        config_.tensor_width_, config_.tensor_height_,
        config_.confidence_threshold_, config_.iou_threshold_, params);

    auto& post_out = post_workspace_;

    if (config_.kind_ == dsp_decoder_kind::yolov8) {
        const tensor_blob* box_tensor = vqec_vision_ai_qcom_dspdc_find_tensor(_result, config_.box_tensor_);
        const tensor_blob* conf_tensor = vqec_vision_ai_qcom_dspdc_find_tensor(_result, config_.score_tensor_);
        if (box_tensor == nullptr || conf_tensor == nullptr) {
            return {status_code::invalid_argument, "Cannot locate boxes and conf tensors for YOLOv8"};
        }

        const auto* boxes_raw = reinterpret_cast<const std::uint16_t*>(box_tensor->bytes_.data());
        const auto* conf_raw = reinterpret_cast<const std::uint16_t*>(conf_tensor->bytes_.data());
        const int boxes_len = static_cast<int>(box_tensor->bytes_.size() / sizeof(std::uint16_t));
        const int conf_len = static_cast<int>(conf_tensor->bytes_.size() / sizeof(std::uint16_t));

        float quant[4]{
            box_tensor->spec_.quantization_.scale_,
            -static_cast<float>(box_tensor->spec_.quantization_.zero_point_),
            conf_tensor->spec_.quantization_.scale_,
            -static_cast<float>(conf_tensor->spec_.quantization_.zero_point_)
        };

        const auto status = owned_session_->vqec_vision_ai_qcom_dspsn_postprocess_person_yolov8n(
            boxes_raw, boxes_len, conf_raw, conf_len, quant, 4, params, VQEC_POST_PARAM_FLOATS, post_out);
        if (status.code_ != status_code::ok) {
            return status;
        }
    } else {
        // SCRFD 9 tensors
        const std::uint16_t* tensors[9]{};
        int lens[9]{};
        float quant[18]{};

        for (std::size_t i = 0; i < 9; ++i) {
            const tensor_blob* blob = vqec_vision_ai_qcom_dspdc_find_tensor(_result, g_scrfd_names[i]);
            if (blob == nullptr) {
                return {status_code::invalid_argument,
                    "Cannot locate SCRFD tensor " + std::string(g_scrfd_names[i])};
            }
            tensors[i] = reinterpret_cast<const std::uint16_t*>(blob->bytes_.data());
            lens[i] = static_cast<int>(blob->bytes_.size() / sizeof(std::uint16_t));
            quant[i * 2] = blob->spec_.quantization_.scale_;
            quant[i * 2 + 1] = -static_cast<float>(blob->spec_.quantization_.zero_point_);
        }

        const auto status = owned_session_->vqec_vision_ai_qcom_dspsn_postprocess_face_scrfd(
            tensors, lens, quant, 18, params, VQEC_POST_PARAM_FLOATS, post_out);
        if (status.code_ != status_code::ok) {
            return status;
        }
    }

    observation_batch batch;
    batch.frame_ = _expected_frame;
    batch.geometry_ = {config_.source_width_, config_.source_height_};

    const float src_w = static_cast<float>(config_.source_width_);
    const float src_h = static_cast<float>(config_.source_height_);
    const float max_kx = std::nextafter(src_w, 0.0F);
    const float max_ky = std::nextafter(src_h, 0.0F);

    for (std::int32_t i = 0; i < post_out.count_; ++i) {
        const float* b = &post_out.boxes_[static_cast<std::size_t>(i) * VQEC_BOX_FLOATS];
        const float norm_x1 = b[0];
        const float norm_y1 = b[1];
        const float norm_x2 = b[2];
        const float norm_y2 = b[3];
        const float score = b[4];

        if (!std::isfinite(norm_x1) || !std::isfinite(norm_y1) ||
            !std::isfinite(norm_x2) || !std::isfinite(norm_y2) ||
            !std::isfinite(score) || score < 0.0F || score > 1.0F) {
            continue;
        }

        const float px1 = std::max(0.0F, std::min(norm_x1 * src_w, max_kx));
        const float py1 = std::max(0.0F, std::min(norm_y1 * src_h, max_ky));
        const float px2 = std::max(px1, std::min(norm_x2 * src_w, src_w));
        const float py2 = std::max(py1, std::min(norm_y2 * src_h, src_h));
        const float width = std::min(px2 - px1, src_w - px1);
        const float height = std::min(py2 - py1, src_h - py1);

        if (width <= 0.0F || height <= 0.0F) {
            continue;
        }

        observation item;
        item.frame_ = _expected_frame;
        item.class_id_ = config_.class_id_;
        item.box_ = {px1, py1, width, height, 0xffffffffU, config_.class_id_};
        item.confidence_ = score;
        item.quality_ = score >= 0.75F ? observation_quality::high :
            (score >= 0.5F ? observation_quality::medium : observation_quality::low);

        if (config_.kind_ == dsp_decoder_kind::scrfd && !post_out.kps_.empty()) {
            const float* kps_base = &post_out.kps_[static_cast<std::size_t>(i) * VQEC_KPS_FLOATS];
            item.landmarks_.schema_id_ = config_.landmark_schema_id_;
            item.landmarks_.schema_version_ = config_.landmark_schema_version_;
            item.landmarks_.points_.reserve(config_.landmark_count_);
            for (std::size_t p = 0; p < config_.landmark_count_; ++p) {
                const float raw_kx = kps_base[p * 2];
                const float raw_ky = kps_base[p * 2 + 1];
                if (!std::isfinite(raw_kx) || !std::isfinite(raw_ky)) {
                    continue;
                }
                const float kx = std::max(0.0F, std::min(raw_kx, max_kx));
                const float ky = std::max(0.0F, std::min(raw_ky, max_ky));
                item.landmarks_.points_.push_back(landmark_point{kx, ky});
            }
        }

        batch.observations_.push_back(std::move(item));
        if (batch.observations_.size() >= observation_limits::g_max_observations) {
            break;
        }
    }

    _observations = std::move(batch);
    return {status_code::ok, ""};
}

}  // namespace vqec::vision::ai
