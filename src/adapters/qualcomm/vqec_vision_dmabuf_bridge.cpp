#include "vqec_vision_dmabuf_bridge.hpp"

#include <array>
#include <atomic>
#include <utility>

#include <fcntl.h>
#include <unistd.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/video/video.h>

namespace vqec::vision::ai {

struct input_release_signal {
    std::atomic<bool> released_{false};
};

namespace {

struct tracked_frame_owner {
    std::shared_ptr<const void> owner_;
    std::shared_ptr<input_release_signal> signal_;
    ~tracked_frame_owner() noexcept {
        owner_.reset();
        signal_->released_.store(true, std::memory_order_release);
    }
};

struct fd_owner {
    int value_{-1};
    ~fd_owner() noexcept {
        if (value_ >= 0) {
            ::close(value_);
        }
    }
};

struct allocator_owner {
    GstAllocator* value_{nullptr};
    ~allocator_owner() noexcept {
        if (value_ != nullptr) {
            gst_object_unref(value_);
        }
    }
};

struct memory_owner {
    GstMemory* value_{nullptr};
    ~memory_owner() noexcept {
        if (value_ != nullptr) {
            gst_memory_unref(value_);
        }
    }
};

struct buffer_owner {
    GstBuffer* value_{nullptr};
    ~buffer_owner() noexcept {
        if (value_ != nullptr) {
            gst_buffer_unref(value_);
        }
    }
};

void vqec_vision_ai_qcom_dmbrg_release_owner(gpointer _owner) noexcept {
    delete static_cast<std::shared_ptr<const void>*>(_owner);
}

status vqec_vision_ai_qcom_dmbrg_validate_frame(const frame_descriptor& _descriptor,
                                                const dmabuf_bridge_profile& _profile) {
    if (_profile.width_ == 0 || _profile.height_ == 0 || _profile.max_allocation_bytes_ == 0 ||
        _descriptor.width_ != _profile.width_ || _descriptor.height_ != _profile.height_ ||
        (_descriptor.width_ & 1U) != 0 || (_descriptor.height_ & 1U) != 0 ||
        _descriptor.width_ > G_MAXINT || _descriptor.height_ > G_MAXINT) {
        return {status_code::invalid_argument, "invalid or mismatched NV12 source profile"};
    }
    if (_descriptor.allocation_size_bytes_ == 0 ||
        _descriptor.allocation_size_bytes_ > _profile.max_allocation_bytes_ ||
        _descriptor.allocation_size_bytes_ > static_cast<std::uint64_t>(G_MAXSSIZE) ||
        _descriptor.memory_offset_bytes_ > _descriptor.allocation_size_bytes_ ||
        _descriptor.view_size_bytes_ >
            _descriptor.allocation_size_bytes_ - _descriptor.memory_offset_bytes_) {
        return {status_code::invalid_argument, "invalid DMA-BUF allocation or valid view"};
    }
    std::array<std::uint64_t, 2> ends{};
    for (std::size_t plane = 0; plane < 2; ++plane) {
        if (_descriptor.strides_[plane] <= 0 ||
            static_cast<std::uint32_t>(_descriptor.strides_[plane]) < _descriptor.width_) {
            return {status_code::invalid_argument, "invalid NV12 plane stride"};
        }
        const std::uint64_t rows = plane == 0 ? _descriptor.height_ : _descriptor.height_ / 2;
        ends[plane] = _descriptor.offsets_[plane] +
                      rows * static_cast<std::uint64_t>(_descriptor.strides_[plane]);
        if (ends[plane] > _descriptor.view_size_bytes_) {
            return {status_code::invalid_argument, "NV12 plane exceeds valid memory view"};
        }
    }
    if (_descriptor.offsets_[0] < ends[1] && _descriptor.offsets_[1] < ends[0]) {
        return {status_code::invalid_argument, "overlapping NV12 plane spans"};
    }
    return {};
}

} // namespace

status vqec_vision_ai_qcom_dmbrg_make_profile(
    const source_deployment_config& _source, dmabuf_bridge_profile& _profile) {
    if (_source.raw_source_ref_.empty() || _source.profile_.width_ == 0 ||
        _source.profile_.height_ == 0 ||
        _source.profile_.width_ > deployment_limits::g_max_dimension_pixels ||
        _source.profile_.height_ > deployment_limits::g_max_dimension_pixels ||
        _source.profile_.width_ % 2 != 0 || _source.profile_.height_ % 2 != 0) {
        return {status_code::invalid_argument, "invalid deployment source profile"};
    }
    const auto pixels = static_cast<std::uint64_t>(_source.profile_.width_) *
        _source.profile_.height_;
    const auto packed_bytes = pixels + pixels / 2;
    if (_source.memory_.max_frame_allocation_bytes_ < packed_bytes ||
        _source.memory_.max_frame_allocation_bytes_ >
            deployment_limits::g_max_frame_allocation_bytes) {
        return {status_code::invalid_argument, "invalid deployment frame budget"};
    }
    dmabuf_bridge_profile candidate;
    candidate.width_ = _source.profile_.width_;
    candidate.height_ = _source.profile_.height_;
    candidate.max_allocation_bytes_ = _source.memory_.max_frame_allocation_bytes_;
    _profile = candidate;
    return {};
}

status vqec_vision_ai_qcom_dmbrg_wrap_frame(const frame_descriptor& _descriptor, int _frame_fd,
                                            const std::shared_ptr<const void>& _owner,
                                            const dmabuf_bridge_profile& _profile,
                                            GstBuffer*& _buffer) {
    if (_buffer != nullptr || _frame_fd < 0 || !_owner) {
        return {status_code::invalid_argument, "output must be null; frame FD and owner required"};
    }
    const auto validated = vqec_vision_ai_qcom_dmbrg_validate_frame(_descriptor, _profile);
    if (validated.code_ != status_code::ok) {
        return validated;
    }
    // Copy, never move: a failed wrap must not consume the caller's frame owner.
    auto retained_owner = std::make_unique<std::shared_ptr<const void>>(_owner);
    fd_owner duplicate{::fcntl(_frame_fd, F_DUPFD_CLOEXEC, 0)};
    if (duplicate.value_ < 0) {
        return {status_code::io_error, "cannot duplicate camera FD"};
    }
    allocator_owner allocator{gst_dmabuf_allocator_new()};
    if (allocator.value_ == nullptr) {
        return {status_code::resource_exhausted, "cannot create DMA-BUF allocator"};
    }
    memory_owner memory{
        gst_dmabuf_allocator_alloc(allocator.value_, duplicate.value_,
                                   static_cast<gsize>(_descriptor.allocation_size_bytes_))};
    if (memory.value_ == nullptr) {
        return {status_code::resource_exhausted, "cannot wrap camera DMA-BUF allocation"};
    }
    duplicate.value_ = -1; // Allocator owns the duplicate after successful wrapping.
    gst_memory_resize(memory.value_, static_cast<gssize>(_descriptor.memory_offset_bytes_),
                      static_cast<gssize>(_descriptor.view_size_bytes_));
    GST_MINI_OBJECT_FLAG_SET(memory.value_, GST_MEMORY_FLAG_READONLY);
    gst_mini_object_set_qdata(GST_MINI_OBJECT(memory.value_),
                              g_quark_from_static_string("vqec-vision-ai-camera-frame-owner"),
                              retained_owner.release(), vqec_vision_ai_qcom_dmbrg_release_owner);
    buffer_owner candidate{gst_buffer_new()};
    if (candidate.value_ == nullptr) {
        return {status_code::resource_exhausted, "cannot create camera GstBuffer"};
    }
    gst_buffer_append_memory(candidate.value_, memory.value_);
    memory.value_ = nullptr; // GstBuffer owns the memory reference now.
    std::array<gsize, GST_VIDEO_MAX_PLANES> offsets{};
    std::array<gint, GST_VIDEO_MAX_PLANES> strides{};
    for (std::size_t plane = 0; plane < 2; ++plane) {
        offsets[plane] = _descriptor.offsets_[plane];
        strides[plane] = _descriptor.strides_[plane];
    }
    if (gst_buffer_add_video_meta_full(
            candidate.value_, GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_FORMAT_NV12, _descriptor.width_,
            _descriptor.height_, 2, offsets.data(), strides.data()) == nullptr) {
        return {status_code::resource_exhausted, "cannot attach original NV12 plane metadata"};
    }
    GST_BUFFER_PTS(candidate.value_) = _descriptor.pts_ns_;
    GST_BUFFER_DTS(candidate.value_) = _descriptor.dts_ns_;
    GST_BUFFER_DURATION(candidate.value_) = _descriptor.duration_ns_;
    _buffer = std::exchange(candidate.value_, nullptr);
    return {};
}

status vqec_vision_ai_qcom_dmbrg_wrap_tracked_frame(
    const frame_descriptor& _descriptor, int _frame_fd,
    const std::shared_ptr<const void>& _owner, const dmabuf_bridge_profile& _profile,
    const submission_ticket& _ticket, GstBuffer*& _buffer,
    std::unique_ptr<read_completion>& _completion) {
    if (_buffer != nullptr || _completion || !_owner || _ticket.token_.cycle_id_ == 0 ||
        _ticket.token_.job_id_ == 0 || _ticket.pipeline_pts_ns_ == UINT64_MAX ||
        _ticket.source_epoch_ != _descriptor.session_epoch_ ||
        _ticket.source_frame_id_ != _descriptor.buffer_id_ ||
        _ticket.source_pts_ns_ == UINT64_MAX || _ticket.source_pts_ns_ != _descriptor.pts_ns_) {
        return {status_code::invalid_argument, "invalid output, owner or source ticket"};
    }
    auto observer = std::make_unique<read_completion>();
    observer->signal_ = std::make_shared<input_release_signal>();
    observer->token_ = _ticket.token_;
    auto tracked = std::make_shared<tracked_frame_owner>();
    tracked->signal_ = observer->signal_;
    tracked->owner_ = _owner;
    const auto wrapped = vqec_vision_ai_qcom_dmbrg_wrap_frame(
        _descriptor, _frame_fd, tracked, _profile, _buffer);
    if (wrapped.code_ != status_code::ok) {
        return wrapped;
    }
    GST_BUFFER_PTS(_buffer) = _ticket.pipeline_pts_ns_;
    GST_BUFFER_DTS(_buffer) = GST_CLOCK_TIME_NONE;
    _completion = std::move(observer);
    return {};
}

status read_completion::vqec_vision_ai_qcom_dmbrg_poll_input(submission_window& _window) {
    if (!signal_) {
        return {status_code::invalid_state, "completion observer is not bound to a frame"};
    }
    if (reported_) {
        return {};  // Idempotent polling, no second ledger mutation.
    }
    if (!signal_->released_.load(std::memory_order_acquire)) {
        return {status_code::pending, "input memory still retained by a reader"};
    }
    const auto completed = _window.vqec_vision_ai_core_subwn_complete_input(token_);
    if (completed.code_ == status_code::ok) {
        reported_ = true;
    }
    return completed;
}

} // namespace vqec::vision::ai
