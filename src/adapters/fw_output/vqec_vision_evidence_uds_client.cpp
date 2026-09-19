#include "vqec_vision_evidence_uds_client.hpp"

#include <cerrno>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace vqec::vision::ai {
namespace {

constexpr int g_evidence_maximum_io_timeout_ms = 60000;

status vqec_vision_ai_fwout_evuds_socket_error(const char* _operation) {
    return {errno == EAGAIN || errno == EWOULDBLOCK ?
            status_code::timeout : status_code::io_error,
        std::string(_operation) + ": " + std::strerror(errno)};
}

}  // namespace

evidence_uds_client::evidence_uds_client(evidence_uds_client_config _config)
    : config_(std::move(_config)) {}

evidence_uds_client::~evidence_uds_client() noexcept {
    vqec_vision_ai_fwout_evuds_disconnect();
}

void evidence_uds_client::vqec_vision_ai_fwout_evuds_disconnect() noexcept {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
}

status evidence_uds_client::vqec_vision_ai_fwout_evuds_connect() {
    if (socket_fd_ >= 0) {
        return {};
    }
    sockaddr_un address{};
    if (config_.socket_path_.empty() || config_.socket_path_.front() != '/' ||
        config_.socket_path_.size() >= sizeof(address.sun_path) ||
        config_.io_timeout_ms_ <= 0 ||
        config_.io_timeout_ms_ > g_evidence_maximum_io_timeout_ms) {
        return {status_code::invalid_argument,
            "invalid evidence UDS client configuration"};
    }
    const int candidate = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (candidate < 0) {
        return vqec_vision_ai_fwout_evuds_socket_error("create evidence socket");
    }
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, config_.socket_path_.c_str(),
        config_.socket_path_.size() + 1U);
    timeval timeout{};
    timeout.tv_sec = config_.io_timeout_ms_ / 1000;
    timeout.tv_usec = (config_.io_timeout_ms_ % 1000) * 1000;
    if (setsockopt(candidate, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0 ||
        setsockopt(candidate, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        connect(candidate, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        const auto failure = vqec_vision_ai_fwout_evuds_socket_error(
            "connect evidence socket");
        close(candidate);
        return failure;
    }
    ucred credentials{};
    socklen_t credentials_bytes = sizeof(credentials);
    if (getsockopt(candidate, SOL_SOCKET, SO_PEERCRED,
            &credentials, &credentials_bytes) != 0 ||
        credentials_bytes != sizeof(credentials) ||
        credentials.uid != config_.expected_peer_uid_) {
        close(candidate);
        return {status_code::unauthorized,
            "evidence receiver peer credentials do not match"};
    }
    socket_fd_ = candidate;
    return {};
}

status evidence_uds_client::vqec_vision_ai_ports_evtrn_exchange(
    const evidence_command& _command, evidence_receipt& _receipt) {
    std::vector<std::uint8_t> command_wire;
    auto result = vqec_vision_ai_core_evtrn_encode_command(_command, command_wire);
    if (result.code_ != status_code::ok) {
        return result;
    }
    result = vqec_vision_ai_fwout_evuds_connect();
    if (result.code_ != status_code::ok) {
        return result;
    }
    const auto sent = send(socket_fd_, command_wire.data(), command_wire.size(), MSG_NOSIGNAL);
    if (sent < 0 || static_cast<std::size_t>(sent) != command_wire.size()) {
        const auto failure = sent < 0 ?
            vqec_vision_ai_fwout_evuds_socket_error("send evidence command") :
            status{status_code::io_error, "evidence command packet was truncated"};
        vqec_vision_ai_fwout_evuds_disconnect();
        return failure;
    }
    try {
        std::vector<std::uint8_t> receipt_wire(
            evidence_transport_limits::g_max_message_bytes);
        const auto received = recv(socket_fd_, receipt_wire.data(), receipt_wire.size(),
            MSG_TRUNC);
        if (received < 0) {
            const auto failure = vqec_vision_ai_fwout_evuds_socket_error(
                "receive evidence receipt");
            vqec_vision_ai_fwout_evuds_disconnect();
            return failure;
        }
        if (received == 0 ||
            static_cast<std::size_t>(received) > receipt_wire.size()) {
            vqec_vision_ai_fwout_evuds_disconnect();
            return {status_code::protocol_error,
                "evidence receipt packet is empty or oversized"};
        }
        evidence_receipt candidate;
        result = vqec_vision_ai_core_evtrn_decode_receipt(receipt_wire.data(),
            static_cast<std::size_t>(received), candidate);
        if (result.code_ == status_code::ok) {
            result = vqec_vision_ai_core_evtrn_validate_receipt(candidate, _command);
        }
        if (result.code_ != status_code::ok) {
            vqec_vision_ai_fwout_evuds_disconnect();
            return result;
        }
        _receipt = std::move(candidate);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "evidence receipt allocation failed"};
    }
    return {};
}

}  // namespace vqec::vision::ai
