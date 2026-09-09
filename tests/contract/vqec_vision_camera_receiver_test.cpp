#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "vqec_vision_frame_source.hpp"
#include "vqec_vision_source_lifecycle.hpp"

namespace {

struct camera_fixture {
    std::string directory_;
    std::string path_;
    int listener_{-1};
    int peer_{-1};
    int replacement_peer_{-1};
    int payload_fd_{-1};

    ~camera_fixture() noexcept {
        for (const int handle : {listener_, peer_, replacement_peer_, payload_fd_}) {
            if (handle >= 0) {
                ::close(handle);
            }
        }
        if (!path_.empty()) {
            ::unlink(path_.c_str());
        }
        if (!directory_.empty()) {
            ::rmdir(directory_.c_str());
        }
    }
};

class lifecycle_rpc final : public vqec::vision::ai::camera_rpc {
public:
    unsigned start_calls_{0};
    unsigned stop_calls_{0};
    bool should_timeout_start_{false};
    bool should_timeout_stop_{false};
    std::string width_{"640"};

    vqec::vision::ai::status vqec_vision_ai_camer_cmrpc_call(
        const std::string& _method, const vqec::vision::ai::camera_fields& _request,
        int _timeout_ms, vqec::vision::ai::camera_fields& _response) override {
        using vqec::vision::ai::status_code;
        _response.clear();
        if (_timeout_ms < 1 || _request.at("stream_id") != "third") {
            return {status_code::invalid_argument, "unexpected lifecycle test request"};
        }
        if (_method == "StartStream") {
            ++start_calls_;
            if (should_timeout_start_) {
                return {status_code::timeout, "injected uncertain acquisition"};
            }
            _response = {{"code", "0"}, {"stream_handle", "test_handle"}, {"codec", "RAW"},
                         {"width", width_}, {"height", "480"}, {"fps", "25"}};
        } else if (_method == "StopStream") {
            ++stop_calls_;
            if (should_timeout_stop_) {
                return {status_code::timeout, "injected uncertain release"};
            }
            _response = {{"code", "0"}};
        } else {
            return {status_code::unsupported, "unexpected lifecycle test method"};
        }
        return {};
    }
};

bool vqec_vision_ai_ctest_crtst_send_frame(
    int _socket_fd, int _payload_fd, std::uint64_t _token, bool _extra_fd) {
    // Native-endian fixture assembled at FW-defined offsets, no packed struct cast.
    std::array<std::uint8_t, 104> bytes{};
    const auto put = [&bytes](std::size_t _offset, auto _value) {
        std::memcpy(bytes.data() + _offset, &_value, sizeof(_value));
    };
    put(0, _token);
    put(8, std::uint32_t{640});
    put(12, std::uint32_t{480});
    put(16, std::uint32_t{123});
    put(20, std::uint32_t{2});
    put(28, std::uint32_t{640 * 480});
    put(40, std::int32_t{640});
    put(44, std::int32_t{640});
    put(56, std::uint64_t{640 * 480 * 3 / 2});
    put(72, std::uint64_t{640 * 480 * 3 / 2});
    iovec payload{bytes.data(), bytes.size()};
    alignas(cmsghdr) std::array<unsigned char, CMSG_SPACE(2 * sizeof(int))> control{};
    msghdr message{};
    message.msg_iov = &payload;
    message.msg_iovlen = 1;
    message.msg_control = control.data();
    const std::size_t fd_bytes = (_extra_fd ? 2 : 1) * sizeof(int);
    message.msg_controllen = CMSG_SPACE(fd_bytes);
    auto* header = CMSG_FIRSTHDR(&message);
    header->cmsg_level = SOL_SOCKET;
    header->cmsg_type = SCM_RIGHTS;
    header->cmsg_len = CMSG_LEN(fd_bytes);
    const std::array<int, 2> handles{_payload_fd, _payload_fd};
    std::memcpy(CMSG_DATA(header), handles.data(), fd_bytes);
    return ::sendmsg(_socket_fd, &message, MSG_NOSIGNAL) ==
        static_cast<ssize_t>(bytes.size());
}

}  // namespace

