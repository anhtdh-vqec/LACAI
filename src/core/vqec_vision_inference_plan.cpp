#include <vqec/vision/ai/contracts/vqec_vision_inference_plan.hpp>

#include <vqec/vision/ai/contracts/vqec_vision_preview_limits.hpp>
#include <vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp>

#include <cmath>

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_core_infpl_is_absolute_path(const std::string& _path) {
    if (_path.empty() || _path.front() != '/' ||
        _path.size() > inference_limits::g_max_path_bytes) {
        return false;
    }
    for (const auto character : _path) {
        if (static_cast<unsigned char>(character) < 32 || character == '\\') {
            return false;
        }
    }
    // This is lexical validation only, not filesystem authorization.
    return _path.find("/../") == std::string::npos &&
           _path.find("/./") == std::string::npos &&
           _path.compare(_path.size() >= 3 ? _path.size() - 3 : 0, 3, "/..") != 0 &&
           _path.compare(_path.size() >= 2 ? _path.size() - 2 : 0, 2, "/.") != 0;
}

bool vqec_vision_ai_core_infpl_has_suffix(
    const std::string& _path, const std::string& _suffix) {
    return _path.size() >= _suffix.size() &&
           _path.compare(_path.size() - _suffix.size(), _suffix.size(), _suffix) == 0;
}

}  // namespace

std::uint64_t vqec_vision_ai_core_infpl_get_packed_frame_bytes(
    const inference_plan& _plan) noexcept {
    if (_plan.source_width_ == 0 || _plan.source_height_ == 0 ||
        _plan.source_width_ > preview_limits::g_max_dimension_pixels ||
        _plan.source_height_ > preview_limits::g_max_dimension_pixels ||
        (_plan.source_width_ % 2) != 0 || (_plan.source_height_ % 2) != 0) {
        return 0;
    }
    return static_cast<std::uint64_t>(_plan.source_width_) * _plan.source_height_ * 3 / 2;
}

status vqec_vision_ai_core_infpl_validate_plan(const inference_plan& _plan) {
    const auto frame_bytes = vqec_vision_ai_core_infpl_get_packed_frame_bytes(_plan);
    if (frame_bytes == 0) {
        return {status_code::invalid_argument,
                "expected configured even linear NV12 geometry within safety limits"};
    }
    if (_plan.tensor_width_ < inference_limits::g_min_tensor_dimension ||
        _plan.tensor_height_ < inference_limits::g_min_tensor_dimension ||
        _plan.tensor_width_ > inference_limits::g_max_tensor_dimension ||
        _plan.tensor_height_ > inference_limits::g_max_tensor_dimension) {
        return {status_code::invalid_argument, "Tensor dimensions outside initial plan limits"};
    }
    // Bounds also make narrowing to GStreamer fraction/int fields safe.
    if (_plan.fps_numerator_ == 0 || _plan.fps_denominator_ == 0 ||
        _plan.fps_numerator_ > inference_limits::g_max_fps_numerator ||
        _plan.fps_denominator_ > inference_limits::g_max_fps_denominator ||
        static_cast<std::uint64_t>(_plan.fps_numerator_) >
            static_cast<std::uint64_t>(inference_limits::g_max_frames_per_second) *
                _plan.fps_denominator_) {
        return {status_code::invalid_argument, "Invalid frame rate"};
    }
    switch (_plan.placement_) {
        case image_placement::top_left:
        case image_placement::centre:
        case image_placement::stretch:
            break;
        default:
            return {status_code::invalid_argument, "Model image placement must be explicit"};
    }
    if (vqec_vision_ai_core_tnctr_element_size(_plan.input_type_) == 0 ||
        _plan.input_type_ == tensor_element_type::int64 ||
        _plan.input_type_ == tensor_element_type::uint64) {
        // The reviewed converter/plugin path can produce the smaller integer and float
        // element types; 64-bit input is not a supported preprocessing target.
        return {status_code::unsupported, "unsupported model input element type"};
    }
    if (_plan.channel_order_ != channel_order::rgb && _plan.channel_order_ != channel_order::bgr) {
        return {status_code::invalid_argument, "Unsupported channel order"};
    }
    for (std::size_t channel = 0; channel < _plan.mean_.size(); ++channel) {
        if (!std::isfinite(_plan.mean_[channel]) || _plan.mean_[channel] < 0.0 ||
            _plan.mean_[channel] > 255.0 || !std::isfinite(_plan.sigma_[channel]) ||
            _plan.sigma_[channel] <= 0.0 || _plan.sigma_[channel] > 255.0) {
            return {status_code::invalid_argument, "Invalid plugin normalization coefficient"};
        }
    }
    if (_plan.input_type_ != tensor_element_type::float32 &&
        (_plan.mean_ != std::array<double, 3>{0.0, 0.0, 0.0} ||
         _plan.sigma_ != std::array<double, 3>{1.0, 1.0, 1.0})) {
        // Integer/fp16 converter output is not paired with FP32 mean/sigma coefficients;
        // custom normalization requires the explicit FLOAT32 path.
        return {status_code::unsupported,
                "custom normalization requires a FLOAT32 model input"};
    }
    if (!vqec_vision_ai_core_infpl_is_absolute_path(_plan.model_path_) ||
        !(vqec_vision_ai_core_infpl_has_suffix(_plan.model_path_, ".bin") ||
          vqec_vision_ai_core_infpl_has_suffix(_plan.model_path_, ".so")) ||
        !vqec_vision_ai_core_infpl_is_absolute_path(_plan.backend_path_) ||
        !vqec_vision_ai_core_infpl_is_absolute_path(_plan.system_path_)) {
        return {status_code::invalid_argument, "Expected absolute model and QNN library paths"};
    }
    if (_plan.input_queue_bytes_ < frame_bytes ||
        _plan.input_queue_bytes_ > inference_limits::g_max_input_queue_bytes ||
        _plan.output_queue_buffers_ == 0 ||
        _plan.output_queue_buffers_ > inference_limits::g_max_output_queue_buffers) {
        return {status_code::invalid_argument, "Queue budget outside initial plan limits"};
    }
    return {};
}

}  // namespace vqec::vision::ai
