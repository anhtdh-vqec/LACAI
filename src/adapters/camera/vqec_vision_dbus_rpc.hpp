#ifndef VQEC_VISION_AI_CAMERA_DBUS_RPC_HPP
#define VQEC_VISION_AI_CAMERA_DBUS_RPC_HPP

#include <memory>

#include "vqec_vision_camera_rpc.hpp"

namespace vqec::vision::ai {

class dbus_rpc final : public camera_rpc {
public:
    dbus_rpc();
    ~dbus_rpc() override;
    dbus_rpc(const dbus_rpc&) = delete;
    dbus_rpc& operator=(const dbus_rpc&) = delete;

    // Serialized setup, not a frame-thread operation. Bus acquisition has no explicit deadline.
    [[nodiscard]] status vqec_vision_ai_camer_dbrpc_open(bool _use_session_bus = false);
    [[nodiscard]] status vqec_vision_ai_camer_cmrpc_call(
        const std::string& _method, const camera_fields& _request,
        int _timeout_ms, camera_fields& _response) override;

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CAMERA_DBUS_RPC_HPP
