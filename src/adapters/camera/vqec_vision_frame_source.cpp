#include "vqec_vision_frame_source.hpp"

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <utility>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace vqec::vision::ai {

struct camera_reader_count {
    std::atomic<unsigned> outstanding_{0};
};

struct camera_session {
    int socket_fd_{-1};
    legacy_frame_limits limits_;
    std::uint64_t epoch_{0};
    std::uint64_t last_buffer_id_{0};
    std::atomic<unsigned> outstanding_{0};
    std::atomic<bool> healthy_{true};
    std::shared_ptr<camera_reader_count> reader_count_;

    ~camera_session() noexcept {
        if (socket_fd_ >= 0) {
            ::close(socket_fd_);
        }
    }
};

namespace {

struct received_fds {
    std::array<int, 16> values_{};
    std::size_t count_{0};

    ~received_fds() noexcept {
        for (std::size_t index = 0; index < count_; ++index) {
            if (values_[index] >= 0) {
                ::close(values_[index]);
            }
        }
    }
};

}  // namespace

received_frame::~received_frame() noexcept {
    if (frame_fd_ >= 0) {
        // The final owner is the completion boundary, not receive or appsrc push.
        const auto token = descriptor_.buffer_id_;
        const auto sent = ::send(session_->socket_fd_, &token, sizeof(token),
                                 MSG_NOSIGNAL | MSG_DONTWAIT);
        if (sent != static_cast<ssize_t>(sizeof(token))) {
            // No blocking retry in destruction and no ACK on a replacement session.
            session_->healthy_.store(false);
        }
        ::close(frame_fd_);
        session_->outstanding_.fetch_sub(1);
        const auto readers = session_->reader_count_;
        session_.reset();
        // Publish drain completion only after ACK/FD close/session release above.
        readers->outstanding_.fetch_sub(1);
    }
}

int received_frame::vqec_vision_ai_camer_frsrc_get_fd() const noexcept {
    return frame_fd_;
}

const frame_descriptor& received_frame::vqec_vision_ai_camer_frsrc_get_descriptor() const noexcept {
    return descriptor_;
}

frame_source::frame_source() : reader_count_(std::make_shared<camera_reader_count>()) {}

status frame_source::vqec_vision_ai_camer_frsrc_connect(const camera_source_config& _config) {
    if (session_) {
        return {status_code::invalid_state, "detach current session before connecting"};
    }
    if (_config.producer_uid_ == UINT32_MAX || _config.limits_.nv12_format_value_ == 0 ||
        _config.limits_.max_width_ == 0 || _config.limits_.max_height_ == 0 ||
        _config.limits_.max_allocation_bytes_ == 0 || next_epoch_ == UINT64_MAX) {
        return {status_code::invalid_argument, "producer identity, ABI or limits are invalid"};
    }
    if (_config.socket_dir_.empty() || _config.socket_dir_.front() != '/' ||
        _config.socket_dir_.find('\0') != std::string::npos) {
        return {status_code::invalid_argument, "socket directory must be an absolute Linux path"};
    }
    const std::string path = _config.socket_dir_ + "/0_third_ai" +
        (_config.camera_id_ == 0 ? std::string{} : "_cam" + std::to_string(_config.camera_id_)) +
        ".sock";
    sockaddr_un address{};
    if (path.size() >= sizeof(address.sun_path)) {
        return {status_code::invalid_argument, "camera socket path exceeds sun_path"};
    }
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
    auto candidate = std::make_shared<camera_session>();
    candidate->socket_fd_ = ::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (candidate->socket_fd_ < 0) {
        return {status_code::io_error, "cannot create camera socket"};
    }
    if (::connect(candidate->socket_fd_, reinterpret_cast<const sockaddr*>(&address),
                  sizeof(address)) != 0) {
        const int connect_error = errno;
        if (connect_error == EAGAIN || connect_error == EINPROGRESS || connect_error == EINTR) {
            return {status_code::timeout, "connect pending; retry with a fresh session"};
        }
        return {status_code::source_lost, "camera socket connection failed"};
    }
    ucred credentials{};
    socklen_t credentials_size = sizeof(credentials);
    if (::getsockopt(candidate->socket_fd_, SOL_SOCKET, SO_PEERCRED,
                     &credentials, &credentials_size) != 0 ||
        credentials_size != sizeof(credentials) || credentials.uid != _config.producer_uid_) {
        return {status_code::unauthorized, "camera producer UID does not match deployment policy"};
    }
    candidate->limits_ = _config.limits_;
    candidate->reader_count_ = reader_count_;
    candidate->epoch_ = next_epoch_++;
    session_ = std::move(candidate);
    return {};
}

status frame_source::vqec_vision_ai_camer_frsrc_receive(
    std::shared_ptr<const received_frame>& _frame, int _timeout_ms) {
    if (_frame || _timeout_ms < 0 || _timeout_ms > 60000) {
        return {status_code::invalid_argument, "output must be empty and timeout in 0..60000 ms"};
    }
    if (!session_ || !session_->healthy_.load()) {
        return {status_code::source_lost, "camera session absent or faulted"};
    }
    if (reader_count_->outstanding_.load() >= 4) {
        return {status_code::resource_exhausted, "four camera frame leases are already live"};
    }
    // Allocate before acquiring any FD. Allocation failure cannot orphan a received buffer.
    auto frame = std::make_shared<received_frame>();
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(_timeout_ms);
    pollfd descriptor{session_->socket_fd_, POLLIN, 0};
    int wait_ms = _timeout_ms;
    int ready = 0;
    do {
        descriptor.revents = 0;
        ready = ::poll(&descriptor, 1, wait_ms);
        if (ready >= 0 || errno != EINTR) {
            break;
        }
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) {
            return {status_code::timeout, "camera receive deadline elapsed"};
        }
        wait_ms = static_cast<int>(remaining);
    } while (true);
    if (ready == 0) {
        return {status_code::timeout, "no camera frame before deadline"};
    }
    // Even readable data after HUP is unsafe: FW may already have unpinned the buffer.
    if (ready < 0 || (descriptor.revents & (POLLHUP | POLLERR | POLLNVAL)) != 0) {
        session_->healthy_.store(false);
        return {status_code::source_lost, "camera socket failed or disconnected"};
    }
    std::array<std::uint8_t, legacy_wire_layout::g_header_bytes> packet{};
    alignas(cmsghdr) std::array<unsigned char, CMSG_SPACE(16 * sizeof(int))> control{};
    iovec payload{packet.data(), packet.size()};
    msghdr message{};
    message.msg_iov = &payload;
    message.msg_iovlen = 1;
    message.msg_control = control.data();
    message.msg_controllen = control.size();
    const auto bytes = ::recvmsg(session_->socket_fd_, &message,
                                 MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
    if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
        return {status_code::timeout, "camera receive would block or was interrupted"};
    }
    received_fds handles;
    bool ancillary_valid = true;
    unsigned rights_messages = 0;
    if (bytes >= 0) {
        for (auto* header = CMSG_FIRSTHDR(&message); header != nullptr;
             header = CMSG_NXTHDR(&message, header)) {
            if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS ||
                header->cmsg_len < CMSG_LEN(0)) {
                ancillary_valid = false;
                continue;
            }
            ++rights_messages;
            const auto data_bytes = header->cmsg_len - CMSG_LEN(0);
            if (data_bytes % sizeof(int) != 0) {
                ancillary_valid = false;
            }
            for (std::size_t index = 0; index < data_bytes / sizeof(int); ++index) {
                int handle = -1;
                std::memcpy(&handle, CMSG_DATA(header) + index * sizeof(int), sizeof(handle));
                if (handles.count_ < handles.values_.size()) {
                    handles.values_[handles.count_++] = handle;
                } else {
                    ::close(handle);
                    ancillary_valid = false;
                }
            }
        }
    }
    if (bytes <= 0) {
        session_->healthy_.store(false);
        return {status_code::source_lost, "camera recvmsg failed or returned EOF"};
    }
    if (bytes != static_cast<ssize_t>(packet.size()) ||
        (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) != 0 || !ancillary_valid ||
        rights_messages != 1 || handles.count_ != 1) {
        session_->healthy_.store(false);
        return {status_code::protocol_error, "expected one complete legacy header and one FD"};
    }
    const auto decoded = vqec_vision_ai_camer_lwire_decode_frame(
        packet.data(), packet.size(), session_->limits_, frame->descriptor_);
    if (decoded.code_ != status_code::ok) {
        session_->healthy_.store(false);
        return decoded;
    }
    if (frame->descriptor_.buffer_id_ <= session_->last_buffer_id_) {
        session_->healthy_.store(false);
        return {status_code::protocol_error, "duplicate or non-increasing buffer token"};
    }
    frame->descriptor_.session_epoch_ = session_->epoch_;
    frame->session_ = session_;
    frame->frame_fd_ = handles.values_[0];
    handles.values_[0] = -1;
    session_->last_buffer_id_ = frame->descriptor_.buffer_id_;
    session_->outstanding_.fetch_add(1);
    reader_count_->outstanding_.fetch_add(1);
    _frame = std::move(frame);
    return {};
}

void frame_source::vqec_vision_ai_camer_frsrc_disconnect() noexcept {
    session_.reset();
}

bool frame_source::vqec_vision_ai_camer_frsrc_is_healthy() const noexcept {
    return session_ && session_->healthy_.load();
}

unsigned frame_source::vqec_vision_ai_camer_frsrc_get_outstanding() const noexcept {
    return reader_count_->outstanding_.load();
}

}  // namespace vqec::vision::ai
