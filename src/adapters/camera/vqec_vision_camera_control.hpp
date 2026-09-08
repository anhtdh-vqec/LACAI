#ifndef VQEC_VISION_AI_CAMERA_CAMERA_CONTROL_HPP
#define VQEC_VISION_AI_CAMERA_CAMERA_CONTROL_HPP

#include <cstdint>
#include <memory>
#include <string>

#include "vqec_vision_camera_rpc.hpp"

namespace vqec::vision::ai {

enum class camera_lease_state { idle, start_pending, acquired, stop_pending };

struct camera_acquire_request {
    std::uint32_t camera_id_{0};
    std::uint32_t channel_id_{0};
    std::string consumer_id_;
    std::string request_id_;
};

struct camera_stream_profile {
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::uint32_t fps_{0};
    bool is_valid_{false};
};

// One serialized control owner. No destructor RPC; explicitly reconcile before destruction.
class camera_control {
public:
    explicit camera_control(std::shared_ptr<camera_rpc> _rpc);
    camera_control(const camera_control&) = delete;
    camera_control& operator=(const camera_control&) = delete;

    [[nodiscard]] status vqec_vision_ai_camer_cctrl_start(
        const camera_acquire_request& _request, int _timeout_ms);
    // Precondition: every hardware reader drained and receiver detached by caller.
    [[nodiscard]] status vqec_vision_ai_camer_cctrl_stop(
        const std::string& _request_id, int _timeout_ms);
    [[nodiscard]] camera_lease_state vqec_vision_ai_camer_cctrl_get_state() const noexcept;
    [[nodiscard]] const camera_stream_profile&
    vqec_vision_ai_camer_cctrl_get_profile() const noexcept;
    [[nodiscard]] const std::string& vqec_vision_ai_camer_cctrl_get_handle() const noexcept;

private:
    std::shared_ptr<camera_rpc> rpc_;
    camera_lease_state state_{camera_lease_state::idle};
    camera_acquire_request request_;
    std::string stop_request_id_;
    std::string handle_;
    camera_stream_profile profile_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CAMERA_CAMERA_CONTROL_HPP