int main() {
    using vqec::vision::ai::camera_source_config;
    using vqec::vision::ai::frame_source;
    using vqec::vision::ai::received_frame;
    using vqec::vision::ai::status_code;
    camera_fixture fixture;
    char temp_path[] = "/tmp/vqec_ai_receiver_XXXXXX";
    const auto* directory = ::mkdtemp(temp_path);
    if (directory == nullptr) {
        return 1;
    }
    fixture.directory_ = directory;
    fixture.path_ = fixture.directory_ + "/0_third_ai.sock";
    fixture.listener_ = ::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, fixture.path_.c_str(), fixture.path_.size() + 1);
    if (fixture.listener_ < 0 ||
        ::bind(fixture.listener_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) ||
        ::listen(fixture.listener_, 4)) {
        return 1;
    }
    // Transport-only fixture: /dev/zero is NOT a DMA-BUF hardware compatibility test.
    fixture.payload_fd_ = ::open("/dev/zero", O_RDONLY | O_CLOEXEC);
    camera_source_config config;
    config.socket_path_ = fixture.path_;
    config.producer_uid_ = static_cast<std::uint32_t>(::getuid());
    config.limits_.nv12_format_value_ = 123;
    frame_source source;
    if (fixture.payload_fd_ < 0 ||
        source.vqec_vision_ai_camer_frsrc_connect(config).code_ != status_code::ok) {
        return 1;
    }
    fixture.peer_ = ::accept4(fixture.listener_, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (fixture.peer_ < 0) {
        return 1;
    }
    std::shared_ptr<const received_frame> frame;
    if (source.vqec_vision_ai_camer_frsrc_receive(frame, 0).code_ != status_code::timeout ||
        !vqec_vision_ai_ctest_crtst_send_frame(fixture.peer_, fixture.payload_fd_, 1, false) ||
        source.vqec_vision_ai_camer_frsrc_receive(frame, 100).code_ != status_code::ok) {
        return 1;
    }
    const int borrowed_fd = frame->vqec_vision_ai_camer_frsrc_get_fd();
    const auto old_epoch = frame->vqec_vision_ai_camer_frsrc_get_descriptor().session_epoch_;
    if ((::fcntl(borrowed_fd, F_GETFD) & FD_CLOEXEC) == 0) {
        return 1;
    }
    auto reader = frame;
    frame.reset();
    source.vqec_vision_ai_camer_frsrc_disconnect();
    if (source.vqec_vision_ai_camer_frsrc_get_outstanding() != 1) {
        return 1;  // Detached session still has a reader.
    }
    std::uint64_t ack = 0;
    if (::recv(fixture.peer_, &ack, sizeof(ack), MSG_DONTWAIT) != -1 || errno != EAGAIN) {
        return 1;  // Reader must keep both buffer and old session alive, no early ACK/EOF.
    }
    if (source.vqec_vision_ai_camer_frsrc_connect(config).code_ != status_code::ok) {
        return 1;
    }
    fixture.replacement_peer_ =
        ::accept4(fixture.listener_, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (fixture.replacement_peer_ < 0 ||
        !vqec_vision_ai_ctest_crtst_send_frame(
            fixture.replacement_peer_, fixture.payload_fd_, 1, false) ||
        source.vqec_vision_ai_camer_frsrc_receive(frame, 100).code_ != status_code::ok ||
        frame->vqec_vision_ai_camer_frsrc_get_descriptor().session_epoch_ <= old_epoch) {
        return 1;
    }
    reader.reset();
    if (::recv(fixture.peer_, &ack, sizeof(ack), MSG_DONTWAIT) !=
            static_cast<ssize_t>(sizeof(ack)) || ack != 1 ||
        ::fcntl(borrowed_fd, F_GETFD) != -1 || errno != EBADF) {
        return 1;
    }
    if (::recv(fixture.replacement_peer_, &ack, sizeof(ack), MSG_DONTWAIT) != -1 ||
        errno != EAGAIN) {
        return 1;  // Old ACK must not appear on replacement connection.
    }
    std::array<std::shared_ptr<const received_frame>, 3> additional_readers;
    for (std::size_t index = 0; index < additional_readers.size(); ++index) {
        if (!vqec_vision_ai_ctest_crtst_send_frame(
                fixture.replacement_peer_, fixture.payload_fd_, index + 2, false) ||
            source.vqec_vision_ai_camer_frsrc_receive(additional_readers[index], 100).code_ !=
                status_code::ok) {
            return 1;
        }
    }
    std::shared_ptr<const received_frame> rejected;
    if (source.vqec_vision_ai_camer_frsrc_receive(rejected, 0).code_ !=
        status_code::resource_exhausted) {
        return 1;
    }
    for (std::size_t index = 0; index < additional_readers.size(); ++index) {
        additional_readers[index].reset();
        if (::recv(fixture.replacement_peer_, &ack, sizeof(ack), MSG_DONTWAIT) !=
                static_cast<ssize_t>(sizeof(ack)) || ack != index + 2) {
            return 1;
        }
    }
    if (!vqec_vision_ai_ctest_crtst_send_frame(
            fixture.replacement_peer_, fixture.payload_fd_, 5, true) ||
        source.vqec_vision_ai_camer_frsrc_receive(rejected, 100).code_ !=
            status_code::protocol_error || rejected ||
        source.vqec_vision_ai_camer_frsrc_is_healthy()) {
        return 1;
    }
    frame.reset();
    if (::recv(fixture.replacement_peer_, &ack, sizeof(ack), MSG_DONTWAIT) !=
            static_cast<ssize_t>(sizeof(ack)) ||
        ack != 1) {
        return 1;  // Valid outstanding owner still ACKs its original session after parser fault.
    }
    source.vqec_vision_ai_camer_frsrc_disconnect();
    if (source.vqec_vision_ai_camer_frsrc_get_outstanding() != 0) {
        return 1;
    }

    using vqec::vision::ai::camera_lifecycle_config;
    using vqec::vision::ai::camera_source_state;
    using vqec::vision::ai::source_lifecycle;
    camera_lifecycle_config lifecycle_config;
    lifecycle_config.acquire_ = {0, 0, "ai:test_cycle_1", "test_cycle_1:start"};
    lifecycle_config.stop_request_id_ = "test_cycle_1:stop";
    lifecycle_config.media_ = config;
    auto rpc = std::make_shared<lifecycle_rpc>();
    source_lifecycle lifecycle(rpc, lifecycle_config);
    if (lifecycle.vqec_vision_ai_camer_srclc_start(100).code_ != status_code::ok ||
        lifecycle.vqec_vision_ai_camer_srclc_start(100).code_ != status_code::ok ||
        rpc->start_calls_ != 1) {
        return 1;
    }
    ::close(fixture.peer_);
    fixture.peer_ = ::accept4(fixture.listener_, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (fixture.peer_ < 0 ||
        !vqec_vision_ai_ctest_crtst_send_frame(fixture.peer_, fixture.payload_fd_, 1, false) ||
        lifecycle.vqec_vision_ai_camer_srclc_receive(frame, 100).code_ != status_code::ok ||
        lifecycle.vqec_vision_ai_camer_srclc_get_outstanding() != 1) {
        return 1;
    }
    if (lifecycle.vqec_vision_ai_camer_srclc_stop(100).code_ != status_code::pending ||
        lifecycle.vqec_vision_ai_camer_srclc_stop(100).code_ != status_code::pending ||
        rpc->stop_calls_ != 0 ||
        lifecycle.vqec_vision_ai_camer_srclc_receive(rejected, 0).code_ !=
            status_code::invalid_state) {
        return 1;
    }
    frame.reset();
    if (::recv(fixture.peer_, &ack, sizeof(ack), MSG_DONTWAIT) !=
            static_cast<ssize_t>(sizeof(ack)) || ack != 1) {
        return 1;
    }
    rpc->should_timeout_stop_ = true;
    if (lifecycle.vqec_vision_ai_camer_srclc_stop(100).code_ != status_code::timeout ||
        ::recv(fixture.peer_, &ack, sizeof(ack), MSG_DONTWAIT) != 0 ||
        lifecycle.vqec_vision_ai_camer_srclc_get_state() != camera_source_state::releasing) {
        return 1;
    }
    rpc->should_timeout_stop_ = false;
    if (lifecycle.vqec_vision_ai_camer_srclc_stop(100).code_ != status_code::ok ||
        lifecycle.vqec_vision_ai_camer_srclc_get_state() != camera_source_state::stopped ||
        lifecycle.vqec_vision_ai_camer_srclc_start(100).code_ != status_code::invalid_state) {
        return 1;
    }

    // Stop after an uncertain Start first recovers the handle, without opening media.
    lifecycle_config.acquire_.request_id_ = "test_cycle_2:start";
    lifecycle_config.stop_request_id_ = "test_cycle_2:stop";
    auto uncertain_rpc = std::make_shared<lifecycle_rpc>();
    uncertain_rpc->should_timeout_start_ = true;
    source_lifecycle uncertain(uncertain_rpc, lifecycle_config);
    if (uncertain.vqec_vision_ai_camer_srclc_start(100).code_ != status_code::timeout) {
        return 1;
    }
    uncertain_rpc->should_timeout_start_ = false;
    if (uncertain.vqec_vision_ai_camer_srclc_stop(100).code_ != status_code::pending ||
        uncertain_rpc->start_calls_ != 2 || uncertain_rpc->stop_calls_ != 0 ||
        uncertain.vqec_vision_ai_camer_srclc_stop(100).code_ != status_code::ok ||
        uncertain_rpc->stop_calls_ != 1) {
        return 1;
    }

    // A geometry change never reaches downstream readers under the old profile.
    lifecycle_config.acquire_.request_id_ = "test_cycle_3:start";
    lifecycle_config.stop_request_id_ = "test_cycle_3:stop";
    auto changed_rpc = std::make_shared<lifecycle_rpc>();
    changed_rpc->width_ = "1280";
    source_lifecycle changed(changed_rpc, lifecycle_config);
    if (changed.vqec_vision_ai_camer_srclc_start(100).code_ != status_code::ok) {
        return 1;
    }
    ::close(fixture.peer_);
    fixture.peer_ = ::accept4(fixture.listener_, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (fixture.peer_ < 0 ||
        !vqec_vision_ai_ctest_crtst_send_frame(fixture.peer_, fixture.payload_fd_, 1, false) ||
        changed.vqec_vision_ai_camer_srclc_receive(frame, 100).code_ != status_code::unsupported ||
        frame || changed.vqec_vision_ai_camer_srclc_get_outstanding() != 0 ||
        changed.vqec_vision_ai_camer_srclc_stop(100).code_ != status_code::ok) {
        return 1;
    }
    std::cout << "camera receiver and combined lifecycle checks passed\n";
    return 0;
}
