#include "vqec_vision_dsp_preprocessor.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {

namespace {

constexpr std::size_t g_geom_size = 12;
constexpr std::uint32_t g_rgb_channels = 3;

}  // namespace

struct dsp_preprocessor::implementation {
    dsp_preprocessor_config config_;
    std::shared_ptr<dsp_buffer_cache> buffer_cache_;

    explicit implementation(dsp_preprocessor_config _config)
        : config_(std::move(_config)),
          buffer_cache_(config_.buffer_cache_ != nullptr ?
              config_.buffer_cache_ : std::make_shared<dsp_buffer_cache>()) {}
};

dsp_preprocessor::dsp_preprocessor(dsp_preprocessor_config _config)
    : implementation_(std::make_unique<implementation>(std::move(_config))) {}

dsp_preprocessor::~dsp_preprocessor() noexcept = default;

dsp_preprocessor::dsp_preprocessor(dsp_preprocessor&&) noexcept = default;
dsp_preprocessor& dsp_preprocessor::operator=(dsp_preprocessor&&) noexcept = default;

std::array<std::int32_t, 12> dsp_preprocessor::vqec_vision_ai_qcom_dsppr_compute_geom(
    dsp_preprocessor_kind _kind,
    std::uint32_t _src_w, std::uint32_t _src_h,
    std::int32_t _y_stride, std::uint32_t _uv_offset, std::int32_t _uv_stride,
    std::uint32_t _tensor_side) {
    std::array<std::int32_t, 12> geom{};
    if (_src_w == 0 || _src_h == 0 || _tensor_side == 0) {
        return geom;
    }

    const int src_w = static_cast<int>(_src_w);
    const int src_h = static_cast<int>(_src_h);
    const int side = static_cast<int>(_tensor_side);
    const int y_stride = _y_stride > 0 ? _y_stride : src_w;
    const int uv_offset = _uv_offset > 0 ? static_cast<int>(_uv_offset) : (y_stride * src_h);
    const int uv_stride = _uv_stride > 0 ? _uv_stride : src_w;

    const float scale = std::min(
        static_cast<float>(side) / static_cast<float>(src_w),
        static_cast<float>(side) / static_cast<float>(src_h));

    int new_w = 0;
    int new_h = 0;
    int dst_x = 0;
    int dst_y = 0;
    int pad = 0;

    if (_kind == dsp_preprocessor_kind::yolov8) {
        new_w = static_cast<int>(std::lround(static_cast<float>(src_w) * scale));
        new_h = static_cast<int>(std::lround(static_cast<float>(src_h) * scale));
        new_w &= ~1;
        new_h &= ~1;
        dst_x = static_cast<int>(std::lround(static_cast<float>(side - new_w) / 2.0F - 0.1F));
        dst_y = static_cast<int>(std::lround(static_cast<float>(side - new_h) / 2.0F - 0.1F));
        pad = 114;
    } else {
        new_w = static_cast<int>(static_cast<float>(src_w) * scale);
        new_h = static_cast<int>(static_cast<float>(src_h) * scale);
        new_w &= ~1;
        new_h &= ~1;
        dst_x = 0;
        dst_y = 0;
        pad = 0;
    }

    geom[0] = src_w;
    geom[1] = src_h;
    geom[2] = y_stride;
    geom[3] = uv_offset;
    geom[4] = uv_stride;
    geom[5] = side;
    geom[6] = side;
    geom[7] = dst_x;
    geom[8] = dst_y;
    geom[9] = new_w;
    geom[10] = new_h;
    geom[11] = pad;
    return geom;
}

