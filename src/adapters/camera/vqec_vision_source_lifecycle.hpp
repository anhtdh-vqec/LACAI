#ifndef VQEC_VISION_AI_CAMERA_SOURCE_LIFECYCLE_HPP
#define VQEC_VISION_AI_CAMERA_SOURCE_LIFECYCLE_HPP

#include <memory>
#include <string>

#include "vqec_vision_camera_control.hpp"
#include "vqec_vision_frame_source.hpp"

namespace vqec::vision::ai {

enum class camera_source_state {
    idle, starting, connecting, running, draining, releasing, stopped
};

struct camera_lifecycle_config {
    camera_acquire_request acquire_;
    camera_source_config media_;
    std::string stop_request_id_;
    std::uint32_t max_fps_{240};
};

// Linux-only, serialized methods. One acquisition cycle, no destructor RPC.
// Caller must reach stopped before destruction; pending is not hardware completion.
class source_lifecycle {
public:
    source_lifecycle(std::shared_ptr<camera_rpc> _rpc, camera_lifecycle_config _config);
    source_lifecycle(const source_lifecycle&) = delete;
    source_lifecycle& operator=(const source_lifecycle&) = delete;

    // Retry connecting without duplicating StartStream; no internal sleep.
    [[nodiscard]] status vqec_vision_ai_camer_srclc_start(int _timeout_ms);
    [[nodiscard]] status vqec_vision_ai_camer_srclc_receive(
        std::shared_ptr<const received_frame>& _frame, int _timeout_ms);
    // One progress step, at most one RPC. pending means retry after readers/reconcile progress.
    [[nodiscard]] status vqec_vision_ai_camer_srclc_stop(int _timeout_ms);
    [[nodiscard]] camera_source_state vqec_vision_ai_camer_srclc_get_state() const noexcept;
    [[nodiscard]] unsigned vqec_vision_ai_camer_srclc_get_outstanding() const noexcept;
    [[nodiscard]] const camera_stream_profile&
    vqec_vision_ai_camer_srclc_get_profile() const noexcept;

private:
    camera_lifecycle_config config_;
    camera_control control_;
    frame_source source_;
    camera_source_state state_{camera_source_state::idle};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CAMERA_SOURCE_LIFECYCLE_HPP
