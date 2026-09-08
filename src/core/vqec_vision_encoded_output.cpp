#include "vqec/vision/ai/contracts/vqec_vision_encoded_output.hpp"

#include <new>
#include <utility>

namespace vqec::vision::ai {

h264_access_unit_view owned_h264_output::vqec_vision_ai_core_encot_borrow_view() const noexcept {
    return {frame_, geometry_, is_keyframe_, {payload_.data(), payload_.size()},
        {sps_.data(), sps_.size()}, {pps_.data(), pps_.size()}};
}

status owned_h264_output::vqec_vision_ai_core_encot_copy_output(
    const h264_access_unit_view& _input, const preview_frame_key& _expected_frame,
    const preview_geometry& _expected_geometry,
    std::shared_ptr<const owned_h264_output>& _output) {
    const auto valid = vqec_vision_ai_core_pvctr_validate_access_unit(
        _input, _expected_frame, _expected_geometry);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    try {
        // Private constructor centralizes validation. shared_ptr cleans up on control-block failure.
        auto pending = std::shared_ptr<owned_h264_output>(new owned_h264_output());
        pending->frame_ = _input.frame_;
        pending->geometry_ = _input.geometry_;
        pending->is_keyframe_ = _input.is_keyframe_;
        pending->payload_.assign(_input.payload_.data_, _input.payload_.data_ + _input.payload_.size_);
        if (_input.sps_.size_ != 0) {
            pending->sps_.assign(_input.sps_.data_, _input.sps_.data_ + _input.sps_.size_);
        }
        if (_input.pps_.size_ != 0) {
            pending->pps_.assign(_input.pps_.data_, _input.pps_.data_ + _input.pps_.size_);
        }
        _output = std::move(pending);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "encoded output allocation failed"};
    }
    return {};
}

}  // namespace vqec::vision::ai
