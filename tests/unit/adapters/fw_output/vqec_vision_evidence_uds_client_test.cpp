#include <cassert>
#include <cstring>
#include <filesystem>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "vqec_vision_evidence_uds_client.hpp"

using namespace vqec::vision::ai;

namespace {

evidence_command vqec_vision_ai_unit_euctst_make_command() {
    evidence_command command;
    command.request_id_ = "fire.request.uds";
    command.event_id_ = "fire.event.uds";
    command.event_revision_ = 1U;
    command.source_id_ = "camera.front";
    command.feature_id_ = "fire_smoke_alarm";
    command.schema_id_ = "security.fire_smoke.event";
    command.schema_version_ = "1";
    command.frame_ = {1U, 0U, 1U, 2U, 100U};
    command.occurred_at_ns_ = 100U;
    command.policy_revision_ = 1U;
    command.config_revision_ = 1U;
    command.fields_.push_back({"security.fire_smoke.evidence", "1", "profile",
        1.0F, observation_quality::high});
    return command;
}

int vqec_vision_ai_unit_euctst_open_server(const std::string& _path) {
    const int server = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    assert(server >= 0);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    assert(_path.size() < sizeof(address.sun_path));
    std::memcpy(address.sun_path, _path.c_str(), _path.size() + 1U);
    assert(bind(server, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0);
    assert(listen(server, 1) == 0);
    return server;
}

void vqec_vision_ai_unit_euctst_serve(int _server) {
    const int peer = accept4(_server, nullptr, nullptr, SOCK_CLOEXEC);
    assert(peer >= 0);
    std::vector<std::uint8_t> packet(evidence_transport_limits::g_max_message_bytes);
    for (unsigned request = 0U; request < 2U; ++request) {
        const auto received = recv(peer, packet.data(), packet.size(), 0);
        assert(received > 0);
        evidence_command command;
        assert(vqec_vision_ai_core_evtrn_decode_command(packet.data(),
                   static_cast<std::size_t>(received), command).code_ == status_code::ok);
        if (request == 0U) {
            evidence_receipt receipt;
            receipt.request_id_ = command.request_id_;
            receipt.event_revision_ = command.event_revision_;
            receipt.state_ = evidence_receipt_state::ready;
            receipt.media_id_ = "media.uds.1";
            receipt.actual_begin_ns_ = 90U;
            receipt.actual_end_ns_ = 110U;
            std::vector<std::uint8_t> wire;
            assert(vqec_vision_ai_core_evtrn_encode_receipt(receipt, wire).code_ ==
                   status_code::ok);
            assert(send(peer, wire.data(), wire.size(), MSG_NOSIGNAL) ==
                   static_cast<ssize_t>(wire.size()));
        } else {
            const std::uint8_t malformed[] = {1U, 2U, 3U};
            assert(send(peer, malformed, sizeof(malformed), MSG_NOSIGNAL) ==
                   static_cast<ssize_t>(sizeof(malformed)));
        }
    }
    close(peer);
    close(_server);
}

}  // namespace

int main() {
    char directory_template[] = "/tmp/vqec_vision_evidence_uds_XXXXXX";
    const auto* directory = mkdtemp(directory_template);
    assert(directory != nullptr);
    const auto socket_path = (std::filesystem::path(directory) / "evidence.sock").string();
    const int server = vqec_vision_ai_unit_euctst_open_server(socket_path);
    std::thread receiver(vqec_vision_ai_unit_euctst_serve, server);

    evidence_uds_client client({socket_path, static_cast<std::uint32_t>(getuid()), 1000});
    const auto command = vqec_vision_ai_unit_euctst_make_command();
    evidence_receipt receipt;
    assert(client.vqec_vision_ai_ports_evtrn_exchange(command, receipt).code_ ==
           status_code::ok);
    assert(receipt.media_id_ == "media.uds.1");
    assert(client.vqec_vision_ai_ports_evtrn_exchange(command, receipt).code_ ==
           status_code::protocol_error);
    receiver.join();

    evidence_uds_client offline({socket_path, static_cast<std::uint32_t>(getuid()), 10});
    assert(offline.vqec_vision_ai_ports_evtrn_exchange(command, receipt).code_ ==
           status_code::io_error);
    std::filesystem::remove_all(directory);
    return 0;
}
