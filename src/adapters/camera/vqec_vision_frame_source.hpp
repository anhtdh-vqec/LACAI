#ifndef VQEC_VISION_AI_CAMERA_FRAME_SOURCE_HPP
#define VQEC_VISION_AI_CAMERA_FRAME_SOURCE_HPP

#include <cstdint>
#include <memory>
#include <string>

#include "vqec_vision_legacy_wire.hpp"

namespace vqec::vision::ai {

struct camera_session;
struct camera_reader_count;

struct camera_source_config {
    // Exact activation-time route; frame receive must not infer product topology.
    std::string socket_path_;
    // Required deployment identity; UINT32_MAX is an unset sentinel.
    std::uint32_t producer_uid_{UINT32_MAX};
    legacy_frame_limits limits_;
};

class received_frame {
public:
    received_frame() = default;
    ~received_frame() noexcept;
    received_frame(const received_frame&) = delete;
    received_frame& operator=(const received_frame&) = delete;

    // Borrow only. Retain shared received_frame through EVERY reader's completion.
    [[nodiscard]] int vqec_vision_ai_camer_frsrc_get_fd() const noexcept;
    [[nodiscard]] const frame_descriptor&
    vqec_vision_ai_camer_frsrc_get_descriptor() const noexcept;

private:
    friend class frame_source;
    int frame_fd_{-1};
    frame_descriptor descriptor_;
    std::shared_ptr<camera_session> session_;
};

// Linux-only implementation. Caller serializes source methods/destruction.
// Frame owners may be released from other threads; their ACKs stay on the old socket.
// Allocation failures may throw std::bad_alloc; never call across a C/vendor callback uncaught.
class frame_source {
public:
    frame_source();
    ~frame_source() = default;
    frame_source(const frame_source&) = delete;
    frame_source& operator=(const frame_source&) = delete;

    // One nonblocking connect attempt. No implicit StartStream or retry loop.
    [[nodiscard]] status vqec_vision_ai_camer_frsrc_connect(
        const camera_source_config& _config);
    // 0..60000 ms; output must be empty; max 4 live frames. Failure leaves output empty.
    [[nodiscard]] status vqec_vision_ai_camer_frsrc_receive(
        std::shared_ptr<const received_frame>& _frame, int _timeout_ms);
    // Detach only, not cancel. Old frames retain socket until their final release.
    void vqec_vision_ai_camer_frsrc_disconnect() noexcept;
    [[nodiscard]] bool vqec_vision_ai_camer_frsrc_is_healthy() const noexcept;
    // Includes every frame delivered by this receiver, even from detached sessions.
    [[nodiscard]] unsigned vqec_vision_ai_camer_frsrc_get_outstanding() const noexcept;

private:
    std::shared_ptr<camera_session> session_;
    std::shared_ptr<camera_reader_count> reader_count_;
    std::uint64_t next_epoch_{1};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CAMERA_FRAME_SOURCE_HPP
