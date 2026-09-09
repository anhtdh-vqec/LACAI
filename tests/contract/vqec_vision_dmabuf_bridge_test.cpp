#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>

#include <fcntl.h>
#include <unistd.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/video/video.h>

#include "vqec_vision_dmabuf_bridge.hpp"

namespace {

struct test_frame_owner {
    int fd_{-1};
    ~test_frame_owner() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
};

struct test_buffers {
    GstBuffer* original_{nullptr};
    GstBuffer* shallow_{nullptr};
    GstMemory* child_{nullptr};
    ~test_buffers() noexcept {
        if (child_ != nullptr) {
            gst_memory_unref(child_);
        }
        if (shallow_ != nullptr) {
            gst_buffer_unref(shallow_);
        }
        if (original_ != nullptr) {
            gst_buffer_unref(original_);
        }
    }
};

}  // namespace

int main() {
    using vqec::vision::ai::dmabuf_bridge_profile;
    using vqec::vision::ai::frame_descriptor;
    using vqec::vision::ai::status_code;
    using vqec::vision::ai::vqec_vision_ai_qcom_dmbrg_wrap_frame;
    using vqec::vision::ai::vqec_vision_ai_qcom_dmbrg_wrap_tracked_frame;
    gst_init(nullptr, nullptr);
    auto owner = std::make_shared<test_frame_owner>();
    char path[] = "/tmp/vqec_ai_dmabuf_bridge_XXXXXX";
    owner->fd_ = ::mkstemp(path);
    if (owner->fd_ < 0) {
        return 1;
    }
    // Private fixture file is unlinked immediately; no device import or pixel access.
    if (::unlink(path) != 0 || ::ftruncate(owner->fd_, 4096) != 0) {
        return 1;
    }
    const int original_fd = owner->fd_;
    std::weak_ptr<test_frame_owner> lifetime = owner;
    frame_descriptor descriptor;
    descriptor.width_ = 32;
    descriptor.height_ = 8;
    descriptor.offsets_ = {16, 576};
    descriptor.strides_ = {64, 64};
    descriptor.view_size_bytes_ = 1024;
    descriptor.memory_offset_bytes_ = 128;
    descriptor.allocation_size_bytes_ = 4096;
    descriptor.buffer_id_ = 7;
    descriptor.session_epoch_ = 1;
    descriptor.pts_ns_ = 123456;
    descriptor.duration_ns_ = 40000000;
    dmabuf_bridge_profile profile{32, 8, 4096};
    test_buffers buffers;
    auto invalid = descriptor;
    invalid.memory_offset_bytes_ = UINT64_MAX;
    if (vqec_vision_ai_qcom_dmbrg_wrap_frame(
            invalid, original_fd, owner, profile, buffers.original_).code_ !=
            status_code::invalid_argument || buffers.original_ != nullptr ||
        ::fcntl(original_fd, F_GETFD) == -1) {
        return 1;
    }
    invalid = descriptor;
    invalid.offsets_[1] = 16;
    if (vqec_vision_ai_qcom_dmbrg_wrap_frame(
            invalid, original_fd, owner, profile, buffers.original_).code_ !=
        status_code::invalid_argument) {
        return 1;
    }
    if (vqec_vision_ai_qcom_dmbrg_wrap_frame(
            descriptor, original_fd, {}, profile, buffers.original_).code_ !=
        status_code::invalid_argument) {
        return 1;
    }
    vqec::vision::ai::submission_window window;
    vqec::vision::ai::submission_ticket ticket;
    std::unique_ptr<vqec::vision::ai::read_completion> completion;
    if (window.vqec_vision_ai_core_subwn_configure({42, 1, 200000, 100, 1}).code_ !=
            status_code::ok ||
        window.vqec_vision_ai_core_subwn_reserve(
            1, descriptor.buffer_id_, descriptor.pts_ns_, 0, ticket).code_ !=
            status_code::ok) {
        return 1;
    }
    auto wrong_ticket = ticket;
    wrong_ticket.source_frame_id_++;
    if (vqec_vision_ai_qcom_dmbrg_wrap_tracked_frame(
            descriptor, original_fd, owner, profile, wrong_ticket,
            buffers.original_, completion).code_ != status_code::invalid_argument ||
        buffers.original_ != nullptr || completion) {
        return 1;
    }
    wrong_ticket = ticket;
    wrong_ticket.source_pts_ns_++;
    if (vqec_vision_ai_qcom_dmbrg_wrap_tracked_frame(
            descriptor, original_fd, owner, profile, wrong_ticket,
            buffers.original_, completion).code_ != status_code::invalid_argument ||
        buffers.original_ != nullptr || completion) {
        return 1;
    }
    if (vqec_vision_ai_qcom_dmbrg_wrap_tracked_frame(
            descriptor, original_fd, owner, profile, ticket,
            buffers.original_, completion).code_ != status_code::ok || !completion) {
        return 1;
    }
    if (window.vqec_vision_ai_core_subwn_commit(ticket.token_).code_ != status_code::ok ||
        completion->vqec_vision_ai_qcom_dmbrg_poll_input(window).code_ != status_code::pending) {
        return 1;
    }
    if (gst_buffer_n_memory(buffers.original_) != 1 ||
        gst_buffer_get_size(buffers.original_) != 1024 ||
        GST_BUFFER_PTS(buffers.original_) != 200000 ||
        GST_BUFFER_DTS(buffers.original_) != GST_CLOCK_TIME_NONE ||
        GST_BUFFER_DURATION(buffers.original_) != 40000000) {
        return 1;
    }
    auto* metadata = gst_buffer_get_video_meta(buffers.original_);
    auto* memory = gst_buffer_peek_memory(buffers.original_, 0);
    gsize offset = 0;
    gsize allocation = 0;
    if (metadata == nullptr || metadata->format != GST_VIDEO_FORMAT_NV12 ||
        metadata->n_planes != 2 || metadata->offset[0] != 16 || metadata->offset[1] != 576 ||
        metadata->stride[0] != 64 || metadata->stride[1] != 64 ||
        !gst_is_dmabuf_memory(memory) || !GST_MEMORY_IS_READONLY(memory) ||
        gst_memory_get_sizes(memory, &offset, &allocation) != 1024 ||
        offset != 128 || allocation != 4096) {
        return 1;
    }
    const int duplicate_fd = gst_dmabuf_memory_get_fd(memory);
    if (duplicate_fd == original_fd || (::fcntl(duplicate_fd, F_GETFD) & FD_CLOEXEC) == 0) {
        return 1;
    }
    buffers.shallow_ = gst_buffer_copy(buffers.original_);
    buffers.child_ = gst_memory_share(memory, 0, -1);
    if (buffers.shallow_ == nullptr || buffers.child_ == nullptr) {
        return 1;
    }
    owner.reset();
    gst_buffer_unref(buffers.original_);
    buffers.original_ = nullptr;
    if (lifetime.expired()) {
        return 1;
    }
    gst_buffer_unref(buffers.shallow_);
    buffers.shallow_ = nullptr;
    if (lifetime.expired()) {
        return 1;  // A memory child must retain the root even with no original GstBuffer.
    }
    if (window.vqec_vision_ai_core_subwn_complete_result(ticket.token_).code_ != status_code::ok ||
        completion->vqec_vision_ai_qcom_dmbrg_poll_input(window).code_ != status_code::pending ||
        window.vqec_vision_ai_core_subwn_get_outstanding() != 1) {
        return 1;  // Output completion does not prove input release.
    }
    gst_memory_unref(buffers.child_);
    buffers.child_ = nullptr;
    if (!lifetime.expired() || ::fcntl(original_fd, F_GETFD) != -1 || errno != EBADF ||
        ::fcntl(duplicate_fd, F_GETFD) != -1 || errno != EBADF) {
        return 1;
    }
    if (completion->vqec_vision_ai_qcom_dmbrg_poll_input(window).code_ != status_code::ok ||
        window.vqec_vision_ai_core_subwn_get_outstanding() != 0 ||
        completion->vqec_vision_ai_qcom_dmbrg_poll_input(window).code_ != status_code::ok) {
        return 1;
    }
    std::cout << "DMA-BUF wrapper layout/ownership checks passed (no hardware import)\n";
    return 0;
}
