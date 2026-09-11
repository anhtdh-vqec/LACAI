#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "vqec_vision_reference_source.hpp"

#include <memory>
#include <utility>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace vqec::vision::ai {
namespace {

struct reference_frame_owner {};

int vqec_vision_ai_refer_rfsrc_open_synthetic_handle() noexcept {
    // Development handle only: it stands in for a FW FD so descriptor plumbing can be
    // exercised. It is never imported, mapped or read by this backend.
    const int fd = ::memfd_create("vqec_vision_ai_reference_source", MFD_CLOEXEC);
    return fd >= 0 ? fd : ::open("/dev/null", O_RDONLY | O_CLOEXEC);
}

}  // namespace

reference_raw_source::reference_raw_source(reference_source_config _config) noexcept
    : config_(_config) {}

reference_raw_source::~reference_raw_source() noexcept {
    if (synthetic_fd_ >= 0) {
        ::close(synthetic_fd_);
    }
}

status reference_raw_source::vqec_vision_ai_ports_rawsr_start(int _timeout_ms) {
    if (_timeout_ms < 1 || _timeout_ms > 60000) {
        return {status_code::invalid_argument, "reference source timeout must be in 1..60000 ms"};
    }
    if (state_ == raw_source_state::running || state_ == raw_source_state::starting) {
        return {status_code::invalid_state, "reference source is already running"};
    }
    if (config_.width_ == 0 || config_.height_ == 0 ||
        config_.width_ % 2 != 0 || config_.height_ % 2 != 0 ||
        config_.fps_numerator_ == 0 || config_.fps_denominator_ == 0) {
        return {status_code::invalid_argument, "reference source profile is invalid"};
    }
    if (synthetic_fd_ < 0) {
        synthetic_fd_ = vqec_vision_ai_refer_rfsrc_open_synthetic_handle();
        if (synthetic_fd_ < 0) {
            return {status_code::io_error, "cannot create reference source synthetic handle"};
        }
    }
    ++epoch_;
    next_buffer_id_ = 1;
    state_ = raw_source_state::running;
    return {};
}

status reference_raw_source::vqec_vision_ai_ports_rawsr_receive(
    raw_frame& _frame, int _timeout_ms) {
    if (_timeout_ms < 0 || _timeout_ms > 60000) {
        return {status_code::invalid_argument, "reference source timeout must be in 0..60000 ms"};
    }
    if (state_ != raw_source_state::running) {
        return {status_code::invalid_state, "reference source is not running"};
    }
    const std::uint64_t buffer_id = next_buffer_id_++;
    const std::uint64_t luma_bytes =
        static_cast<std::uint64_t>(config_.width_) * config_.height_;
    const std::uint64_t duration_ns =
        1000000000ULL * config_.fps_denominator_ / config_.fps_numerator_;

    frame_descriptor descriptor;
    descriptor.buffer_id_ = buffer_id;
    descriptor.session_epoch_ = epoch_;
    descriptor.width_ = config_.width_;
    descriptor.height_ = config_.height_;
    descriptor.offsets_ = {0, static_cast<std::uint32_t>(luma_bytes)};
    descriptor.strides_ = {static_cast<std::int32_t>(config_.width_),
                           static_cast<std::int32_t>(config_.width_)};
    descriptor.view_size_bytes_ = luma_bytes + luma_bytes / 2;
    descriptor.memory_offset_bytes_ = 0;
    descriptor.allocation_size_bytes_ = descriptor.view_size_bytes_;
    descriptor.pts_ns_ = (buffer_id - 1) * duration_ns;
    descriptor.dts_ns_ = UINT64_MAX;
    descriptor.duration_ns_ = duration_ns;

    _frame.descriptor_ = descriptor;
    _frame.native_handle_ = synthetic_fd_;
    _frame.owner_ = std::make_shared<reference_frame_owner>();
    return {};
}

status reference_raw_source::vqec_vision_ai_ports_rawsr_stop(int _timeout_ms) {
    if (_timeout_ms < 0 || _timeout_ms > 60000) {
        return {status_code::invalid_argument, "reference source timeout must be in 0..60000 ms"};
    }
    if (state_ == raw_source_state::idle) {
        return {status_code::invalid_state, "reference source was never started"};
    }
    state_ = raw_source_state::stopped;
    return {};
}

raw_source_state reference_raw_source::vqec_vision_ai_ports_rawsr_get_state() const noexcept {
    return state_;
}

raw_source_profile reference_raw_source::vqec_vision_ai_ports_rawsr_get_profile() const noexcept {
    return {config_.width_, config_.height_, config_.fps_numerator_, config_.fps_denominator_};
}

unsigned reference_raw_source::vqec_vision_ai_ports_rawsr_get_outstanding() const noexcept {
    return 0;
}

}  // namespace vqec::vision::ai