status dsp_preprocessor::vqec_vision_ai_ports_imgpr_validate(
    const raw_frame& _frame, const inference_plan& _plan,
    const tensor_spec& _target) const {
    if (implementation_ == nullptr) {
        return {status_code::unsupported, "dsp_preprocessor implementation is null"};
    }
    if (_frame.descriptor_.width_ == 0 || _frame.descriptor_.height_ == 0) {
        return {status_code::invalid_argument,
            "dsp_preprocessor requires non-zero frame dimensions"};
    }
    if (_frame.native_handle_ < 0) {
        return {status_code::invalid_argument,
            "dsp_preprocessor requires a valid dma-buf native handle"};
    }
    if (_target.dtype_ != tensor_element_type::uint16) {
        return {status_code::invalid_argument,
            "dsp_preprocessor target must be quantized uint16"};
    }
    if (_target.layout_ != tensor_layout::nhwc &&
        _target.layout_ != tensor_layout::unknown) {
        return {status_code::invalid_argument,
            "dsp_preprocessor target layout must be NHWC"};
    }
    if (_target.dimensions_.size() != 4 || _target.dimensions_[0] != 1 ||
        _target.dimensions_[3] != g_rgb_channels) {
        return {status_code::invalid_argument,
            "dsp_preprocessor target dimensions must be 1xHxWx3"};
    }
    if (_target.dimensions_[1] != _target.dimensions_[2]) {
        return {status_code::invalid_argument,
            "dsp_preprocessor target must be a square tensor"};
    }
    (void)_plan;
    return {};
}

status dsp_preprocessor::vqec_vision_ai_ports_imgpr_preprocess(
    const raw_frame& _frame, const inference_plan& _plan,
    const tensor_spec& _target, std::vector<tensor_blob>& _outputs) {
    const auto valid = vqec_vision_ai_ports_imgpr_validate(_frame, _plan, _target);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    if (implementation_->config_.session_ == nullptr) {
        return {status_code::unsupported,
            "dsp_preprocessor requires an initialized dsp_session"};
    }

    status map_status;
    const std::uint8_t* frame_base = implementation_->buffer_cache_->vqec_vision_ai_qcom_dspbc_map(
        static_cast<int>(_frame.native_handle_),
        static_cast<std::size_t>(_frame.descriptor_.allocation_size_bytes_),
        map_status);
    if (frame_base == nullptr || map_status.code_ != status_code::ok) {
        return map_status.code_ != status_code::ok ? map_status :
            status{status_code::io_error, "dsp_buffer_cache map returned null"};
    }

    const std::uint32_t tensor_side = _target.dimensions_[1];
    const auto geom = vqec_vision_ai_qcom_dsppr_compute_geom(
        implementation_->config_.kind_,
        _frame.descriptor_.width_, _frame.descriptor_.height_,
        _frame.descriptor_.strides_[0],
        _frame.descriptor_.offsets_[1],
        _frame.descriptor_.strides_[1],
        tensor_side);

    const std::size_t tensor_elements = static_cast<std::size_t>(tensor_side * tensor_side * g_rgb_channels);
    const std::size_t expected_bytes = tensor_elements * sizeof(std::uint16_t);

    tensor_blob candidate;
    const bool can_reuse = (_outputs.size() == 1U &&
                            _outputs[0].bytes_.size() == expected_bytes &&
                            _outputs[0].spec_.dtype_ == _target.dtype_);
    try {
        if (can_reuse) {
            candidate = std::move(_outputs[0]);
        } else {
            candidate.bytes_.resize(expected_bytes);
        }
        candidate.spec_ = _target;
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "cannot allocate dsp_preprocessor output tensor"};
    }

    auto* tensor_ptr = reinterpret_cast<std::uint16_t*>(candidate.bytes_.data());
    const int tensor_len = static_cast<int>(tensor_elements);
    const int frame_len = static_cast<int>(_frame.descriptor_.allocation_size_bytes_);

    status prep_status;
    if (implementation_->config_.kind_ == dsp_preprocessor_kind::yolov8) {
        prep_status = implementation_->config_.session_->vqec_vision_ai_qcom_dspsn_preprocess_person_yolov8n(
            frame_base, frame_len, geom.data(), static_cast<int>(g_geom_size),
            tensor_ptr, tensor_len);
    } else {
        prep_status = implementation_->config_.session_->vqec_vision_ai_qcom_dspsn_preprocess_face_scrfd(
            frame_base, frame_len, geom.data(), static_cast<int>(g_geom_size),
            tensor_ptr, tensor_len);
    }

    if (prep_status.code_ != status_code::ok) {
        return prep_status;
    }

    if (can_reuse) {
        _outputs[0] = std::move(candidate);
    } else {
        _outputs.clear();
        _outputs.push_back(std::move(candidate));
    }
    return {};
}

}  // namespace vqec::vision::ai
